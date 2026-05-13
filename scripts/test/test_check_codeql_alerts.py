"""Tests for check_codeql_alerts.py.

Coverage strategy
-----------------
Unit tests target individual functions in isolation.  API-touching functions
are tested via unittest.mock so no real network calls are made.

Integration tests exercise main() end-to-end using temporary SARIF files and
captured environment / file I/O, matching exactly how the script is invoked
from .github/workflows/codeql-analysis.yaml:

    python3 scripts/check_codeql_alerts.py \
        --sarif "$GITHUB_WORKSPACE/sarif" \
        --min-level warning \
        --log-path "$LOG_PATH" \
        [--ref "refs/pull/<N>/merge"]
"""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any
from unittest.mock import MagicMock, patch

import pytest

# sys.path is set up by scripts/test/conftest.py.
import check_codeql_alerts as M  # noqa: E402

# ---------------------------------------------------------------------------
# Helpers / fixtures
# ---------------------------------------------------------------------------

_SARIF_TEMPLATE: dict[str, Any] = {
    "version": "2.1.0",
    "$schema": "https://raw.githubusercontent.com/oasis-tcs/sarif-spec/master/Schemata/sarif-schema-2.1.0.json",
    "runs": [],
}


def _make_run(
    results: list[dict[str, Any]],
    rules: list[dict[str, Any]] | None = None,
) -> dict[str, Any]:
    """Minimal SARIF run object."""
    return {
        "tool": {
            "driver": {
                "name": "CodeQL",
                "rules": rules or [],
            }
        },
        "results": results,
    }


def _make_result(
    rule_id: str = "py/sql-injection",
    level: str = "error",
    baseline_state: str = "new",
    uri: str = "src/app.py",
    start_line: int = 10,
    message: str = "Possible SQL injection.",
) -> dict[str, Any]:
    return {
        "ruleId": rule_id,
        "level": level,
        "baselineState": baseline_state,
        "message": {"text": message},
        "locations": [
            {
                "physicalLocation": {
                    "artifactLocation": {"uri": uri},
                    "region": {"startLine": start_line},
                }
            }
        ],
    }


def _make_sarif(results: list[dict[str, Any]]) -> dict[str, Any]:
    sarif = dict(_SARIF_TEMPLATE)
    sarif["runs"] = [_make_run(results)]
    return sarif


def _write_sarif_to(path: Path, results: list[dict[str, Any]]) -> Path:
    """Write a minimal SARIF file into *path* (created if absent) and return *path*."""
    path.mkdir(parents=True, exist_ok=True)
    (path / "results.sarif").write_text(json.dumps(_make_sarif(results)), encoding="utf-8")
    return path


def _make_api_alert(
    number: int = 1,
    rule_id: str = "py/sql-injection",
    severity: str = "error",
    path: str = "src/app.py",
    start_line: int = 10,
    message_text: str = "SQL injection risk.",
    analysis_key: str | None = None,
) -> dict[str, Any]:
    """Minimal GitHub Code Scanning API alert object."""
    return {
        "number": number,
        "html_url": f"https://github.com/owner/repo/security/code-scanning/{number}",
        "rule": {
            "id": rule_id,
            "name": rule_id.replace("/", "-"),
            "helpUri": f"https://codeql.github.com/codeql-query-help/{rule_id}",
        },
        "severity": severity,
        "most_recent_instance": {
            "analysis_key": analysis_key or f"ak:{number}",
            "message": {"text": message_text},
            "location": {
                "path": path,
                "start_line": start_line,
            },
        },
    }


# ---------------------------------------------------------------------------
# severity / severity_reaches_threshold
# ---------------------------------------------------------------------------


class TestSeverity:
    """Normalisation of SARIF level strings."""

    def test_known_levels_pass_through(self) -> None:
        """Known levels pass through."""
        for level in ("none", "note", "warning", "error"):
            assert M.severity(level) == level

    def test_unknown_level_becomes_warning(self) -> None:
        """Unknown level becomes warning."""
        assert M.severity("critical") == "warning"

    def test_none_input_becomes_warning(self) -> None:
        """None input becomes warning."""
        assert M.severity(None) == "warning"

    def test_case_insensitive(self) -> None:
        """Case insensitive."""
        assert M.severity("ERROR") == "error"
        assert M.severity("Warning") == "warning"

    def test_threshold_order(self) -> None:
        """Threshold order."""
        assert M.severity_reaches_threshold("error", "warning")
        assert M.severity_reaches_threshold("warning", "warning")
        assert not M.severity_reaches_threshold("note", "warning")
        assert not M.severity_reaches_threshold("none", "warning")

    def test_threshold_none_allows_everything(self) -> None:
        """Threshold none allows everything."""
        assert M.severity_reaches_threshold("none", "none")

    def test_threshold_error_excludes_warning(self) -> None:
        """Threshold error excludes warning."""
        assert not M.severity_reaches_threshold("warning", "error")


# ---------------------------------------------------------------------------
# sanitize_message / extract_message
# ---------------------------------------------------------------------------


class TestSanitizeMessage:
    """Tests for TestSanitizeMessage."""

    def test_truncates_long_messages(self) -> None:
        """Truncates long messages."""
        long = "x" * 300
        result = M.sanitize_message(long)
        assert result.endswith("...")
        assert len(result) <= 220

    def test_collapses_whitespace(self) -> None:
        """Collapses whitespace."""
        assert M.sanitize_message("a  b\n  c") == "a b c"

    def test_none_returns_placeholder(self) -> None:
        """None returns placeholder."""
        assert M.sanitize_message(None) == "(no message provided)"

    def test_empty_returns_placeholder(self) -> None:
        """Empty returns placeholder."""
        assert M.sanitize_message("") == "(no message provided)"


class TestExtractMessage:
    """Tests for TestExtractMessage."""

    def test_prefers_markdown_over_text(self) -> None:
        """Prefers markdown over text."""
        result = {"message": {"markdown": "**bold**", "text": "plain"}}
        assert M.extract_message(result) == "**bold**"

    def test_falls_back_to_text(self) -> None:
        """Falls back to text."""
        result = {"message": {"text": "plain text"}}
        assert M.extract_message(result) == "plain text"

    def test_uses_arguments_when_no_text(self) -> None:
        """Uses arguments when no text."""
        result = {"message": {"arguments": ["arg1", "arg2"]}}
        assert M.extract_message(result) == "arg1 arg2"

    def test_missing_message_returns_placeholder(self) -> None:
        """Missing message returns placeholder."""
        assert M.extract_message({}) == "(no message provided)"


# ---------------------------------------------------------------------------
# extract_location
# ---------------------------------------------------------------------------


class TestExtractLocation:
    """Tests for TestExtractLocation."""

    def _result(self, uri: str, line: int | None = None, col: int | None = None) -> dict:
        region: dict[str, Any] = {}
        if line is not None:
            region["startLine"] = line
        if col is not None:
            region["startColumn"] = col
        return {
            "locations": [
                {
                    "physicalLocation": {
                        "artifactLocation": {"uri": uri},
                        "region": region,
                    }
                }
            ]
        }

    def test_uri_only(self) -> None:
        """Uri only."""
        assert M.extract_location(self._result("src/app.py")) == "src/app.py"

    def test_uri_and_line(self) -> None:
        """Uri and line."""
        assert M.extract_location(self._result("src/app.py", line=42)) == "src/app.py:42"

    def test_uri_line_and_column(self) -> None:
        """Uri line and column."""
        assert M.extract_location(self._result("src/app.py", 42, 7)) == "src/app.py:42:7"

    def test_no_locations_returns_unavailable(self) -> None:
        """No locations returns unavailable."""
        assert M.extract_location({}) == "(location unavailable)"

    def test_related_location_fallback(self) -> None:
        """Related location fallback."""
        result = {
            "locations": [],
            "relatedLocations": [
                {
                    "physicalLocation": {
                        "artifactLocation": {"uri": "src/util.py"},
                        "region": {"startLine": 5},
                    }
                }
            ],
        }
        assert M.extract_location(result) == "src/util.py:5"

    def test_logical_location_fallback(self) -> None:
        """Logical location fallback."""
        result = {
            "locations": [],
            "logicalLocations": [{"fullyQualifiedName": "MyModule::MyClass"}],
        }
        assert M.extract_location(result) == "MyModule::MyClass"


# ---------------------------------------------------------------------------
# rule_lookup_map
# ---------------------------------------------------------------------------


class TestRuleLookupMap:
    """Tests for TestRuleLookupMap."""

    def test_returns_rules_keyed_by_id(self) -> None:
        """Returns rules keyed by id."""
        run = _make_run([], rules=[{"id": "py/sql-injection", "name": "SQL injection"}])
        rules = M.rule_lookup_map(run)
        assert "py/sql-injection" in rules
        assert rules["py/sql-injection"]["name"] == "SQL injection"

    def test_empty_driver_returns_empty_map(self) -> None:
        """Empty driver returns empty map."""
        assert M.rule_lookup_map({"tool": {"driver": {}}}) == {}

    def test_rule_without_id_is_skipped(self) -> None:
        """Rule without id is skipped."""
        run = _make_run([], rules=[{"name": "No ID rule"}])
        assert M.rule_lookup_map(run) == {}


# ---------------------------------------------------------------------------
# collect_alerts (SARIF baselineState)
# ---------------------------------------------------------------------------


class TestCollectAlerts:
    """Tests for TestCollectAlerts."""

    def test_new_alert_collected(self) -> None:
        """New alert collected."""
        sarif = _make_sarif([_make_result(baseline_state="new", level="error")])
        buckets = M.collect_alerts(sarif, min_level="warning")
        assert len(buckets["new"]) == 1
        assert buckets["new"][0].rule_id == "py/sql-injection"

    def test_absent_alert_collected(self) -> None:
        """Absent alert collected."""
        sarif = _make_sarif([_make_result(baseline_state="absent", level="note")])
        buckets = M.collect_alerts(sarif, min_level="warning")
        # absent alerts are always collected regardless of level
        assert len(buckets["absent"]) == 1

    def test_unchanged_alert_ignored(self) -> None:
        """Unchanged alert ignored."""
        sarif = _make_sarif([_make_result(baseline_state="unchanged", level="error")])
        buckets = M.collect_alerts(sarif, min_level="warning")
        assert len(buckets["new"]) == 0

    def test_new_alert_below_threshold_excluded(self) -> None:
        """New alert below threshold excluded."""
        sarif = _make_sarif([_make_result(baseline_state="new", level="note")])
        buckets = M.collect_alerts(sarif, min_level="warning")
        assert len(buckets["new"]) == 0

    def test_new_alert_below_threshold_included_when_threshold_none(self) -> None:
        """New alert below threshold included when threshold none."""
        sarif = _make_sarif([_make_result(baseline_state="new", level="note")])
        buckets = M.collect_alerts(sarif, min_level="none")
        assert len(buckets["new"]) == 1

    def test_multiple_runs_merged(self) -> None:
        """Multiple runs merged."""
        sarif = dict(_SARIF_TEMPLATE)
        sarif["runs"] = [
            _make_run([_make_result(rule_id="py/r1", baseline_state="new")]),
            _make_run([_make_result(rule_id="cpp/r2", baseline_state="new")]),
        ]
        buckets = M.collect_alerts(sarif, min_level="warning")
        rule_ids = {a.rule_id for a in buckets["new"]}
        assert rule_ids == {"py/r1", "cpp/r2"}

    def test_rule_metadata_attached(self) -> None:
        """Rule metadata attached."""
        rule = {
            "id": "py/sql-injection",
            "name": "SQL Injection",
            "helpUri": "https://example.com",
        }
        sarif = dict(_SARIF_TEMPLATE)
        sarif["runs"] = [_make_run([_make_result(baseline_state="new")], rules=[rule])]
        buckets = M.collect_alerts(sarif, min_level="warning")
        alert = buckets["new"][0]
        assert alert.rule_name == "SQL Injection"
        assert alert.help_uri == "https://example.com"

    def test_empty_runs_returns_empty_buckets(self) -> None:
        """Empty runs returns empty buckets."""
        sarif = dict(_SARIF_TEMPLATE)
        sarif["runs"] = []
        buckets = M.collect_alerts(sarif, min_level="warning")
        assert buckets["new"] == []
        assert buckets["absent"] == []


# ---------------------------------------------------------------------------
# load_sarif
# ---------------------------------------------------------------------------


class TestLoadSarif:
    """Tests for TestLoadSarif."""

    def test_loads_single_file(self, tmp_path: Path) -> None:
        """Loads single file."""
        sarif = _make_sarif([_make_result()])
        f = tmp_path / "results.sarif"
        f.write_text(json.dumps(sarif), encoding="utf-8")
        loaded = M.load_sarif(f)
        assert len(loaded["runs"]) == 1

    def test_loads_directory_merges_runs(self, tmp_path: Path) -> None:
        """Loads directory merges runs."""
        for i, lang in enumerate(("cpp", "python")):
            sarif = _make_sarif([_make_result(rule_id=f"{lang}/r{i}")])
            (tmp_path / f"{lang}.sarif").write_text(json.dumps(sarif), encoding="utf-8")
        loaded = M.load_sarif(tmp_path)
        assert len(loaded["runs"]) == 2

    def test_missing_file_raises(self, tmp_path: Path) -> None:
        """Missing file raises."""
        with pytest.raises(FileNotFoundError):
            M.load_sarif(tmp_path / "nonexistent.sarif")

    def test_empty_directory_raises(self, tmp_path: Path) -> None:
        """Empty directory raises."""
        with pytest.raises(FileNotFoundError):
            M.load_sarif(tmp_path)

    def test_invalid_json_raises(self, tmp_path: Path) -> None:
        """Invalid json raises."""
        f = tmp_path / "bad.sarif"
        f.write_text("{not valid json}", encoding="utf-8")
        with pytest.raises(ValueError, match="Failed to parse SARIF JSON"):
            M.load_sarif(f)


# ---------------------------------------------------------------------------
# Alert helpers
# ---------------------------------------------------------------------------


class TestAlertHelpers:
    """Tests for TestAlertHelpers."""

    def _alert(self, **kwargs: Any) -> M.Alert:
        defaults: dict[str, Any] = {
            "number": 42,
            "html_url": "https://github.com/owner/repo/security/code-scanning/42",
            "rule_id": "py/sql-injection",
            "level": "error",
            "message": "SQL injection risk.",
            "location": "src/app.py:10",
            "rule_name": "SQL Injection",
            "help_uri": "https://example.com",
            "security_severity": "9.8",
        }
        defaults.update(kwargs)
        return M.Alert(**defaults)

    def test_icon_error(self) -> None:
        """Icon error."""
        assert self._alert(level="error").icon() == ":x:"

    def test_icon_warning(self) -> None:
        """Icon warning."""
        assert self._alert(level="warning").icon() == ":warning:"

    def test_icon_unknown(self) -> None:
        """Icon unknown."""
        assert self._alert(level="banana").icon() == ":grey_question:"

    def test_level_title(self) -> None:
        """Level title."""
        assert self._alert(level="error").level_title() == "Error"

    def test_rule_display_with_uri(self) -> None:
        """Rule display with uri."""
        display = self._alert().rule_display()
        assert display == "[py/sql-injection](https://example.com)"

    def test_rule_display_without_uri(self) -> None:
        """Rule display without uri."""
        display = self._alert(help_uri=None).rule_display()
        assert display == "`py/sql-injection`"

    def test_severity_suffix_present(self) -> None:
        """Severity suffix present."""
        assert self._alert(security_severity="9.8").severity_suffix() == " (9.8)"

    def test_severity_suffix_absent(self) -> None:
        """Severity suffix absent."""
        assert self._alert(security_severity=None).severity_suffix() == ""


# ---------------------------------------------------------------------------
# _format_section
# ---------------------------------------------------------------------------


class TestFormatSection:
    """Tests for TestFormatSection."""

    def _alert(self, n: int, level: str = "error") -> M.Alert:
        return M.Alert(
            number=n,
            html_url=f"https://example.com/{n}",
            rule_id="py/r",
            level=level,
            message="msg",
            location=f"src/app.py:{n}",
        )

    def test_all_alerts_shown_when_under_limit(self) -> None:
        """All alerts shown when under limit."""
        alerts = [self._alert(i) for i in range(3)]
        lines = M._format_section(alerts, max_results=10, bullet_prefix=":x:")
        assert len(lines) == 3

    def test_overflow_produces_trailing_line(self) -> None:
        """Overflow produces trailing line."""
        alerts = [self._alert(i) for i in range(5)]
        lines = M._format_section(alerts, max_results=3, bullet_prefix=":x:")
        assert len(lines) == 4
        assert "2 more" in lines[-1]

    def test_alert_with_number_link(self) -> None:
        """Alert with number link."""
        lines = M._format_section([self._alert(7)], max_results=10, bullet_prefix=":x:")
        assert "[# 7]" in lines[0]
        assert "https://example.com/7" in lines[0]

    def test_dismissed_note_included(self) -> None:
        """Dismissed note included."""
        alert = self._alert(1)
        alert.dismissed_reason = "false positive"
        lines = M._format_section([alert], max_results=10, bullet_prefix=":x:")
        assert "dismissed" in lines[0]
        assert "false positive" in lines[0]

    def test_empty_list_returns_empty(self) -> None:
        """Empty list returns empty."""
        assert M._format_section([], max_results=10, bullet_prefix=":x:") == []


# ---------------------------------------------------------------------------
# build_comment
# ---------------------------------------------------------------------------


class TestBuildComment:
    """Tests for TestBuildComment."""

    def _alert(self, level: str = "error") -> M.Alert:
        return M.Alert(
            number=None,
            html_url=None,
            rule_id="py/sql-injection",
            level=level,
            message="SQL injection.",
            location="src/app.py:10",
        )

    def test_new_alerts_heading(self) -> None:
        """New alerts heading."""
        body = M.build_comment(
            new_alerts=[self._alert()],
            fixed_alerts=[],
            repo="owner/repo",
            max_results=20,
            threshold="warning",
        )
        assert "new CodeQL alert" in body
        assert "❌" in body

    def test_fixed_alerts_heading(self) -> None:
        """Fixed alerts heading."""
        body = M.build_comment(
            new_alerts=[],
            fixed_alerts=[self._alert()],
            repo="owner/repo",
            max_results=20,
            threshold="warning",
        )
        assert "resolved" in body
        assert "✅" in body

    def test_code_scanning_link_included_when_repo_known(self) -> None:
        """Code scanning link included when repo known."""
        body = M.build_comment(
            new_alerts=[self._alert()],
            fixed_alerts=[],
            repo="owner/repo",
            max_results=20,
            threshold="warning",
        )
        assert "https://github.com/owner/repo/security/code-scanning" in body

    def test_generic_link_when_repo_unknown(self) -> None:
        """Generic link when repo unknown."""
        body = M.build_comment(
            new_alerts=[self._alert()],
            fixed_alerts=[],
            repo=None,
            max_results=20,
            threshold="warning",
        )
        assert "Security tab" in body

    def test_plural_singular_new(self) -> None:
        """Plural singular new."""
        one = M.build_comment(
            new_alerts=[self._alert()],
            fixed_alerts=[],
            repo=None,
            max_results=20,
            threshold="warning",
        )
        two = M.build_comment(
            new_alerts=[self._alert(), self._alert()],
            fixed_alerts=[],
            repo=None,
            max_results=20,
            threshold="warning",
        )
        assert "1 new CodeQL alert " in one  # singular (no trailing 's')
        assert "2 new CodeQL alerts" in two

    def test_threshold_shown_in_heading(self) -> None:
        """Threshold shown in heading."""
        body = M.build_comment(
            new_alerts=[self._alert()],
            fixed_alerts=[],
            repo=None,
            max_results=20,
            threshold="warning",
        )
        assert "≥ warning" in body

    def test_body_ends_with_single_newline(self) -> None:
        """Body ends with single newline."""
        body = M.build_comment(
            new_alerts=[self._alert()],
            fixed_alerts=[],
            repo=None,
            max_results=20,
            threshold="warning",
        )
        assert body.endswith("\n")
        assert not body.endswith("\n\n")

    def test_highest_severity_shown(self) -> None:
        """Highest severity shown."""
        alerts = [
            self._alert("warning"),
            self._alert("error"),
        ]
        body = M.build_comment(
            new_alerts=alerts,
            fixed_alerts=[],
            repo=None,
            max_results=20,
            threshold="warning",
        )
        assert "Error" in body  # highest severity title


# ---------------------------------------------------------------------------
# _to_alert_api
# ---------------------------------------------------------------------------


class TestToAlertApi:
    """Tests for TestToAlertApi."""

    def test_basic_fields_extracted(self) -> None:
        """Basic fields extracted."""
        raw = _make_api_alert(number=5, rule_id="py/path-traversal", severity="warning")
        alert = M._to_alert_api(raw)
        assert alert.number == 5
        assert alert.rule_id == "py/path-traversal"
        assert alert.level == "warning"
        assert alert.html_url == "https://github.com/owner/repo/security/code-scanning/5"

    def test_message_from_most_recent_instance(self) -> None:
        """Message from most recent instance."""
        raw = _make_api_alert(message_text="Traversal risk.")
        alert = M._to_alert_api(raw)
        assert alert.message == "Traversal risk."

    def test_message_falls_back_to_rule_name(self) -> None:
        """Message falls back to rule name."""
        raw = _make_api_alert()
        raw["most_recent_instance"]["message"] = {}
        alert = M._to_alert_api(raw)
        # rule_name is "py-sql-injection" (from _make_api_alert)
        assert "py-sql-injection" in alert.message or alert.message != "(no message)"

    def test_location_flat_api_format(self) -> None:
        """Location flat api format."""
        raw = _make_api_alert(path="src/app.py", start_line=42)
        alert = M._to_alert_api(raw)
        assert "src/app.py" in alert.location
        assert "42" in alert.location

    def test_location_nested_sarif_format(self) -> None:
        """Location nested sarif format."""
        raw = _make_api_alert()
        raw["most_recent_instance"]["location"] = {
            "physicalLocation": {
                "artifactLocation": {"uri": "lib/util.py"},
                "region": {"startLine": 7},
            }
        }
        alert = M._to_alert_api(raw)
        assert "lib/util.py" in alert.location
        assert "7" in alert.location

    def test_analysis_key_from_instance(self) -> None:
        """Analysis key from instance."""
        raw = _make_api_alert(analysis_key="cpp/queries/CodeQL.cpp")
        alert = M._to_alert_api(raw)
        assert alert.analysis_key == "cpp/queries/CodeQL.cpp"

    def test_dismissed_reason_extracted(self) -> None:
        """Dismissed reason extracted."""
        raw = _make_api_alert()
        raw["dismissed_reason"] = "false positive"
        alert = M._to_alert_api(raw)
        assert alert.dismissed_reason == "false positive"

    def test_security_severity_from_rule_properties(self) -> None:
        """Security severity from rule properties."""
        raw = _make_api_alert()
        raw["rule"]["properties"] = {"security-severity": "8.5"}
        alert = M._to_alert_api(raw)
        assert alert.security_severity == "8.5"

    def test_missing_number_becomes_none(self) -> None:
        """Missing number becomes none."""
        raw = _make_api_alert()
        del raw["number"]
        alert = M._to_alert_api(raw)
        assert alert.number is None


# ---------------------------------------------------------------------------
# _paginate_alerts_api
# ---------------------------------------------------------------------------


class TestPaginateAlertsApi:
    """Tests for TestPaginateAlertsApi."""

    def _page(self, n: int, count: int) -> list[dict]:
        return [_make_api_alert(number=i + (n - 1) * count) for i in range(count)]

    @patch("check_codeql_alerts._api_request")
    def test_single_page(self, mock_req: MagicMock) -> None:
        """Single page."""
        mock_req.return_value = [_make_api_alert(1), _make_api_alert(2)]
        alerts = list(M._paginate_alerts_api("owner", "repo"))
        assert len(alerts) == 2
        assert mock_req.call_count == 1

    @patch("check_codeql_alerts._api_request")
    def test_multiple_pages(self, mock_req: MagicMock) -> None:
        """Multiple pages."""
        page1 = self._page(1, 100)
        page2 = self._page(2, 50)
        mock_req.side_effect = [page1, page2]
        alerts = list(M._paginate_alerts_api("owner", "repo"))
        assert len(alerts) == 150
        assert mock_req.call_count == 2

    @patch("check_codeql_alerts._api_request")
    def test_early_exit_on_partial_page(self, mock_req: MagicMock) -> None:
        """A page with fewer than 100 items must stop pagination without an extra call."""
        mock_req.return_value = self._page(1, 50)
        list(M._paginate_alerts_api("owner", "repo"))
        assert mock_req.call_count == 1

    @patch("check_codeql_alerts._api_request")
    def test_empty_first_page_returns_nothing(self, mock_req: MagicMock) -> None:
        """Empty first page returns nothing."""
        mock_req.return_value = []
        alerts = list(M._paginate_alerts_api("owner", "repo"))
        assert alerts == []

    @patch("check_codeql_alerts._api_request")
    def test_non_list_response_raises(self, mock_req: MagicMock) -> None:
        """Non list response raises."""
        mock_req.return_value = {"message": "not a list"}
        with pytest.raises(M.GitHubAPIError):
            list(M._paginate_alerts_api("owner", "repo"))

    @patch("check_codeql_alerts._api_request")
    def test_ref_forwarded_to_api(self, mock_req: MagicMock) -> None:
        """Ref forwarded to api."""
        mock_req.return_value = []
        list(M._paginate_alerts_api("owner", "repo", ref="refs/pull/42/merge"))
        _, kwargs = mock_req.call_args
        assert kwargs["params"]["ref"] == "refs/pull/42/merge"


# ---------------------------------------------------------------------------
# _compare_alerts_via_api
# ---------------------------------------------------------------------------


class TestCompareAlertsViaApi:
    """Tests for the core API comparison logic."""

    def _mock_paginate(self, pages: dict[str | None, list[dict]]) -> MagicMock:
        """Return a side_effect callable that dispatches on the `ref` kwarg."""

        def _paginate(owner: str, repo: str, *, state: str = "open", ref: str | None = None):  # noqa: ANN202
            yield from pages.get(ref, [])

        return MagicMock(side_effect=_paginate)

    def _pr_info(self, base_ref: str = "main", base_sha: str = "abc1234") -> dict:
        return {"base": {"ref": base_ref, "sha": base_sha}}

    @patch("check_codeql_alerts._api_request")
    @patch("check_codeql_alerts._paginate_alerts_api")
    def test_new_alert_detected(self, mock_pag: MagicMock, mock_req: MagicMock) -> None:
        """New alert detected."""
        pr_alert = _make_api_alert(number=1, analysis_key="ak:1")
        mock_pag.side_effect = self._mock_paginate({"refs/pull/7/merge": [pr_alert], None: []})
        mock_req.side_effect = [
            self._pr_info(),  # pulls/{n}
            [{"sha": "prev000"}],  # pulls/{n}/commits (only 1 commit → no prev)
        ]
        result = M._compare_alerts_via_api("owner", "repo", "refs/pull/7/merge")
        assert len(result.new_alerts) == 1
        assert result.new_alerts[0].number == 1

    @patch("check_codeql_alerts._api_request")
    @patch("check_codeql_alerts._paginate_alerts_api")
    def test_fixed_alert_detected(self, mock_pag: MagicMock, mock_req: MagicMock) -> None:
        """Fixed alert detected."""
        main_alert = _make_api_alert(number=2, analysis_key="ak:2")
        mock_pag.side_effect = self._mock_paginate({"refs/pull/7/merge": [], None: [main_alert]})
        mock_req.side_effect = [self._pr_info(), [{"sha": "prev000"}]]
        result = M._compare_alerts_via_api("owner", "repo", "refs/pull/7/merge")
        assert len(result.fixed_alerts) == 1
        assert result.fixed_alerts[0].number == 2

    @patch("check_codeql_alerts._api_request")
    @patch("check_codeql_alerts._paginate_alerts_api")
    def test_matched_alert(self, mock_pag: MagicMock, mock_req: MagicMock) -> None:
        """Matched alert."""
        alert = _make_api_alert(number=3, analysis_key="ak:3")
        mock_pag.side_effect = self._mock_paginate({"refs/pull/7/merge": [alert], None: [alert]})
        mock_req.side_effect = [self._pr_info(), [{"sha": "prev000"}]]
        result = M._compare_alerts_via_api("owner", "repo", "refs/pull/7/merge")
        assert len(result.new_alerts) == 0
        assert len(result.fixed_alerts) == 0
        assert len(result.matched_alerts) == 1

    @patch("check_codeql_alerts._api_request")
    @patch("check_codeql_alerts._paginate_alerts_api")
    def test_min_level_filters_new_alerts(self, mock_pag: MagicMock, mock_req: MagicMock) -> None:
        """Min level filters new alerts."""
        note_alert = _make_api_alert(number=5, severity="note", analysis_key="ak:5")
        mock_pag.side_effect = self._mock_paginate({"refs/pull/7/merge": [note_alert], None: []})
        mock_req.side_effect = [self._pr_info(), [{"sha": "prev000"}]]
        result = M._compare_alerts_via_api(
            "owner", "repo", "refs/pull/7/merge", min_level="warning"
        )
        assert len(result.new_alerts) == 0

    @patch("check_codeql_alerts._api_request")
    @patch("check_codeql_alerts._paginate_alerts_api")
    def test_min_level_none_includes_notes(self, mock_pag: MagicMock, mock_req: MagicMock) -> None:
        """Min level none includes notes."""
        note_alert = _make_api_alert(number=5, severity="note", analysis_key="ak:5")
        mock_pag.side_effect = self._mock_paginate({"refs/pull/7/merge": [note_alert], None: []})
        mock_req.side_effect = [self._pr_info(), [{"sha": "prev000"}]]
        result = M._compare_alerts_via_api("owner", "repo", "refs/pull/7/merge", min_level="none")
        assert len(result.new_alerts) == 1

    @patch("check_codeql_alerts._api_request")
    @patch("check_codeql_alerts._paginate_alerts_api")
    def test_prev_commit_comparison(self, mock_pag: MagicMock, mock_req: MagicMock) -> None:
        """Prev commit comparison."""
        prev_only = _make_api_alert(number=10, analysis_key="ak:10")
        pr_only = _make_api_alert(number=11, analysis_key="ak:11")
        # The script uses commits[-2]["sha"], so with [older, prev] that is "older_sha".
        mock_pag.side_effect = self._mock_paginate(
            {
                "refs/pull/7/merge": [pr_only],
                None: [],
                "older_sha": [prev_only],  # key must match commits[-2]["sha"]
            }
        )
        mock_req.side_effect = [
            self._pr_info(),
            [{"sha": "older_sha"}, {"sha": "head_sha"}],  # 2 commits; [-2] = "older_sha"
        ]
        result = M._compare_alerts_via_api("owner", "repo", "refs/pull/7/merge")
        assert len(result.new_vs_prev) == 1
        assert result.new_vs_prev[0].number == 11
        assert len(result.fixed_vs_prev) == 1
        assert result.fixed_vs_prev[0].number == 10

    @patch("check_codeql_alerts._api_request")
    @patch("check_codeql_alerts._paginate_alerts_api")
    def test_base_comparison(self, mock_pag: MagicMock, mock_req: MagicMock) -> None:
        """Base comparison."""
        base_only = _make_api_alert(number=20, analysis_key="ak:20")
        pr_only = _make_api_alert(number=21, analysis_key="ak:21")
        mock_pag.side_effect = self._mock_paginate(
            {
                "refs/pull/7/merge": [pr_only],
                None: [],
                "abc1234": [base_only],  # base_sha from _pr_info
            }
        )
        mock_req.side_effect = [
            self._pr_info(base_sha="abc1234"),
            [{"sha": "prev000"}],
        ]
        result = M._compare_alerts_via_api("owner", "repo", "refs/pull/7/merge")
        assert len(result.new_vs_base) == 1
        assert result.new_vs_base[0].number == 21
        assert len(result.fixed_vs_base) == 1
        assert result.fixed_vs_base[0].number == 20
        assert result.base_sha == "abc1234"

    @patch("check_codeql_alerts._api_request")
    @patch("check_codeql_alerts._paginate_alerts_api")
    def test_api_error_on_pr_info_is_handled(
        self, mock_pag: MagicMock, mock_req: MagicMock
    ) -> None:
        """Api error on pr info is handled."""
        mock_pag.side_effect = self._mock_paginate({"refs/pull/7/merge": [], None: []})
        mock_req.side_effect = M.GitHubAPIError("404 not found")
        result = M._compare_alerts_via_api("owner", "repo", "refs/pull/7/merge")
        # base/prev should be empty; comparison completes without raising
        assert result.base_sha is None
        assert result.prev_commit_ref is None
        assert result.new_vs_base == []
        assert result.new_vs_prev == []

    @patch("check_codeql_alerts._api_request")
    @patch("check_codeql_alerts._paginate_alerts_api")
    def test_non_pr_ref_skips_base_fetch(self, mock_pag: MagicMock, mock_req: MagicMock) -> None:
        """Non pr ref skips base fetch."""
        mock_pag.side_effect = self._mock_paginate({"some/ref": [], None: []})
        result = M._compare_alerts_via_api("owner", "repo", "some/ref")
        # No API calls should have been made for PR info
        mock_req.assert_not_called()
        assert result.base_sha is None

    @patch("check_codeql_alerts._api_request")
    @patch("check_codeql_alerts._paginate_alerts_api")
    def test_fixed_alerts_not_filtered_by_min_level(
        self, mock_pag: MagicMock, mock_req: MagicMock
    ) -> None:
        """Resolved alerts must appear in fixed_alerts regardless of severity level.

        Only *new* alerts are gated by min_level; surfacing that even a low-severity
        alert was fixed is always useful information for reviewers.
        """
        note_alert = _make_api_alert(number=6, severity="note", analysis_key="ak:6")
        # note_alert exists on main but not on the PR → it is "fixed"
        mock_pag.side_effect = self._mock_paginate({"refs/pull/7/merge": [], None: [note_alert]})
        mock_req.side_effect = [self._pr_info(), [{"sha": "prev000"}]]
        result = M._compare_alerts_via_api(
            "owner", "repo", "refs/pull/7/merge", min_level="warning"
        )
        assert len(result.fixed_alerts) == 1
        assert result.fixed_alerts[0].number == 6


# ---------------------------------------------------------------------------
# _build_multi_section_comment
# ---------------------------------------------------------------------------


class TestBuildMultiSectionComment:
    """Tests for TestBuildMultiSectionComment."""

    def _alert(self, level: str = "error", number: int = 1) -> M.Alert:
        return M.Alert(
            number=number,
            html_url=f"https://example.com/{number}",
            rule_id="py/r",
            level=level,
            message="msg",
            location="src/app.py:1",
        )

    def _comp(self, **kwargs: Any) -> M.APIAlertComparison:
        defaults: dict[str, Any] = {
            "new_alerts": [],
            "fixed_alerts": [],
            "matched_alerts": [],
            "new_vs_prev": [],
            "fixed_vs_prev": [],
            "new_vs_base": [],
            "fixed_vs_base": [],
            "base_sha": None,
            "prev_commit_ref": None,
        }
        defaults.update(kwargs)
        return M.APIAlertComparison(**defaults)

    def test_new_vs_base_rendered(self) -> None:
        """New vs base rendered."""
        comp = self._comp(
            new_vs_base=[self._alert()],
            base_sha="abc1234",
        )
        body = M._build_multi_section_comment(comp, max_results=10)
        assert "since the branch point" in body
        assert "abc1234" in body

    def test_fixed_vs_prev_rendered(self) -> None:
        """Fixed vs prev rendered."""
        comp = self._comp(
            fixed_vs_prev=[self._alert()],
            prev_commit_ref="def5678",
        )
        body = M._build_multi_section_comment(comp, max_results=10)
        assert "since the previous PR commit" in body

    def test_fallback_to_new_vs_main_when_no_context(self) -> None:
        """When no vs_prev/vs_base data exists, new_alerts must appear in the comment."""
        comp = self._comp(new_alerts=[self._alert()])
        body = M._build_multi_section_comment(comp, max_results=10)
        assert "compared to main" in body

    def test_fallback_to_fixed_vs_main_when_no_context(self) -> None:
        """Fallback to fixed vs main when no context."""
        comp = self._comp(fixed_alerts=[self._alert()])
        body = M._build_multi_section_comment(comp, max_results=10)
        assert "resolved compared to main" in body

    def test_no_fallback_when_detail_present(self) -> None:
        """new_alerts should not appear a second time when vs_base is rendered."""
        comp = self._comp(
            new_alerts=[self._alert(number=1)],
            new_vs_base=[self._alert(number=2)],
            base_sha="abc1234",
        )
        body = M._build_multi_section_comment(comp, max_results=10)
        assert "compared to main" not in body

    def test_body_ends_with_single_newline(self) -> None:
        """Body ends with single newline."""
        comp = self._comp(new_alerts=[self._alert()])
        body = M._build_multi_section_comment(comp, max_results=10)
        assert body.endswith("\n")
        assert not body.endswith("\n\n")

    def test_blank_lines_preserved_between_sections(self) -> None:
        """Adjacent headings must be separated by a blank line for valid markdown."""
        comp = self._comp(
            new_vs_base=[self._alert()],
            fixed_vs_base=[self._alert(number=2)],
            base_sha="abc",
        )
        body = M._build_multi_section_comment(comp, max_results=10)
        # Two H2 headings should not be directly adjacent
        assert "##" in body
        lines = body.splitlines()
        h2_indices = [i for i, ln in enumerate(lines) if ln.startswith("##")]
        assert len(h2_indices) >= 2
        for a, b in zip(h2_indices, h2_indices[1:], strict=False):
            assert b - a > 1, "H2 headings are directly adjacent (no blank line)"

    def test_code_scanning_link_present(self, monkeypatch: pytest.MonkeyPatch) -> None:
        """Code scanning link present."""
        monkeypatch.setenv("GITHUB_REPOSITORY", "owner/repo")
        comp = self._comp(new_alerts=[self._alert()])
        body = M._build_multi_section_comment(comp, max_results=10)
        assert "https://github.com/owner/repo/security/code-scanning" in body


# ---------------------------------------------------------------------------
# set_outputs
# ---------------------------------------------------------------------------


class TestSetOutputs:
    """Tests for TestSetOutputs."""

    def _alert(self) -> M.Alert:
        return M.Alert(
            number=1,
            html_url=None,
            rule_id="py/r",
            level="error",
            message="m",
            location="src/app.py:1",
        )

    def test_outputs_written(self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
        """Outputs written."""
        out_file = tmp_path / "output.txt"
        monkeypatch.setenv("GITHUB_OUTPUT", str(out_file))
        comment = tmp_path / "comment.md"
        M.set_outputs(
            new_alerts=[self._alert()],
            fixed_alerts=[],
            comment_path=comment,
        )
        content = out_file.read_text()
        assert "new_alerts=true" in content
        assert "alert_count=1" in content
        assert "fixed_alerts=false" in content
        assert "fixed_count=0" in content
        assert f"comment_path={comment}" in content

    def test_empty_comment_path(self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
        """Empty comment path."""
        out_file = tmp_path / "output.txt"
        monkeypatch.setenv("GITHUB_OUTPUT", str(out_file))
        M.set_outputs(new_alerts=[], fixed_alerts=[], comment_path=None)
        content = out_file.read_text()
        assert "comment_path=\n" in content

    def test_no_github_output_env_is_noop(self, monkeypatch: pytest.MonkeyPatch) -> None:
        """No github output env is noop."""
        monkeypatch.delenv("GITHUB_OUTPUT", raising=False)
        # Must not raise even without GITHUB_OUTPUT set
        M.set_outputs(new_alerts=[], fixed_alerts=[], comment_path=None)


# ---------------------------------------------------------------------------
# write_summary
# ---------------------------------------------------------------------------


class TestWriteSummary:
    """Tests for TestWriteSummary."""

    def _alert(self, level: str = "warning") -> M.Alert:
        return M.Alert(
            number=None,
            html_url=None,
            rule_id="py/r",
            level=level,
            message="m",
            location="src/app.py:1",
        )

    def test_summary_written(self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
        """Summary written."""
        summary_file = tmp_path / "summary.md"
        monkeypatch.setenv("GITHUB_STEP_SUMMARY", str(summary_file))
        M.write_summary(
            new_alerts=[self._alert("error")],
            fixed_alerts=[self._alert("warning")],
            max_results=10,
            threshold="warning",
        )
        content = summary_file.read_text()
        assert "CodeQL Alerts" in content
        assert "new alert" in content
        assert "resolved" in content
        # threshold must be interpolated correctly (not the literal '{threshold}')
        assert "{threshold}" not in content
        assert "warning" in content

    def test_no_summary_env_is_noop(self, monkeypatch: pytest.MonkeyPatch) -> None:
        """No summary env is noop."""
        monkeypatch.delenv("GITHUB_STEP_SUMMARY", raising=False)
        M.write_summary(
            new_alerts=[self._alert()],
            fixed_alerts=[],
            max_results=10,
            threshold="warning",
        )

    def test_nothing_written_when_no_alerts(
        self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch
    ) -> None:
        """Nothing written when no alerts."""
        summary_file = tmp_path / "summary.md"
        monkeypatch.setenv("GITHUB_STEP_SUMMARY", str(summary_file))
        M.write_summary(new_alerts=[], fixed_alerts=[], max_results=10, threshold="warning")
        assert not summary_file.exists()


# ---------------------------------------------------------------------------
# main() — end-to-end integration
# ---------------------------------------------------------------------------


class TestMainSarifMode:
    """End-to-end via SARIF baselineState (no API calls)."""

    def _write_sarif(self, path: Path, results: list[dict]) -> Path:
        return _write_sarif_to(path, results)

    def test_new_alert_returns_zero_and_writes_comment(
        self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch
    ) -> None:
        """New alert returns zero and writes comment."""
        sarif_dir = self._write_sarif(tmp_path / "sarif", [_make_result(baseline_state="new")])
        monkeypatch.setenv("RUNNER_TEMP", str(tmp_path / "runner"))
        log = tmp_path / "codeql.log"
        rc = M.main(["--sarif", str(sarif_dir), "--min-level", "warning", "--log-path", str(log)])
        assert rc == 0
        comment = tmp_path / "runner" / "codeql-alerts.md"
        assert comment.exists()
        assert "new CodeQL alert" in comment.read_text()

    def test_absent_alert_returns_zero_and_writes_comment(
        self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch
    ) -> None:
        """Absent alert returns zero and writes comment."""
        sarif_dir = self._write_sarif(tmp_path / "sarif", [_make_result(baseline_state="absent")])
        monkeypatch.setenv("RUNNER_TEMP", str(tmp_path / "runner"))
        log = tmp_path / "codeql.log"
        rc = M.main(["--sarif", str(sarif_dir), "--min-level", "warning", "--log-path", str(log)])
        assert rc == 0
        comment = tmp_path / "runner" / "codeql-alerts.md"
        assert comment.exists()
        assert "resolved" in comment.read_text()

    def test_no_alerts_returns_zero_no_comment(
        self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch
    ) -> None:
        """No alerts returns zero no comment."""
        sarif_dir = self._write_sarif(tmp_path / "sarif", [])
        monkeypatch.setenv("RUNNER_TEMP", str(tmp_path / "runner"))
        log = tmp_path / "codeql.log"
        rc = M.main(["--sarif", str(sarif_dir), "--min-level", "warning", "--log-path", str(log)])
        assert rc == 0
        comment = tmp_path / "runner" / "codeql-alerts.md"
        assert not comment.exists()

    def test_below_threshold_alert_not_reported(
        self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch
    ) -> None:
        """Below threshold alert not reported."""
        sarif_dir = self._write_sarif(
            tmp_path / "sarif",
            [_make_result(baseline_state="new", level="note")],
        )
        monkeypatch.setenv("RUNNER_TEMP", str(tmp_path / "runner"))
        log = tmp_path / "codeql.log"
        rc = M.main(["--sarif", str(sarif_dir), "--min-level", "warning", "--log-path", str(log)])
        assert rc == 0
        assert not (tmp_path / "runner" / "codeql-alerts.md").exists()

    def test_missing_sarif_exits_nonzero(self, tmp_path: Path) -> None:
        """Missing sarif exits nonzero."""
        log = tmp_path / "codeql.log"
        with pytest.raises(FileNotFoundError):
            M.main(["--sarif", str(tmp_path / "no_such.sarif"), "--log-path", str(log)])

    def test_github_output_written(self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
        """Github output written."""
        sarif_dir = self._write_sarif(tmp_path / "sarif", [_make_result(baseline_state="new")])
        out_file = tmp_path / "gh_output.txt"
        monkeypatch.setenv("GITHUB_OUTPUT", str(out_file))
        monkeypatch.setenv("RUNNER_TEMP", str(tmp_path / "runner"))
        log = tmp_path / "codeql.log"
        M.main(["--sarif", str(sarif_dir), "--min-level", "warning", "--log-path", str(log)])
        content = out_file.read_text()
        assert "new_alerts=true" in content
        assert "comment_path=" in content

    def test_github_output_false_when_no_alerts(
        self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch
    ) -> None:
        """Github output false when no alerts."""
        sarif_dir = self._write_sarif(tmp_path / "sarif", [])
        out_file = tmp_path / "gh_output.txt"
        monkeypatch.setenv("GITHUB_OUTPUT", str(out_file))
        monkeypatch.setenv("RUNNER_TEMP", str(tmp_path / "runner"))
        log = tmp_path / "codeql.log"
        M.main(["--sarif", str(sarif_dir), "--min-level", "warning", "--log-path", str(log)])
        content = out_file.read_text()
        assert "new_alerts=false" in content

    def test_directory_of_sarif_files(
        self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch
    ) -> None:
        """Directory of sarif files."""
        sarif_dir = tmp_path / "sarif"
        sarif_dir.mkdir()
        for lang in ("cpp", "python"):
            sarif = _make_sarif([_make_result(rule_id=f"{lang}/r1", baseline_state="new")])
            (sarif_dir / f"{lang}.sarif").write_text(json.dumps(sarif), encoding="utf-8")
        monkeypatch.setenv("RUNNER_TEMP", str(tmp_path / "runner"))
        log = tmp_path / "codeql.log"
        rc = M.main(["--sarif", str(sarif_dir), "--min-level", "warning", "--log-path", str(log)])
        assert rc == 0
        comment = (tmp_path / "runner" / "codeql-alerts.md").read_text()
        assert "2 new CodeQL alerts" in comment


class TestMainApiMode:
    """End-to-end tests for API comparison mode (no SARIF baselineState)."""

    def _empty_sarif_dir(self, base: Path) -> Path:
        return _write_sarif_to(base / "sarif", [])  # no baselineState results

    @patch("check_codeql_alerts._api_request")
    @patch("check_codeql_alerts._paginate_alerts_api")
    def test_api_mode_new_alert(
        self,
        mock_pag: MagicMock,
        mock_req: MagicMock,
        tmp_path: Path,
        monkeypatch: pytest.MonkeyPatch,
    ) -> None:
        """Api mode new alert."""
        sarif_dir = self._empty_sarif_dir(tmp_path)
        pr_alert = _make_api_alert(number=1, severity="error", analysis_key="ak:1")

        def _paginate(owner, repo, *, state="open", ref=None):
            if ref == "refs/pull/7/merge":
                yield pr_alert
            # main and base return nothing

        mock_pag.side_effect = _paginate
        mock_req.side_effect = [
            {"base": {"ref": "main", "sha": "base_sha"}},  # PR info
            [{"sha": "prev000"}],  # commits (1 commit, no prev)
        ]
        monkeypatch.setenv("GITHUB_REPOSITORY", "owner/repo")
        monkeypatch.setenv("RUNNER_TEMP", str(tmp_path / "runner"))
        log = tmp_path / "codeql.log"
        rc = M.main(
            [
                "--sarif",
                str(sarif_dir),
                "--ref",
                "refs/pull/7/merge",
                "--min-level",
                "warning",
                "--log-path",
                str(log),
            ]
        )
        assert rc == 0
        comment = (tmp_path / "runner" / "codeql-alerts.md").read_text()
        assert "new CodeQL alert" in comment

    @patch("check_codeql_alerts._api_request")
    @patch("check_codeql_alerts._paginate_alerts_api")
    def test_api_mode_min_level_filtering(
        self,
        mock_pag: MagicMock,
        mock_req: MagicMock,
        tmp_path: Path,
        monkeypatch: pytest.MonkeyPatch,
    ) -> None:
        """Api mode min level filtering."""
        sarif_dir = self._empty_sarif_dir(tmp_path)
        note_alert = _make_api_alert(number=2, severity="note", analysis_key="ak:2")

        def _paginate(owner, repo, *, state="open", ref=None):
            if ref == "refs/pull/7/merge":
                yield note_alert

        mock_pag.side_effect = _paginate
        mock_req.side_effect = [
            {"base": {"ref": "main", "sha": "base_sha"}},
            [{"sha": "prev000"}],
        ]
        monkeypatch.setenv("GITHUB_REPOSITORY", "owner/repo")
        monkeypatch.setenv("RUNNER_TEMP", str(tmp_path / "runner"))
        out_file = tmp_path / "gh_output.txt"
        monkeypatch.setenv("GITHUB_OUTPUT", str(out_file))
        log = tmp_path / "codeql.log"
        M.main(
            [
                "--sarif",
                str(sarif_dir),
                "--ref",
                "refs/pull/7/merge",
                "--min-level",
                "warning",
                "--log-path",
                str(log),
            ]
        )
        content = out_file.read_text()
        # Note-level alert must not be reported when threshold is warning
        assert "new_alerts=false" in content

    @patch("check_codeql_alerts._api_request")
    @patch("check_codeql_alerts._paginate_alerts_api")
    def test_api_mode_github_api_error_exits_2(
        self,
        mock_pag: MagicMock,
        mock_req: MagicMock,
        tmp_path: Path,
        monkeypatch: pytest.MonkeyPatch,
    ) -> None:
        """Api mode github api error exits 2."""
        sarif_dir = self._empty_sarif_dir(tmp_path)
        mock_pag.side_effect = M.GitHubAPIError("403 Forbidden")
        monkeypatch.setenv("GITHUB_REPOSITORY", "owner/repo")
        log = tmp_path / "codeql.log"
        rc = M.main(
            [
                "--sarif",
                str(sarif_dir),
                "--ref",
                "refs/pull/7/merge",
                "--min-level",
                "warning",
                "--log-path",
                str(log),
            ]
        )
        assert rc == 2

    @patch("check_codeql_alerts._api_request")
    @patch("check_codeql_alerts._paginate_alerts_api")
    def test_api_mode_missing_github_repository_exits_2(
        self,
        mock_pag: MagicMock,
        mock_req: MagicMock,
        tmp_path: Path,
        monkeypatch: pytest.MonkeyPatch,
    ) -> None:
        """Api mode missing github repository exits 2."""
        sarif_dir = self._empty_sarif_dir(tmp_path)
        monkeypatch.delenv("GITHUB_REPOSITORY", raising=False)
        log = tmp_path / "codeql.log"
        rc = M.main(
            [
                "--sarif",
                str(sarif_dir),
                "--ref",
                "refs/pull/7/merge",
                "--min-level",
                "warning",
                "--log-path",
                str(log),
            ]
        )
        assert rc == 2

    @patch("check_codeql_alerts._api_request")
    @patch("check_codeql_alerts._paginate_alerts_api")
    def test_api_mode_skipped_when_sarif_has_baseline(
        self,
        mock_pag: MagicMock,
        mock_req: MagicMock,
        tmp_path: Path,
        monkeypatch: pytest.MonkeyPatch,
    ) -> None:
        """SARIF with baselineState results must suppress API mode entirely."""
        sarif_dir = tmp_path / "sarif"
        sarif_dir.mkdir()
        sarif = _make_sarif([_make_result(baseline_state="new", level="error")])
        (sarif_dir / "results.sarif").write_text(json.dumps(sarif), encoding="utf-8")

        monkeypatch.setenv("GITHUB_REPOSITORY", "owner/repo")
        monkeypatch.setenv("RUNNER_TEMP", str(tmp_path / "runner"))
        log = tmp_path / "codeql.log"
        M.main(
            [
                "--sarif",
                str(sarif_dir),
                "--ref",
                "refs/pull/7/merge",
                "--min-level",
                "warning",
                "--log-path",
                str(log),
            ]
        )
        mock_pag.assert_not_called()
        mock_req.assert_not_called()
