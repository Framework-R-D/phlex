"""Tests for codeql_reset_dismissed_alerts.py.

Coverage strategy
-----------------
All GitHub REST API calls are intercepted with unittest.mock so no network
requests are made.

Unit tests cover:
- _token(): token lookup from environment variables
- _request(): request construction, success, and HTTPError handling
- _paginate_alerts(): single-page and multi-page iteration
- _to_alert(): parsing a raw alert dict into an Alert dataclass
- reopen_alert(): dry-run and live modes
- parse_args(): argument parsing

Integration tests exercise main() end-to-end matching the documented usage:

    GITHUB_TOKEN=ghp_... python3 scripts/codeql_reset_dismissed_alerts.py \
        --owner Framework-R-D --repo phlex [--dry-run]
"""

from __future__ import annotations

import json
import urllib.error
import urllib.request
from io import BytesIO
from typing import Any
from unittest.mock import MagicMock, patch

import pytest

# sys.path is set up by scripts/test/conftest.py.
import codeql_reset_dismissed_alerts as M  # noqa: E402

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _raw_alert(
    number: int = 1,
    html_url: str = "https://github.com/owner/repo/security/code-scanning/1",
    rule_id: str = "py/sql-injection",
    dismissed_reason: str | None = "won't fix",
) -> dict[str, Any]:
    """Return a minimal raw alert dict as returned by the GitHub API."""
    return {
        "number": number,
        "html_url": html_url,
        "rule": {"id": rule_id},
        "dismissed_reason": dismissed_reason,
    }


def _fake_urlopen(response_body: bytes, status: int = 200):
    """Return a context-manager mock that yields a response-like object."""
    mock_response = MagicMock()
    mock_response.read.return_value = response_body
    mock_response.__enter__ = lambda s: s
    mock_response.__exit__ = MagicMock(return_value=False)
    return mock_response


# ---------------------------------------------------------------------------
# _token
# ---------------------------------------------------------------------------


class TestToken:
    """Tests for M._token."""

    def test_github_token_env_used(self, monkeypatch: pytest.MonkeyPatch) -> None:
        """GITHUB_TOKEN is picked up from the environment."""
        monkeypatch.setenv("GITHUB_TOKEN", "ghp_abc123")
        monkeypatch.delenv("GH_TOKEN", raising=False)
        assert M._token() == "ghp_abc123"

    def test_gh_token_env_fallback(self, monkeypatch: pytest.MonkeyPatch) -> None:
        """GH_TOKEN is used when GITHUB_TOKEN is absent."""
        monkeypatch.delenv("GITHUB_TOKEN", raising=False)
        monkeypatch.setenv("GH_TOKEN", "ghp_fallback")
        assert M._token() == "ghp_fallback"

    def test_no_token_raises(self, monkeypatch: pytest.MonkeyPatch) -> None:
        """Missing both token env vars raises GitHubAPIError."""
        monkeypatch.delenv("GITHUB_TOKEN", raising=False)
        monkeypatch.delenv("GH_TOKEN", raising=False)
        with pytest.raises(M.GitHubAPIError, match="GITHUB_TOKEN"):
            M._token()


# ---------------------------------------------------------------------------
# _request
# ---------------------------------------------------------------------------


class TestRequest:
    """Tests for M._request."""

    def test_get_request_returns_parsed_json(self, monkeypatch: pytest.MonkeyPatch) -> None:
        """A successful GET returns the decoded JSON payload."""
        monkeypatch.setenv("GITHUB_TOKEN", "tok")
        body = json.dumps([{"number": 1}]).encode()
        with patch("urllib.request.urlopen", return_value=_fake_urlopen(body)):
            result = M._request("GET", "/repos/o/r/code-scanning/alerts")
        assert result == [{"number": 1}]

    def test_empty_response_returns_empty_dict(self, monkeypatch: pytest.MonkeyPatch) -> None:
        """A 204-style response (empty body) returns {}."""
        monkeypatch.setenv("GITHUB_TOKEN", "tok")
        with patch("urllib.request.urlopen", return_value=_fake_urlopen(b"")):
            result = M._request("PATCH", "/repos/o/r/code-scanning/alerts/1")
        assert result == {}

    def test_http_error_raises_github_api_error(self, monkeypatch: pytest.MonkeyPatch) -> None:
        """An HTTPError from urllib is converted to GitHubAPIError."""
        monkeypatch.setenv("GITHUB_TOKEN", "tok")
        error = urllib.error.HTTPError(
            url="https://api.github.com/test",
            code=403,
            msg="Forbidden",
            hdrs={},  # type: ignore[arg-type]
            fp=BytesIO(b'{"message":"Forbidden"}'),
        )
        with patch("urllib.request.urlopen", side_effect=error):
            with pytest.raises(M.GitHubAPIError, match="403"):
                M._request("GET", "/repos/o/r/code-scanning/alerts")

    def test_query_params_appended_to_url(self, monkeypatch: pytest.MonkeyPatch) -> None:
        """Query parameters are included in the request URL."""
        monkeypatch.setenv("GITHUB_TOKEN", "tok")
        captured_urls: list[str] = []

        def fake_urlopen(req):
            captured_urls.append(req.full_url)
            return _fake_urlopen(b"[]")

        params = {"state": "dismissed", "page": 1}
        with patch("urllib.request.urlopen", side_effect=fake_urlopen):
            M._request("GET", "/repos/o/r/code-scanning/alerts", params=params)

        assert "state=dismissed" in captured_urls[0]
        assert "page=1" in captured_urls[0]

    def test_post_sends_json_body(self, monkeypatch: pytest.MonkeyPatch) -> None:
        """A payload dict is serialised to JSON and sent as the request body."""
        monkeypatch.setenv("GITHUB_TOKEN", "tok")
        captured_data: list[bytes | None] = []

        def fake_urlopen(req):
            captured_data.append(req.data)
            return _fake_urlopen(b"{}")

        with patch("urllib.request.urlopen", side_effect=fake_urlopen):
            M._request("PATCH", "/repos/o/r/code-scanning/alerts/1", payload={"state": "open"})

        assert captured_data[0] == b'{"state": "open"}'


# ---------------------------------------------------------------------------
# _paginate_alerts
# ---------------------------------------------------------------------------


class TestPaginateAlerts:
    """Tests for M._paginate_alerts."""

    def test_single_page_yields_all_alerts(self, monkeypatch: pytest.MonkeyPatch) -> None:
        """All alerts on a single page are yielded and pagination stops."""
        monkeypatch.setenv("GITHUB_TOKEN", "tok")
        page1 = [_raw_alert(1), _raw_alert(2)]

        def fake_request(method, path, params=None, payload=None):
            if params and params.get("page") == 1:
                return page1
            return []

        with patch("codeql_reset_dismissed_alerts._request", side_effect=fake_request):
            results = list(M._paginate_alerts("owner", "repo"))

        assert len(results) == 2

    def test_multi_page_yields_all_alerts(self, monkeypatch: pytest.MonkeyPatch) -> None:
        """Alerts spread across two pages are all yielded."""
        monkeypatch.setenv("GITHUB_TOKEN", "tok")
        page1 = [_raw_alert(i) for i in range(1, 4)]
        page2 = [_raw_alert(i) for i in range(4, 6)]

        def fake_request(method, path, params=None, payload=None):
            page = params.get("page", 1) if params else 1
            if page == 1:
                return page1
            if page == 2:
                return page2
            return []

        with patch("codeql_reset_dismissed_alerts._request", side_effect=fake_request):
            results = list(M._paginate_alerts("owner", "repo"))

        assert len(results) == 5

    def test_unexpected_response_raises(self, monkeypatch: pytest.MonkeyPatch) -> None:
        """A non-list API response raises GitHubAPIError."""
        monkeypatch.setenv("GITHUB_TOKEN", "tok")
        with patch("codeql_reset_dismissed_alerts._request", return_value={"error": "oops"}):
            with pytest.raises(M.GitHubAPIError, match="expected a JSON list"):
                list(M._paginate_alerts("owner", "repo"))


# ---------------------------------------------------------------------------
# _to_alert
# ---------------------------------------------------------------------------


class TestToAlert:
    """Tests for M._to_alert."""

    def test_full_alert_parsed(self) -> None:
        """All fields of a raw alert dict are correctly mapped."""
        raw = _raw_alert(
            42, "https://github.com/o/r/security/code-scanning/42", "js/xss", "used in tests"
        )
        alert = M._to_alert(raw)
        assert alert.number == 42
        assert alert.html_url == "https://github.com/o/r/security/code-scanning/42"
        assert alert.rule_id == "js/xss"
        assert alert.dismissed_reason == "used in tests"

    def test_none_dismissed_reason(self) -> None:
        """dismissed_reason=None is stored as None, not 'None'."""
        raw = _raw_alert(dismissed_reason=None)
        alert = M._to_alert(raw)
        assert alert.dismissed_reason is None

    def test_missing_rule_key(self) -> None:
        """A raw dict without a 'rule' key produces an empty rule_id."""
        raw = {"number": 5, "html_url": "", "dismissed_reason": None}
        alert = M._to_alert(raw)
        assert alert.rule_id == ""


# ---------------------------------------------------------------------------
# reopen_alert
# ---------------------------------------------------------------------------


class TestReopenAlert:
    """Tests for M.reopen_alert."""

    def _make_alert(self, number: int = 1) -> M.Alert:
        """Return a minimal Alert object."""
        return M.Alert(number=number, html_url="", rule_id="py/xss", dismissed_reason=None)

    def test_dry_run_does_not_call_api(
        self, monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]
    ) -> None:
        """In dry-run mode, no API call is made."""
        monkeypatch.setenv("GITHUB_TOKEN", "tok")
        with patch("codeql_reset_dismissed_alerts._request") as mock_req:
            M.reopen_alert("owner", "repo", self._make_alert(), dry_run=True)
        mock_req.assert_not_called()
        assert "DRY RUN" in capsys.readouterr().out

    def test_live_mode_calls_patch(
        self, monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]
    ) -> None:
        """In live mode, PATCH /code-scanning/alerts/<number> is called."""
        monkeypatch.setenv("GITHUB_TOKEN", "tok")
        with patch("codeql_reset_dismissed_alerts._request") as mock_req:
            mock_req.return_value = {}
            M.reopen_alert("owner", "repo", self._make_alert(7), dry_run=False)
        mock_req.assert_called_once_with(
            "PATCH",
            "/repos/owner/repo/code-scanning/alerts/7",
            payload={"state": "open"},
        )
        assert "Reopened alert #7" in capsys.readouterr().out


# ---------------------------------------------------------------------------
# parse_args
# ---------------------------------------------------------------------------


class TestParseArgs:
    """Tests for M.parse_args."""

    def test_required_args_parsed(self) -> None:
        """--owner and --repo are required and are parsed correctly."""
        args = M.parse_args(["--owner", "Framework-R-D", "--repo", "phlex"])
        assert args.owner == "Framework-R-D"
        assert args.repo == "phlex"
        assert args.dry_run is False

    def test_dry_run_flag(self) -> None:
        """--dry-run sets dry_run=True."""
        args = M.parse_args(["--owner", "o", "--repo", "r", "--dry-run"])
        assert args.dry_run is True

    def test_missing_owner_exits(self) -> None:
        """Missing --owner causes SystemExit."""
        with pytest.raises(SystemExit):
            M.parse_args(["--repo", "phlex"])

    def test_missing_repo_exits(self) -> None:
        """Missing --repo causes SystemExit."""
        with pytest.raises(SystemExit):
            M.parse_args(["--owner", "org"])


# ---------------------------------------------------------------------------
# main()
# ---------------------------------------------------------------------------


class TestMain:
    """Integration tests for M.main()."""

    def test_no_alerts_returns_zero(
        self, monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]
    ) -> None:
        """No dismissed alerts prints a message and returns 0."""
        monkeypatch.setenv("GITHUB_TOKEN", "tok")
        with patch("codeql_reset_dismissed_alerts._paginate_alerts", return_value=iter([])):
            rc = M.main(["--owner", "org", "--repo", "repo"])
        assert rc == 0
        assert "No dismissed" in capsys.readouterr().out

    def test_alerts_listed_and_reopened(
        self, monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]
    ) -> None:
        """Dismissed alerts are listed and then reopened."""
        monkeypatch.setenv("GITHUB_TOKEN", "tok")
        raw_alerts = [_raw_alert(1, rule_id="py/xss"), _raw_alert(2, rule_id="js/xss")]
        with (
            patch("codeql_reset_dismissed_alerts._paginate_alerts", return_value=iter(raw_alerts)),
            patch("codeql_reset_dismissed_alerts._request", return_value={}) as mock_req,
        ):
            rc = M.main(["--owner", "org", "--repo", "repo"])
        assert rc == 0
        # Two PATCH calls (one per alert)
        assert mock_req.call_count == 2
        out = capsys.readouterr().out
        assert "2 dismissed alert" in out
        assert "All dismissed alerts reopened" in out

    def test_dry_run_no_api_calls(
        self, monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]
    ) -> None:
        """--dry-run lists alerts without making PATCH calls."""
        monkeypatch.setenv("GITHUB_TOKEN", "tok")
        raw_alerts = [_raw_alert(1)]
        with (
            patch("codeql_reset_dismissed_alerts._paginate_alerts", return_value=iter(raw_alerts)),
            patch("codeql_reset_dismissed_alerts._request") as mock_req,
        ):
            rc = M.main(["--owner", "org", "--repo", "repo", "--dry-run"])
        assert rc == 0
        mock_req.assert_not_called()
        out = capsys.readouterr().out
        assert "DRY RUN" in out
        assert "Dry run complete" in out

    def test_api_error_on_list_returns_one(
        self, monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]
    ) -> None:
        """A GitHubAPIError while listing alerts returns exit code 1."""
        monkeypatch.setenv("GITHUB_TOKEN", "tok")
        with patch(
            "codeql_reset_dismissed_alerts._paginate_alerts",
            side_effect=M.GitHubAPIError("network failure"),
        ):
            rc = M.main(["--owner", "org", "--repo", "repo"])
        assert rc == 1
        assert "Error" in capsys.readouterr().err

    def test_api_error_on_reopen_returns_one(
        self, monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]
    ) -> None:
        """A GitHubAPIError while reopening an alert returns exit code 1."""
        monkeypatch.setenv("GITHUB_TOKEN", "tok")
        raw_alerts = [_raw_alert(1)]
        with (
            patch("codeql_reset_dismissed_alerts._paginate_alerts", return_value=iter(raw_alerts)),
            patch(
                "codeql_reset_dismissed_alerts._request",
                side_effect=M.GitHubAPIError("patch failed"),
            ),
        ):
            rc = M.main(["--owner", "org", "--repo", "repo"])
        assert rc == 1
        assert "Failed to reopen" in capsys.readouterr().err

    def test_dismissed_reason_printed(
        self, monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]
    ) -> None:
        """The dismissal reason is included in the listing output."""
        monkeypatch.setenv("GITHUB_TOKEN", "tok")
        raw_alerts = [_raw_alert(1, dismissed_reason="used in tests")]
        with (
            patch("codeql_reset_dismissed_alerts._paginate_alerts", return_value=iter(raw_alerts)),
            patch("codeql_reset_dismissed_alerts._request", return_value={}),
        ):
            M.main(["--owner", "org", "--repo", "repo"])
        out = capsys.readouterr().out
        assert "used in tests" in out

    def test_no_token_returns_one(
        self, monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]
    ) -> None:
        """Missing token env var causes main to return 1."""
        monkeypatch.delenv("GITHUB_TOKEN", raising=False)
        monkeypatch.delenv("GH_TOKEN", raising=False)
        with patch(
            "codeql_reset_dismissed_alerts._paginate_alerts",
            side_effect=M.GitHubAPIError("Set GITHUB_TOKEN"),
        ):
            rc = M.main(["--owner", "org", "--repo", "repo"])
        assert rc == 1

    def test_alert_url_printed_in_listing(
        self, monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]
    ) -> None:
        """The html_url of each alert appears in the listing output."""
        monkeypatch.setenv("GITHUB_TOKEN", "tok")
        raw_alerts = [
            _raw_alert(
                1,
                html_url="https://github.com/org/repo/security/code-scanning/1",
                rule_id="py/xss",
            )
        ]
        with (
            patch("codeql_reset_dismissed_alerts._paginate_alerts", return_value=iter(raw_alerts)),
            patch("codeql_reset_dismissed_alerts._request", return_value={}),
        ):
            M.main(["--owner", "org", "--repo", "repo"])
        out = capsys.readouterr().out
        assert "https://github.com/org/repo/security/code-scanning/1" in out

    def test_dry_run_completion_message(
        self, monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]
    ) -> None:
        """Dry-run completion message says 'no changes made'."""
        monkeypatch.setenv("GITHUB_TOKEN", "tok")
        raw_alerts = [_raw_alert(1)]
        with (
            patch("codeql_reset_dismissed_alerts._paginate_alerts", return_value=iter(raw_alerts)),
            patch("codeql_reset_dismissed_alerts._request"),
        ):
            M.main(["--owner", "org", "--repo", "repo", "--dry-run"])
        out = capsys.readouterr().out
        assert "no changes made" in out

    def test_live_completion_message(
        self, monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]
    ) -> None:
        """Live-run completion message says 'All dismissed alerts reopened.'."""
        monkeypatch.setenv("GITHUB_TOKEN", "tok")
        raw_alerts = [_raw_alert(1)]
        with (
            patch("codeql_reset_dismissed_alerts._paginate_alerts", return_value=iter(raw_alerts)),
            patch("codeql_reset_dismissed_alerts._request", return_value={}),
        ):
            M.main(["--owner", "org", "--repo", "repo"])
        out = capsys.readouterr().out
        assert "All dismissed alerts reopened" in out
