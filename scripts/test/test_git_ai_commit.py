"""Tests for git-ai-commit helper functions.

git-ai-commit follows the git subcommand plugin convention: placing a script
named git-<cmd> on PATH makes it available as `git <cmd>`.  The hyphen and
the absence of a .py extension are intentional, so the module is loaded via
importlib.machinery.SourceFileLoader rather than a normal import statement.

Coverage focuses on the three functions that received error-handling changes:

- _kilo_auth_token  isinstance-based JSON structure validation
- _gh_cli_token     explicit return None for non-zero / missing gh exit
- _edit             FileNotFoundError and CalledProcessError surfaced as _Error
"""

from __future__ import annotations

import importlib.machinery
import importlib.util
import json
import subprocess
import sys
from collections.abc import Callable
from pathlib import Path
from unittest.mock import patch

import pytest

# ---------------------------------------------------------------------------
# Module loading
# ---------------------------------------------------------------------------

_SCRIPT = Path(__file__).parent.parent / "git-ai-commit"
_loader = importlib.machinery.SourceFileLoader("git_ai_commit", str(_SCRIPT))
_spec = importlib.util.spec_from_loader("git_ai_commit", _loader)
assert _spec is not None
_M = importlib.util.module_from_spec(_spec)
sys.modules.setdefault("git_ai_commit", _M)
_loader.exec_module(_M)

# Typed aliases so static analysis can reason about call sites instead of
# treating these as Unknown (the module was loaded dynamically via importlib).
# pylint: disable=protected-access
_kilo_auth_token: Callable[[], str | None] = _M._kilo_auth_token  # type: ignore[attr-defined]
_gh_cli_token: Callable[[], str | None] = _M._gh_cli_token  # type: ignore[attr-defined]
_edit: Callable[[str], str] = _M._edit  # type: ignore[attr-defined]
_Error: type[Exception] = _M._Error  # type: ignore[attr-defined]
# pylint: enable=protected-access


# ===========================================================================
# _kilo_auth_token
# ===========================================================================


class TestKiloAuthToken:
    """_kilo_auth_token validates the kilo credentials file before reading."""

    _PROVIDER = "fnal-litellm"

    def _write(self, path: Path, obj: object) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(obj), encoding="utf-8")

    def _setup(self, monkeypatch: pytest.MonkeyPatch, path: Path) -> None:
        monkeypatch.setattr(_M, "_KILO_AUTH_JSON", path)
        monkeypatch.setattr(_M, "_DEFAULT_KILO_PROVIDER", self._PROVIDER)

    def test_file_missing_returns_none(
        self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch
    ) -> None:
        """No auth.json → None without raising."""
        self._setup(monkeypatch, tmp_path / "absent.json")
        assert _kilo_auth_token() is None

    def test_invalid_json_returns_none(
        self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch
    ) -> None:
        """Malformed JSON content → None."""
        p = tmp_path / "auth.json"
        p.write_text("{not valid json", encoding="utf-8")
        self._setup(monkeypatch, p)
        assert _kilo_auth_token() is None

    def test_oserror_on_read_returns_none(
        self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch
    ) -> None:
        """OSError while reading the file → None."""
        p = tmp_path / "auth.json"
        p.write_text("{}", encoding="utf-8")
        self._setup(monkeypatch, p)
        with patch("pathlib.Path.read_text", side_effect=OSError("permission denied")):
            assert _kilo_auth_token() is None

    def test_top_level_not_dict_returns_none(
        self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch
    ) -> None:
        """JSON root is a list, not an object → None."""
        p = tmp_path / "auth.json"
        p.write_text("[1, 2, 3]", encoding="utf-8")
        self._setup(monkeypatch, p)
        assert _kilo_auth_token() is None

    def test_provider_absent_returns_none(
        self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch
    ) -> None:
        """Provider key missing from the JSON object → None."""
        p = tmp_path / "auth.json"
        self._write(p, {"other-provider": {"key": "abc"}})
        self._setup(monkeypatch, p)
        assert _kilo_auth_token() is None

    def test_provider_not_dict_returns_none(
        self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch
    ) -> None:
        """Provider entry is a bare string, not an object → None."""
        p = tmp_path / "auth.json"
        self._write(p, {self._PROVIDER: "just-a-string"})
        self._setup(monkeypatch, p)
        assert _kilo_auth_token() is None

    def test_key_not_string_returns_none(
        self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch
    ) -> None:
        """'key' field is an integer, not a string → None."""
        p = tmp_path / "auth.json"
        self._write(p, {self._PROVIDER: {"key": 12345}})
        self._setup(monkeypatch, p)
        assert _kilo_auth_token() is None

    def test_key_empty_returns_none(self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
        """Empty 'key' string → None."""
        p = tmp_path / "auth.json"
        self._write(p, {self._PROVIDER: {"key": ""}})
        self._setup(monkeypatch, p)
        assert _kilo_auth_token() is None

    def test_key_whitespace_only_returns_none(
        self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch
    ) -> None:
        """Whitespace-only 'key' string → None."""
        p = tmp_path / "auth.json"
        self._write(p, {self._PROVIDER: {"key": "   "}})
        self._setup(monkeypatch, p)
        assert _kilo_auth_token() is None

    def test_valid_key_stripped_and_returned(
        self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch
    ) -> None:
        """Valid key with surrounding whitespace is stripped and returned."""
        p = tmp_path / "auth.json"
        self._write(p, {self._PROVIDER: {"key": "  my-secret-token  "}})
        self._setup(monkeypatch, p)
        assert _kilo_auth_token() == "my-secret-token"

    def test_valid_key_no_whitespace(
        self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch
    ) -> None:
        """Valid key without surrounding whitespace is returned as-is."""
        p = tmp_path / "auth.json"
        self._write(p, {self._PROVIDER: {"key": "tok123"}})
        self._setup(monkeypatch, p)
        assert _kilo_auth_token() == "tok123"


# ===========================================================================
# _gh_cli_token
# ===========================================================================


class TestGhCliToken:
    """_gh_cli_token wraps `gh auth token` and returns None on any failure."""

    def test_gh_not_installed_returns_none(self) -> None:
        """FileNotFoundError from subprocess (gh absent) → None."""
        with patch("subprocess.run", side_effect=FileNotFoundError):
            assert _gh_cli_token() is None

    def test_gh_exits_nonzero_returns_none(self) -> None:
        """Ensure gh returns a non-zero exit code → explicit None."""
        mock_result = subprocess.CompletedProcess(
            ["gh", "auth", "token"], returncode=1, stdout="", stderr="error"
        )
        with patch("subprocess.run", return_value=mock_result):
            assert _gh_cli_token() is None

    def test_gh_exits_zero_empty_stdout_returns_none(self) -> None:
        """Ensure gh returns zero but produces only whitespace → None."""
        mock_result = subprocess.CompletedProcess(
            ["gh", "auth", "token"], returncode=0, stdout="   \n", stderr=""
        )
        with patch("subprocess.run", return_value=mock_result):
            assert _gh_cli_token() is None

    def test_gh_returns_token_stripped(self) -> None:
        """Ensure gh succeeds with a token — trailing newline is stripped."""
        mock_result = subprocess.CompletedProcess(
            ["gh", "auth", "token"], returncode=0, stdout="ghs_mytoken\n", stderr=""
        )
        with patch("subprocess.run", return_value=mock_result):
            assert _gh_cli_token() == "ghs_mytoken"


# ===========================================================================
# _edit
# ===========================================================================


class TestEdit:
    """_edit opens the staged message in $EDITOR and filters comment lines."""

    def _env(self, monkeypatch: pytest.MonkeyPatch, editor: str = "vi") -> None:
        monkeypatch.setenv("EDITOR", editor)
        monkeypatch.delenv("VISUAL", raising=False)

    def test_editor_not_found_raises_error(self, monkeypatch: pytest.MonkeyPatch) -> None:
        """Missing editor binary raises _Error with a descriptive message."""
        self._env(monkeypatch, "nonexistent-editor-xyz")
        with patch("subprocess.run", side_effect=FileNotFoundError):
            with pytest.raises(_Error, match="Editor not found"):
                _edit("subject\n\nbody")

    def test_editor_nonzero_exit_raises_error(self, monkeypatch: pytest.MonkeyPatch) -> None:
        """Non-zero editor exit raises _Error mentioning the status code."""
        self._env(monkeypatch)
        exc = subprocess.CalledProcessError(1, "vi")
        with patch("subprocess.run", side_effect=exc):
            with pytest.raises(_Error, match="status 1"):
                _edit("subject\n\nbody")

    def test_tempfile_cleaned_up_after_editor_error(self, monkeypatch: pytest.MonkeyPatch) -> None:
        """The temp file is removed even when the editor raises."""
        self._env(monkeypatch)
        recorded: list[Path] = []

        def _fail(cmd: list[str], **_kwargs: object) -> None:
            recorded.append(Path(cmd[-1]))
            raise subprocess.CalledProcessError(130, cmd)

        with patch("subprocess.run", side_effect=_fail):
            with pytest.raises(_Error):
                _edit("msg")

        assert recorded, "mock was never called"
        assert not recorded[0].exists(), "temp file was not cleaned up"

    def test_tempfile_cleaned_up_after_editor_not_found(
        self, monkeypatch: pytest.MonkeyPatch
    ) -> None:
        """The temp file is removed even when the editor binary is missing."""
        self._env(monkeypatch, "no-such-editor")
        recorded: list[Path] = []

        def _missing(cmd: list[str], **_kwargs: object) -> None:
            recorded.append(Path(cmd[-1]))
            raise FileNotFoundError

        with patch("subprocess.run", side_effect=_missing):
            with pytest.raises(_Error):
                _edit("msg")

        assert recorded, "mock was never called"
        assert not recorded[0].exists(), "temp file was not cleaned up"

    def test_comment_lines_stripped(self, monkeypatch: pytest.MonkeyPatch) -> None:
        """Lines starting with '#' are removed from the returned message."""
        self._env(monkeypatch)

        def _save(cmd: list[str], **_kwargs: object) -> subprocess.CompletedProcess[str]:
            Path(cmd[-1]).write_text("subject\n\nbody line\n# a comment\n", encoding="utf-8")
            return subprocess.CompletedProcess(cmd, 0)

        with patch("subprocess.run", side_effect=_save):
            result = _edit("original")

        assert "# a comment" not in result
        assert "subject" in result
        assert "body line" in result

    def test_empty_result_after_stripping(self, monkeypatch: pytest.MonkeyPatch) -> None:
        """A file consisting entirely of comment lines returns an empty string."""
        self._env(monkeypatch)

        def _all_comments(cmd: list[str], **_kwargs: object) -> subprocess.CompletedProcess[str]:
            Path(cmd[-1]).write_text("# line 1\n# line 2\n", encoding="utf-8")
            return subprocess.CompletedProcess(cmd, 0)

        with patch("subprocess.run", side_effect=_all_comments):
            result = _edit("original")

        assert result == ""

    def test_read_error_after_edit_raises_error(self, monkeypatch: pytest.MonkeyPatch) -> None:
        """OSError reading the temp file after a successful editor run raises _Error."""
        self._env(monkeypatch)

        def _succeed(cmd: list[str], **_kwargs: object) -> subprocess.CompletedProcess[str]:
            return subprocess.CompletedProcess(cmd, 0)

        with patch("subprocess.run", side_effect=_succeed):
            with patch("pathlib.Path.read_text", side_effect=OSError("disk error")):
                with pytest.raises(_Error, match="Could not read"):
                    _edit("original")
