"""Tests for git-ai-commit helper functions.

git-ai-commit follows the git subcommand plugin convention: placing a script
named git-<cmd> on PATH makes it available as `git <cmd>`.  The hyphen and
the absence of a .py extension are intentional, so the module is loaded via
importlib.machinery.SourceFileLoader rather than a normal import statement.

Coverage focuses on the three functions that received error-handling changes:

- _kilo_auth_token  isinstance-based JSON structure validation
- _gh_cli_token     explicit return None for non-zero / missing gh exit
- _edit             FileNotFoundError and CalledProcessError surfaced as _Error

Plus new tests for staged changes:

- _gh_oauth_token   returns GitHub OAuth token (not LiteLLM keys)
- _clean_message    strips model preamble and fenced code blocks
- _KILO_MODEL_PINNED_BY_ENV, _ESCALATION_* constants
- Empty staged change handling
"""

from __future__ import annotations

import importlib.machinery
import importlib.util
import io
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
_gh_oauth_token: Callable[[], str | None] = _M._gh_oauth_token  # type: ignore[attr-defined]
_clean_message: Callable[[str], str] = _M._clean_message  # type: ignore[attr-defined]
_edit: Callable[[str], str] = _M._edit  # type: ignore[attr-defined]
_chat_antigravity: Callable[[str, list[dict[str, str]]], str] = _M._chat_antigravity  # type: ignore[attr-defined]
_Error: type[Exception] = _M._Error  # type: ignore[attr-defined]
_APIError: type[Exception] = _M._APIError  # type: ignore[attr-defined]
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
# _gh_oauth_token
# ===========================================================================


class TestGhOAuthToken:
    """_gh_oauth_token returns a GitHub OAuth token.

    This deliberately ignores GIT_AI_COMMIT_TOKEN because that variable may
    hold a non-GitHub key (e.g. a LiteLLM API key). Used when a genuine
    GitHub OAuth token is required, such as the Copilot token exchange.
    """

    def test_github_token_env_used(self, monkeypatch: pytest.MonkeyPatch) -> None:
        """GITHUB_TOKEN env var is used when set."""
        for var in ("GIT_AI_COMMIT_TOKEN", "GITHUB_TOKEN", "GH_TOKEN"):
            monkeypatch.delenv(var, raising=False)
        monkeypatch.setenv("GITHUB_TOKEN", "ghs_oauth_token_123")
        with patch.multiple(
            _M,
            _gh_cli_token=lambda: None,
            _gh_config_token=lambda: None,
        ):
            assert _gh_oauth_token() == "ghs_oauth_token_123"

    def test_github_token_env_used_first(self, monkeypatch: pytest.MonkeyPatch) -> None:
        """GITHUB_TOKEN takes precedence when both are set."""
        for var in ("GIT_AI_COMMIT_TOKEN", "GITHUB_TOKEN", "GH_TOKEN"):
            monkeypatch.delenv(var, raising=False)
        monkeypatch.setenv("GH_TOKEN", "ghs_oauth_token_abc")
        monkeypatch.setenv("GITHUB_TOKEN", "ghs_oauth_token_xyz")
        with patch.multiple(
            _M,
            _gh_cli_token=lambda: None,
            _gh_config_token=lambda: None,
        ):
            # GITHUB_TOKEN is checked first in _gh_oauth_token
            assert _gh_oauth_token() == "ghs_oauth_token_xyz"

    def test_gh_cli_token_fallback(self, monkeypatch: pytest.MonkeyPatch) -> None:
        """Falls back to gh CLI if no env vars set."""
        for var in ("GIT_AI_COMMIT_TOKEN", "GITHUB_TOKEN", "GH_TOKEN"):
            monkeypatch.delenv(var, raising=False)
        mock_result = subprocess.CompletedProcess(
            ["gh", "auth", "token"], returncode=0, stdout="ghs_cli_token\n", stderr=""
        )
        with patch("subprocess.run", return_value=mock_result):
            with patch.multiple(
                _M,
                _gh_config_token=lambda: None,
            ):
                assert _gh_oauth_token() == "ghs_cli_token"

    def test_gh_config_token_fallback(self, monkeypatch: pytest.MonkeyPatch) -> None:
        """Falls back to ~/.config/gh/hosts.yml if gh CLI not available."""
        for var in ("GIT_AI_COMMIT_TOKEN", "GITHUB_TOKEN", "GH_TOKEN"):
            monkeypatch.delenv(var, raising=False)
        with patch("subprocess.run", side_effect=FileNotFoundError):
            with patch.multiple(
                _M,
                _gh_config_token=lambda: "ghs_hosts_file_token",
            ):
                assert _gh_oauth_token() == "ghs_hosts_file_token"

    def test_git_ai_commit_token_ignored(self, monkeypatch: pytest.MonkeyPatch) -> None:
        """GIT_AI_COMMIT_TOKEN is deliberately ignored for GitHub OAuth."""
        monkeypatch.setenv("GIT_AI_COMMIT_TOKEN", "not_a_github_oauth_token")
        monkeypatch.delenv("GITHUB_TOKEN", raising=False)
        monkeypatch.delenv("GH_TOKEN", raising=False)
        with patch("subprocess.run", side_effect=FileNotFoundError):
            with patch.multiple(
                _M,
                _gh_config_token=lambda: "ghs_oauth_token_from_hosts",
            ):
                assert _gh_oauth_token() == "ghs_oauth_token_from_hosts"

    def test_no_token_returns_none(self, monkeypatch: pytest.MonkeyPatch) -> None:
        """No token sources available → None."""
        for var in ("GIT_AI_COMMIT_TOKEN", "GITHUB_TOKEN", "GH_TOKEN"):
            monkeypatch.delenv(var, raising=False)
        with patch("subprocess.run", side_effect=FileNotFoundError):
            with patch.multiple(
                _M,
                _gh_config_token=lambda: None,
            ):
                assert _gh_oauth_token() is None


# ===========================================================================
# _clean_message
# ===========================================================================


class TestCleanMessage:
    """_clean_message strips model preamble and fenced code blocks."""

    def test_fenced_code_block_extracted(self) -> None:
        """Content inside a fenced code block is extracted; the language tag is dropped."""
        raw = (
            "Here is the commit message:\n\n```text\nfeat: add new feature\n\n"
            "- Implementation details\n```\n\nThat's all!"
        )
        expected = "feat: add new feature\n\n- Implementation details"
        assert _clean_message(raw) == expected

    def test_fenced_code_block_preserves_language_tag_content(self) -> None:
        """Language tag after ``` is discarded, content is preserved."""
        raw = """```json\n{\n  "message": "feat: add new feature"\n}\n```\n"""
        expected = '{\n  "message": "feat: add new feature"\n}'
        assert _clean_message(raw) == expected

    def test_leading_prose_preamble_stripped(self) -> None:
        """Lines ending with ':' followed by blank line are removed."""
        raw = "Here is the commit message:\n\nfeat: add new feature\n\nAdditional context here."
        expected = "feat: add new feature\n\nAdditional context here."
        assert _clean_message(raw) == expected

    def test_multiple_leading_preamble_lines_stripped(self) -> None:
        """Multiple prose preamble lines are all removed."""
        raw = (
            "I have analyzed the changes.\n\nHere is the commit message:\n\nfeat: add new feature"
        )
        expected = "feat: add new feature"
        assert _clean_message(raw) == expected

    def test_no_preamble_returns_cleaned(self) -> None:
        """Message without preamble is returned stripped."""
        raw = "feat: add new feature  \n  \n  Some context.\n  "
        expected = "feat: add new feature  \n  \n  Some context."
        assert _clean_message(raw) == expected

    def test_only_preamble_returns_empty(self) -> None:
        """Message consisting only of prose preamble becomes empty."""
        raw = "Here is the commit message:\n\n"
        assert _clean_message(raw) == ""

    def test_empty_string_returns_empty(self) -> None:
        """Empty string returns empty string."""
        assert _clean_message("") == ""

    def test_fenced_code_with_no_language(self) -> None:
        """Fence without language tag works correctly."""
        raw = "```\nfeat: add new feature\n```\n"
        expected = "feat: add new feature"
        assert _clean_message(raw) == expected

    def test_conventional_commit_subject_not_stripped(self) -> None:
        """A clean conventional-commit message is never treated as preamble."""
        raw = "fix: broken:\n\nActual body."
        assert _clean_message(raw) == "fix: broken:\n\nActual body."

    def test_conventional_commit_with_scope_not_stripped(self) -> None:
        """A scoped conventional-commit subject is not treated as preamble."""
        raw = "feat(api): add endpoint\n\nWired up the new handler."
        assert _clean_message(raw) == raw

    def test_breaking_change_subject_not_stripped(self) -> None:
        """A breaking-change (!) conventional-commit subject is not treated as preamble."""
        raw = "refactor!: rename all the things\n\nOld names were confusing."
        assert _clean_message(raw) == raw


# ===========================================================================
# _KILO_MODEL_PINNED_BY_ENV constant
# ===========================================================================


class TestKiloModelPinnedByEnv:
    """_KILO_MODEL_PINNED_BY_ENV is evaluated at import time."""

    def test_constant_exists(self) -> None:
        """_KILO_MODEL_PINNED_BY_ENV constant exists."""
        assert hasattr(_M, "_KILO_MODEL_PINNED_BY_ENV")
        assert isinstance(_M._KILO_MODEL_PINNED_BY_ENV, bool)


# ===========================================================================
# Empty staged change handling
# ===========================================================================

# _build_messages is used to verify the prompt wiring; pull it from the module.
# pylint: disable=protected-access
_build_messages = _M._build_messages  # type: ignore[attr-defined]
# pylint: enable=protected-access


class TestEmptyStagedChanges:
    """Tests for the empty-diff guard introduced in main().

    The guard has two branches:
    - no --amend: exit 0 immediately with a hint message.
    - --amend + no prior commit message: exit 1 with a descriptive message.

    We test the detection logic by examining what _build_messages receives, and
    also verify that the guard condition itself (`not diff.strip()`) is triggered
    by the values that `_staged_diff` would return for an empty index.
    """

    def test_main_exits_0_when_no_staged_changes(
        self,
        monkeypatch: pytest.MonkeyPatch,
        tmp_path: Path,
        capsys: pytest.CaptureFixture[str],
    ) -> None:
        """No staged diff and no --amend exits 0 with a hint message."""
        monkeypatch.setattr(sys, "argv", ["git-ai-commit"])
        monkeypatch.setattr(_M, "_git_root", lambda: tmp_path)
        monkeypatch.setattr(_M, "_staged_diff", lambda _amend=False: "")
        monkeypatch.setattr(_M, "_status", lambda: "")
        monkeypatch.setattr(_M, "_recent_log", lambda: "")
        monkeypatch.setattr(_M, "_get_instructions", lambda _root: "")
        monkeypatch.setattr(_M, "_token", lambda _backend: "tok")
        # Simulate a non-tty (pipe) stdin with no content so the stdin-read
        # branch is exercised without touching pytest's capture machinery.
        monkeypatch.setattr(sys, "stdin", io.StringIO(""))

        with pytest.raises(SystemExit) as exc:
            _M.main()

        assert exc.value.code == 0
        _, err = capsys.readouterr()
        assert "Nothing staged" in err

    def test_build_messages_includes_diff_when_nonempty(self, tmp_path: Path) -> None:
        """_build_messages embeds the diff verbatim in the user message when non-empty."""
        diff = "diff --git a/foo.py b/foo.py\n+new line\n"
        msgs = _build_messages(diff, "", "", "", tmp_path)
        user_content = msgs[1]["content"]
        assert diff in user_content

    def test_build_messages_with_last_msg_for_amend(self, tmp_path: Path) -> None:
        """When --amend provides a prior message, it appears in the prompt even with empty diff."""
        prior = "fix: previous commit subject"
        msgs = _build_messages("", "", "", "", tmp_path, last_msg=prior)
        user_content = msgs[1]["content"]
        assert prior in user_content


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


# ===========================================================================
# Antigravity Backend Tests
# ===========================================================================


class TestAntigravityBackend:
    """Tests for the antigravity/agy backend."""

    def test_resolve_backend_and_api(self) -> None:
        """_resolve_backend_and_api returns empty string for antigravity/agy."""
        backend, api = _M._resolve_backend_and_api("antigravity", "http://ignored")
        assert backend == "antigravity"
        assert api == ""

        backend, api = _M._resolve_backend_and_api("agy", "")
        assert backend == "agy"
        assert api == ""

    def test_default_model(self) -> None:
        """_default_model returns empty string for antigravity/agy."""
        assert _M._default_model("antigravity") == ""
        assert _M._default_model("agy") == ""

    def test_chat_antigravity_success_no_model(self) -> None:
        """_chat_antigravity runs agy successfully without model."""
        messages = [
            {"role": "system", "content": "system instruction"},
            {"role": "user", "content": "user query"},
        ]

        mock_completed = subprocess.CompletedProcess(
            args=["agy", "-p", "-"],
            returncode=0,
            stdout="Generated Commit Message\n",
            stderr="",
        )

        with patch("subprocess.run", return_value=mock_completed) as mock_run:
            res = _chat_antigravity("", messages)
            assert res == "Generated Commit Message"
            mock_run.assert_called_once()
            cmd, kwargs = mock_run.call_args
            assert cmd[0] == ["agy", "-p", "-"]
            assert kwargs["input"] == "System Instructions:\nsystem instruction\n\nuser query"

    def test_chat_antigravity_success_with_model(self) -> None:
        """_chat_antigravity runs agy successfully with model."""
        messages = [
            {"role": "user", "content": "user query"},
        ]

        mock_completed = subprocess.CompletedProcess(
            args=["agy", "-p", "-", "--model", "my-model"],
            returncode=0,
            stdout="Generated Commit Message\n",
            stderr="",
        )

        with patch("subprocess.run", return_value=mock_completed) as mock_run:
            res = _chat_antigravity("my-model", messages)
            assert res == "Generated Commit Message"
            mock_run.assert_called_once()
            cmd, kwargs = mock_run.call_args
            assert cmd[0] == ["agy", "-p", "-", "--model", "my-model"]
            assert kwargs["input"] == "user query"

    def test_chat_antigravity_file_not_found(self) -> None:
        """_chat_antigravity raises _Error if agy CLI is not found."""
        with patch("subprocess.run", side_effect=FileNotFoundError):
            with pytest.raises(_Error, match="not found in PATH"):
                _chat_antigravity("my-model", [])

    def test_chat_antigravity_nonzero_exit(self) -> None:
        """_chat_antigravity raises _APIError if agy CLI fails."""
        mock_completed = subprocess.CompletedProcess(
            args=["agy"],
            returncode=1,
            stdout="",
            stderr="invalid model",
        )
        with patch("subprocess.run", return_value=mock_completed):
            with pytest.raises(_APIError, match="failed \\(exit code 1\\)"):
                _chat_antigravity("my-model", [])

    def test_main_with_antigravity_backend(
        self,
        monkeypatch: pytest.MonkeyPatch,
        tmp_path: Path,
        capsys: pytest.CaptureFixture[str],
    ) -> None:
        """Main runs successfully with antigravity backend."""
        monkeypatch.setattr(sys, "argv", ["git-ai-commit", "--backend", "antigravity", "--yes"])
        monkeypatch.setattr(_M, "_git_root", lambda: tmp_path)
        monkeypatch.setattr(_M, "_staged_diff", lambda _amend=False: "some diff")
        monkeypatch.setattr(_M, "_status", lambda: "")
        monkeypatch.setattr(_M, "_recent_log", lambda: "")
        monkeypatch.setattr(_M, "_get_instructions", lambda _root: "")
        # Simulate a non-tty (pipe) stdin with no content.
        monkeypatch.setattr(sys, "stdin", io.StringIO(""))

        mock_completed = subprocess.CompletedProcess(
            args=["agy", "-p", "-"],
            returncode=0,
            stdout="feat: mock commit message\n",
            stderr="",
        )

        with patch(
            "subprocess.run",
            side_effect=[mock_completed, subprocess.CompletedProcess([], 0)],
        ) as mock_run:
            _M.main()

            # The first call should be to 'agy' to generate the commit message.
            # The second call should be 'git commit' with the message.
            assert mock_run.call_count == 2

            # Check the first call (agy)
            agy_cmd = mock_run.call_args_list[0][0][0]
            assert "agy" in agy_cmd

            # Check the second call (git commit)
            git_cmd = mock_run.call_args_list[1][0][0]
            assert git_cmd == ["git", "commit", "-m", "feat: mock commit message"]
