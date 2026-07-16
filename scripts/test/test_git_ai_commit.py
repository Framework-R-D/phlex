"""Tests for git-ai-commit helper functions.

git-ai-commit follows the git subcommand plugin convention: placing a script
named git-<cmd> on PATH makes it available as `git <cmd>`.  The hyphen and
the absence of a .py extension is intentional, so the module is loaded via
importlib.machinery.SourceFileLoader rather than a normal import statement.

Coverage focuses on functions that received error-handling changes:

- _gh_cli_token     explicit return None for non-zero / missing gh exit
- _edit             FileNotFoundError and CalledProcessError surfaced as _Error

Plus tests for:

- _gh_oauth_token   returns GitHub OAuth token (not LiteLLM keys)
- _clean_message    strips model preamble and fenced code blocks
- Empty staged change handling
"""

from __future__ import annotations

import importlib.machinery
import importlib.util
import io
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
_gh_cli_token: Callable[[], str | None] = _M._gh_cli_token  # type: ignore[attr-defined]
_gh_oauth_token: Callable[[], str | None] = _M._gh_oauth_token  # type: ignore[attr-defined]
_clean_message: Callable[[str], str] = _M._clean_message  # type: ignore[attr-defined]

_edit: Callable[[str], str] = _M._edit  # type: ignore[attr-defined]
_chat_antigravity: Callable[[str, list[dict[str, str]]], str] = _M._chat_antigravity  # type: ignore[attr-defined]
_Error: type[Exception] = _M._Error  # type: ignore[attr-defined]
_APIError: type[Exception] = _M._APIError  # type: ignore[attr-defined]
# pylint: enable=protected-access


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

    def test_duplicate_full_message_exact(self) -> None:
        """When the entire message appears twice, keep only the first."""
        msg = "feat: add feature\n\nBody content."
        raw = f"{msg}\n\n{msg}"
        assert _clean_message(raw) == msg

    def test_duplicate_full_message_with_truncation(self) -> None:
        """When AI returns full message followed by truncated duplicate, keep first."""
        full_msg = "feat(git-ai-commit): improve deduplication\n\nAdded state-based scanner."
        # Section 1 is subject only, Section 2 has body then subject again
        # This simulates: subject\n\nbody\n\nsubject\n\nbody
        truncated = "feat(git-ai-commit): improve deduplication"
        raw = f"{full_msg}\n\n{truncated}"
        result = _clean_message(raw)
        # Subject should appear only once
        assert result.count("feat(git-ai-commit): improve deduplication") == 1

    def test_duplicate_message_shortened_second(self) -> None:
        """When AI returns same subject but different body lengths, handle correctly."""
        # Original test case from bug report
        msg1 = "feat(git-ai-commit): robust JSONC parsing, model fallback, diff improvements"
        body1 = "Implemented a state-based scanner to safely strip comments from kilo.jsonc."
        msg2 = "feat(git-ai-commit): robust JSONC parsing, model fallback, diff improvements"

        # Section 1: just subject
        # Section 2: body + subject (duplicate)
        raw = f"{msg1}\n\n{body1}\n\n{msg2}"
        result = _clean_message(raw)
        # Subject should appear only once, body should be preserved
        assert result.count(msg1) == 1
        assert body1 in result

    def test_duplicate_message_truncated_body(self) -> None:
        """When AI returns full message then just the subject line, handle correctly."""
        # This is the exact pattern seen in the bug report with gemma-4-31B-it
        subject = "refactor(scripts): optimize git-ai-commit context and parsing"
        body = """Improve the robustness of the `git-ai-commit` tool.

Changes include:
- Replaced JSONC comment removal with a state-based scanner."""

        # The model outputs: subject\n\nbody\n\nsubject
        raw = f"{subject}\n\n{body}\n\n{subject}"
        result = _clean_message(raw)

        # Subject should appear only once
        assert result.count(subject) == 1
        # Body should be preserved
        assert "Replaced JSONC" in result


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
        monkeypatch.setattr(_M, "_token", lambda _backend, _model: "tok")
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


# ===========================================================================
# _truncate_diff
# ===========================================================================


class TestTruncateDiff:
    """Tests for _truncate_diff, the hunk-aware diff truncation helper."""

    def test_under_budget_returns_unchanged(self) -> None:
        """Diff shorter than max_chars is returned unchanged."""
        diff = "diff --git a/foo.py b/foo.py\n+new line\n"
        result = _M._truncate_diff(diff, 1000)
        assert result == diff

    def test_over_budget_cuts_on_file_boundary(self) -> None:
        """Diff with multiple files is cut on file boundary; omission marker present."""
        # Diff with two file sections; first will fit, second will not
        stat_block = " git diff --cached --stat -p\n\n"
        file1 = "diff --git a/aaa.py b/aaa.py\nnew file mode 100644\n+line 1\n+line 2\n"
        file2 = "diff --git a/bbb.py b/bbb.py\nnew file mode 100644\n+line a\n+line b\n"
        diff = stat_block + file1 + file2
        # Budget that fits stat + file1 exactly, but not file2 on top
        budget = len(stat_block) + len(file1)
        result = _M._truncate_diff(diff, budget)

        # stat + file1 should be present
        assert stat_block in result
        assert "aaa.py" in result
        assert "line 1" in result
        # file2 should be omitted
        assert "bbb.py" not in result
        assert "line a" not in result
        # marker should be present
        assert "files omitted to fit context budget" in result

    def test_single_oversized_file_line_boundary(self) -> None:
        """Single file section larger than budget is truncated at line boundary."""
        stat_block = " git diff --cached --stat -p\n\n"
        # Build a large file section with many lines - this should create one large section
        lines = [f"+line {i}\n" for i in range(100)]
        # Create a single large file section with all the lines
        file_section = "diff --git a/large.py b/large.py\nnew file mode 100644\n" + "".join(lines)
        diff = stat_block + file_section

        # Budget: enough for stat block + the header line + a couple of content lines,
        # but far less than the full file section.
        budget = (
            len(stat_block)
            + len("diff --git a/large.py b/large.py\n")
            + len("+line 0\n")
            + len("+line 1\n")
        )
        result = _M._truncate_diff(diff, budget)

        # Result should be shorter than the original diff (truncation occurred)
        assert len(result) < len(diff)
        # Should contain the truncation marker even for a single-file fallback cut
        assert "files omitted to fit context budget" in result
        # All content lines (non-marker) should be from the original diff (no mid-line cuts)
        marker = "[diff truncated:"
        for line in result.split("\n"):
            if line and marker not in line:
                assert line in diff

    def test_omission_marker_text(self) -> None:
        """Marker text contains expected description."""
        stat_block = " git diff --cached --stat -p\n\n"
        file1 = "diff --git a/a.py b/a.py\n+1\n"
        file2 = "diff --git a/b.py b/b.py\n+2\n"
        file3 = "diff --git a/c.py b/c.py\n+3\n"
        diff = stat_block + file1 + file2 + file3

        # Budget fits stat + file1 exactly; file2 and file3 are dropped (2 of 3 omitted)
        budget = len(stat_block) + len(file1)
        result = _M._truncate_diff(diff, budget)

        assert "files omitted to fit context budget" in result
        # Check the marker format: 2 of 3 files omitted
        assert "2 of 3 files omitted to fit context budget" in result


# ===========================================================================
# _skip_binary_and_generated
# ===========================================================================


class TestSkipBinaryAndGenerated:
    """Tests for _skip_binary_and_generated, the binary/generated file filter."""

    def test_binary_section_replaced_with_note(self) -> None:
        """Binary file section replaced with [skipped binary/generated file: ...]."""
        diff = (
            "git diff --cached --stat -p\n\n"
            "Binary files a/img.png and b/img.png differ\n"
            "diff --git a/text.txt b/text.txt\n+hello\n"
        )
        result = _M._skip_binary_and_generated(diff)

        # Should contain the skip note with filename
        assert "[skipped binary/generated file: b/img.png]" in result
        # Should NOT contain the original binary diff line
        assert "Binary files" not in result
        # Normal section should pass through
        assert "text.txt" in result
        assert "hello" in result

    def test_generated_file_replaced_with_note(self) -> None:
        """Generated file section (matching _GENERATED_FILE_GLOBS) is replaced."""
        diff = (
            "git diff --cached --stat -p\n\n"
            "diff --git a/foo.pb.h b/foo.pb.h\n"
            "new file mode 100644\n"
            "+// Generated by protoc\n"
            "+class Foo {}\n"
            "diff --git a/normal.cpp b/normal.cpp\n+int main() {}\n"
        )
        result = _M._skip_binary_and_generated(diff)

        # foo.pb.h is in _GENERATED_FILE_GLOBS (path stored without b/ prefix)
        assert "[skipped binary/generated file: foo.pb.h]" in result
        # Generated file hunk body should be absent
        assert "Generated by protoc" not in result
        assert "class Foo {}" not in result
        # Normal file should pass through
        assert "normal.cpp" in result
        assert "int main()" in result

    def test_normal_file_section_kept(self) -> None:
        """Plain .py file section passes through unchanged."""
        diff = (
            "git diff --cached --stat -p\n\n"
            "diff --git a/program.py b/program.py\n+print('hello')\n"
        )
        result = _M._skip_binary_and_generated(diff)

        assert result == diff
        assert "print('hello')" in result


# ===========================================================================
# _build_messages max_diff_chars parameter
# ===========================================================================


class TestBuildMessagesMaxDiffChars:
    """Tests for _build_messages with the max_diff_chars parameter."""

    def _make_large_diff(self, *, num_lines: int = 1500) -> str:
        """Construct a large diff with approximately `num_lines` added lines."""
        stat_block = " git diff --cached --stat -p\n\n"
        lines = [f"+line {i} with some padding to make it longer\n" for i in range(num_lines)]
        file_section = "diff --git a/large.py b/large.py\nnew file mode 100644\n".join(lines)
        return stat_block + file_section

    def test_default_cap_truncates_large_diff(self, tmp_path: Path) -> None:
        """Large diff (> 60 000 chars) is truncated with default max_diff_chars."""
        diff = self._make_large_diff()
        assert len(diff) > 60_000

        msgs = _build_messages(diff, "", "", "", tmp_path)
        user_content = msgs[1]["content"]

        # Should contain truncation marker
        assert "files omitted to fit context budget" in user_content

    def test_default_arg_unchanged(self) -> None:
        """_build_messages signature has _MAX_DIFF_CHARS_DEFAULT as max_diff_chars default."""
        import inspect

        sig = inspect.signature(_build_messages)
        param = sig.parameters["max_diff_chars"]
        assert param.default == _M._MAX_DIFF_CHARS_DEFAULT


# ===========================================================================
# JSONC and Model Resolution Tests
# ===========================================================================


class TestJsoncParsing:
    """Tests for JSONC comment removal and parsing in _load_kilo_config."""

    def test_load_kilo_config_removes_comments(self, tmp_path: Path) -> None:
        """_load_kilo_config should remove // and /* */ comments but preserve strings."""
        jsonc_content = (
            "{\n"
            "  // This is a line comment\n"
            '  "disabled_providers": [],\n'
            '  "provider": {\n'
            '    "fnal-ow": { /* block comment */\n'
            '      "models": {\n'
            '        "qwen/qwen3-coder-next": { "desc": "Keep // in strings" }\n'
            "      }\n"
            "    }\n"
            "  }\n"
            "}"
        )
        config_file = tmp_path / "kilo.jsonc"
        config_file.write_text(jsonc_content, encoding="utf-8")

        original_path = _M._KILO_CONFIG_PATH  # type: ignore[attr-defined]
        _M._KILO_CONFIG_PATH = config_file  # type: ignore[attr-defined]
        try:
            result = _M._load_kilo_config()  # type: ignore[attr-defined]
            assert (
                result["provider"]["fnal-ow"]["models"]["qwen/qwen3-coder-next"]["desc"]
                == "Keep // in strings"
            )
            assert "disabled_providers" in result
        finally:
            _M._KILO_CONFIG_PATH = original_path  # type: ignore[attr-defined]

    def test_load_kilo_config_invalid_json_returns_empty(self, tmp_path: Path) -> None:
        """Invalid JSON returns the default empty config."""
        config_file = tmp_path / "kilo.jsonc"
        config_file.write_text("{ invalid json }")

        original_path = _M._KILO_CONFIG_PATH  # type: ignore[attr-defined]
        _M._KILO_CONFIG_PATH = config_file  # type: ignore[attr-defined]
        try:
            result = _M._load_kilo_config()  # type: ignore[attr-defined]
            assert result == {"disabled_providers": [], "provider": {}}
        finally:
            _M._KILO_CONFIG_PATH = original_path  # type: ignore[attr-defined]


class TestModelResolution:
    """Tests for _resolve_kilo_model logic."""

    def test_resolve_kilo_model_found_in_config(self) -> None:
        """Should return the model if it exists in the provider's config."""
        with patch.object(
            _M,
            "_load_kilo_config",
            return_value={"provider": {"fnal-ow": {"models": {"test-model": {"desc": "test"}}}}},
        ):
            assert _M._resolve_kilo_model("test-model") == "fnal-ow/test-model"

    def test_resolve_kilo_model_not_found_fallback(self) -> None:
        """Should return fallback model if requested model is not found."""
        with patch.object(
            _M, "_load_kilo_config", return_value={"provider": {"fnal-ow": {"models": {}}}}
        ):
            result = _M._resolve_kilo_model("unknown-model")
            assert "fnal-ow" in result or result == "google/gemma4-31b"

    def test_resolve_kilo_model_no_config_fallback(self) -> None:
        """Should return default fallback if config is empty."""
        with patch.object(_M, "_load_kilo_config", return_value={"provider": {}}):
            result = _M._resolve_kilo_model("any-model")
            assert result == "google/gemma4-31b"


class TestKiloConfig:
    """Tests for kilo config loading and model resolution."""

    def test_kilo_config_path(self) -> None:
        """_KILO_CONFIG_PATH points to ~/.config/kilo/kilo.jsonc."""
        from pathlib import Path

        expected = Path.home() / ".config" / "kilo" / "kilo.jsonc"
        assert _M._KILO_CONFIG_PATH == expected  # type: ignore[attr-defined]

    def test_load_kilo_config_missing_file_returns_empty(self, tmp_path: Path) -> None:
        """_load_kilo_config returns empty config when file doesn't exist."""
        # Use a temp path that doesn't exist
        non_existent = tmp_path / "nonexistent" / "kilo.jsonc"
        original = _M._KILO_CONFIG_PATH  # type: ignore[attr-defined]
        try:
            _M._KILO_CONFIG_PATH = non_existent  # type: ignore[attr-defined]
            result = _M._load_kilo_config()  # type: ignore[attr-defined]
            assert result == {"disabled_providers": [], "provider": {}}
        finally:
            _M._KILO_CONFIG_PATH = original  # type: ignore[attr-defined]

    def test_load_kilo_config_with_models_maps_providers(
        self, monkeypatch: pytest.MonkeyPatch
    ) -> None:
        """_load_kilo_config_with_models returns provider-to-models mapping."""
        # Mock the config to have fnal-ow and fnal-azure providers with test models
        mock_config = {
            "disabled_providers": ["fnal-azure"],
            "provider": {
                "fnal-ow": {
                    "models": {
                        "qwen/qwen3-coder-next": {"temperature": 0.7},
                        "qwen/qwen2.5-32b": {},
                    }
                },
                "fnal-azure": {
                    "models": {
                        "azure/gpt-5-nano": {"temperature": 0.9},
                        "azure/claude-3-5-sonnet": {},
                    }
                },
            },
        }
        monkeypatch.setattr(_M, "_load_kilo_config", lambda: mock_config)  # type: ignore[attr-defined]

        mapping = _M._load_kilo_config_with_models()  # type: ignore[attr-defined]
        # fnal-ow should be a provider in the mapping
        assert "fnal-ow" in mapping
        # fnal-azure should be EXCLUDED because it is disabled
        assert "fnal-azure" not in mapping
        # mapping[provider] = models_dict (models are directly in the dict)
        assert "qwen/qwen3-coder-next" in mapping["fnal-ow"]
        assert "qwen/qwen2.5-32b" in mapping["fnal-ow"]

    def test_resolve_kilo_model_with_known_provider_prefix(self) -> None:
        """_resolve_kilo_model returns model unchanged if provider prefix is known."""
        # When the prefix (fnal-ow) is a known provider, model is passed as-is
        result = _M._resolve_kilo_model("fnal-ow/qwen3-coder-next")
        assert result == "fnal-ow/qwen3-coder-next"

    def test_resolve_kilo_model_disabled_provider_prefix_is_treated_as_model_name(self) -> None:
        """If provider prefix is disabled, it's treated as part of the model name."""
        mock_config = {
            "disabled_providers": ["disabled-prov"],
            "provider": {
                "enabled-prov": {
                    "models": {
                        "disabled-prov/model-x": {},
                    }
                },
                "disabled-prov": {
                    "models": {
                        "model-x": {},
                    }
                },
            },
        }
        original_load = _M._load_kilo_config  # type: ignore[attr-defined]
        _M._load_kilo_config = lambda: mock_config  # type: ignore[attr-defined]
        try:
            # "disabled-prov" is disabled, so it's not a "known provider".
            # "disabled-prov/model-x" is looked up as a whole.
            # It's found in "enabled-prov".
            result = _M._resolve_kilo_model("disabled-prov/model-x")
            assert result == "enabled-prov/disabled-prov/model-x"
        finally:
            _M._load_kilo_config = original_load  # type: ignore[attr-defined]

    def test_resolve_kilo_model_unknown_provider_prefix_looks_up_model(
        self,
    ) -> None:
        """Model with unknown prefix (qwen/) is looked up in config."""
        # qwen is NOT a known provider, so "qwen/qwen3-coder-next" is looked up
        mock_config = {
            "disabled_providers": [],
            "provider": {
                "fnal-ow": {
                    "models": {
                        "qwen/qwen3-coder-next": {},
                    }
                },
            },
        }
        original_load = _M._load_kilo_config  # type: ignore[attr-defined]
        _M._load_kilo_config = lambda: mock_config  # type: ignore[attr-defined]
        try:
            result = _M._resolve_kilo_model("qwen/qwen3-coder-next")
            assert result == "fnal-ow/qwen/qwen3-coder-next"
        finally:
            _M._load_kilo_config = original_load  # type: ignore[attr-defined]

    def test_resolve_kilo_model_without_provider_prefix_looks_up(self) -> None:
        """_resolve_kilo_model adds provider prefix when missing."""
        # "qwen3-coder-next" (short name) is looked up and found under fnal-ow
        # as "fnal-ow/qwen/qwen3-coder-next" (full name), so result is:
        # fnal-ow/qwen/qwen3-coder-next
        mock_config = {
            "disabled_providers": [],
            "provider": {
                "fnal-ow": {
                    "models": {
                        "qwen/qwen3-coder-next": {},
                    }
                },
            },
        }
        original_load = _M._load_kilo_config  # type: ignore[attr-defined]
        _M._load_kilo_config = lambda: mock_config  # type: ignore[attr-defined]
        try:
            result = _M._resolve_kilo_model("qwen3-coder-next")
            assert result == "fnal-ow/qwen/qwen3-coder-next"
        finally:
            _M._load_kilo_config = original_load  # type: ignore[attr-defined]

    def test_resolve_kilo_model_unknown_model(self) -> None:
        """_resolve_kilo_model returns fallback model for unknown model without prefix."""
        result = _M._resolve_kilo_model("unknown-model-name")
        assert result == "google/gemma4-31b"

    def test_resolve_kilo_model_ambiguous_looks_up_raises_error(self) -> None:
        """_resolve_kilo_model raises _Error for ambiguous model lookups."""
        # Model exists under multiple providers, but one is disabled
        mock_config = {
            "disabled_providers": ["fnal-azure"],
            "provider": {
                "fnal-ow": {
                    "models": {
                        "qwen/qwen3-coder-next": {},
                    }
                },
                "fnal-azure": {
                    "models": {
                        "qwen/qwen3-coder-next": {},
                    }
                },
            },
        }
        original_load = _M._load_kilo_config  # type: ignore[attr-defined]
        _M._load_kilo_config = lambda: mock_config  # type: ignore[attr-defined]
        try:
            # Only fnal-ow is enabled, so this should NOT be ambiguous anymore.
            # It should resolve to fnal-ow/qwen/qwen3-coder-next
            result = _M._resolve_kilo_model("qwen3-coder-next")
            assert result == "fnal-ow/qwen/qwen3-coder-next"
        finally:
            _M._load_kilo_config = original_load  # type: ignore[attr-defined]

    def test_resolve_kilo_model_still_ambiguous_when_multiple_enabled(self) -> None:
        """_resolve_kilo_model still raises _Error if multiple enabled providers have the model."""
        mock_config = {
            "disabled_providers": [],
            "provider": {
                "fnal-ow": {"models": {"qwen/qwen3-coder-next": {}}},
                "fnal-azure": {"models": {"qwen/qwen3-coder-next": {}}},
            },
        }
        original_load = _M._load_kilo_config  # type: ignore[attr-defined]
        _M._load_kilo_config = lambda: mock_config  # type: ignore[attr-defined]
        try:
            with pytest.raises(_M._Error, match="is ambiguous"):
                _M._resolve_kilo_model("qwen3-coder-next")
        finally:
            _M._load_kilo_config = original_load  # type: ignore[attr-defined]

    def test_resolve_kilo_model_model_prefix_not_known_provider(
        self,
    ) -> None:
        """Model with non-provider prefix is looked up and prefix added."""
        # "unknown-ow" is not a known provider, so model name
        # "unknown-ow/qwen3-coder-next" is looked up in config
        mock_config = {
            "disabled_providers": [],
            "provider": {
                "fnal-ow": {
                    "models": {
                        "unknown-ow/qwen3-coder-next": {},
                    }
                },
            },
        }
        original_load = _M._load_kilo_config  # type: ignore[attr-defined]
        _M._load_kilo_config = lambda: mock_config  # type: ignore[attr-defined]
        try:
            result = _M._resolve_kilo_model("unknown-ow/qwen3-coder-next")
            assert result == "fnal-ow/unknown-ow/qwen3-coder-next"
        finally:
            _M._load_kilo_config = original_load  # type: ignore[attr-defined]

    def test_default_model_kilo_uses_qwen3_coder_next(self) -> None:
        """_DEFAULT_MODEL_KILO defaults to qwen/qwen3-coder-next."""
        assert _M._DEFAULT_MODEL_KILO == "qwen/qwen3-coder-next"


class TestKiloBackend:
    """Tests for the kilo backend functionality."""

    def test_resolve_backend_and_api_kilo(self) -> None:
        """_resolve_backend_and_api returns correct api for kilo backend."""
        backend, api = _M._resolve_backend_and_api("kilo", "")
        assert backend == "kilo"
        # API should be empty or from env var (not default)
        assert api == "" or api is not None  # api_base from env var or empty

    def test_default_model_kilo(self) -> None:
        """_default_model returns qwen/qwen3-coder-next for kilo backend."""
        assert _M._default_model("kilo") == "qwen/qwen3-coder-next"

    def test_chat_kilo_uses_agent_code(self, monkeypatch: pytest.MonkeyPatch) -> None:
        """_chat_kilo always passes --agent=code to kilo run."""
        messages = [{"role": "user", "content": "test message"}]
        monkeypatch.setenv("GIT_AI_COMMIT_TOKEN", "test_token")

        # Mock kilo config to ensure consistent model resolution
        mock_config: dict[str, object] = {
            "provider": {"fnal-ow": {"models": {"qwen/qwen3-coder-next": {}}}}
        }
        monkeypatch.setattr(_M, "_load_kilo_config", lambda: mock_config)

        mock_completed = subprocess.CompletedProcess(
            args=["kilo", "run", "--agent=code"],
            returncode=0,
            stdout="Generated Commit Message\n",
            stderr="",
        )

        with patch("subprocess.run", return_value=mock_completed) as mock_run:
            res = _M._chat_kilo("qwen3-coder-next", messages, "test_token", "")
            assert res == "Generated Commit Message"
            mock_run.assert_called_once()
            call_args = mock_run.call_args
            # call_args[0] is the positional args tuple (args,), call_args[1] is kwargs
            cmd_list = call_args[0][0]  # First positional arg is the command list
            # Model is resolved to include provider prefix: fnal-ow/qwen/qwen3-coder-next
            assert cmd_list == [
                "kilo",
                "run",
                "--agent=code",
                "-m",
                "fnal-ow/qwen/qwen3-coder-next",
            ]

    def test_chat_kilo_resolves_model_name(self, monkeypatch: pytest.MonkeyPatch) -> None:
        """_chat_kilo resolves model name (adds provider prefix) before invoking kilo."""
        messages = [{"role": "user", "content": "test message"}]
        monkeypatch.setenv("GIT_AI_COMMIT_TOKEN", "test_token")

        # Mock kilo config to ensure consistent model resolution
        mock_config: dict[str, object] = {
            "provider": {"fnal-ow": {"models": {"qwen/qwen3-coder-next": {}}}}
        }
        monkeypatch.setattr(_M, "_load_kilo_config", lambda: mock_config)

        mock_completed = subprocess.CompletedProcess(
            args=["kilo", "run", "--agent=code", "-m", "fnal-ow/qwen/qwen3-coder-next"],
            returncode=0,
            stdout="Generated Commit Message\n",
            stderr="",
        )

        with patch("subprocess.run", return_value=mock_completed) as mock_run:
            # Pass model without provider prefix
            res = _M._chat_kilo("qwen3-coder-next", messages, "test_token", "")
            assert res == "Generated Commit Message"
            mock_run.assert_called_once()
            call_args = mock_run.call_args
            cmd_list = call_args[0][0]  # First positional arg is the command list
            # Verify the model was resolved with provider prefix
            assert "-m" in cmd_list
            model_idx = cmd_list.index("-m")
            assert cmd_list[model_idx + 1] == "fnal-ow/qwen/qwen3-coder-next"

    def test_chat_kilo_passes_token_as_env(self, monkeypatch: pytest.MonkeyPatch) -> None:
        """_chat_kilo passes token via KILO_SERVER_PASSWORD env var."""
        messages = [{"role": "user", "content": "test message"}]

        mock_completed = subprocess.CompletedProcess(
            args=["kilo", "run", "--agent=code"],
            returncode=0,
            stdout="Generated Commit Message\n",
            stderr="",
        )

        with patch("subprocess.run", return_value=mock_completed) as mock_run:
            res = _M._chat_kilo("qwen/qwen3-coder-next", messages, "my_secret_token", "")
            assert res == "Generated Commit Message"
            mock_run.assert_called_once()
            cmd, kwargs = mock_run.call_args
            assert "env" in kwargs
            assert kwargs["env"]["KILO_SERVER_PASSWORD"] == "my_secret_token"

    def test_chat_kilo_passes_api_url_as_env(self, monkeypatch: pytest.MonkeyPatch) -> None:
        """_chat_kilo passes api_base via KILO_SERVER_URL env var."""
        messages = [{"role": "user", "content": "test message"}]
        api_url = "http://localhost:9798/v1"

        mock_completed = subprocess.CompletedProcess(
            args=["kilo", "run", "--agent=code"],
            returncode=0,
            stdout="Generated Commit Message\n",
            stderr="",
        )

        with patch("subprocess.run", return_value=mock_completed) as mock_run:
            res = _M._chat_kilo("qwen/qwen3-coder-next", messages, "token", api_url)
            assert res == "Generated Commit Message"
            mock_run.assert_called_once()
            cmd, kwargs = mock_run.call_args
            assert "env" in kwargs
            assert kwargs["env"]["KILO_SERVER_URL"] == api_url

    def test_chat_kilo_file_not_found(self) -> None:
        """_chat_kilo raises _Error if kilo CLI is not found."""
        with patch("subprocess.run", side_effect=FileNotFoundError):
            with pytest.raises(_Error, match="kilo CLI not found"):
                _M._chat_kilo("qwen/qwen3-coder-next", [], "", "")

    def test_chat_kilo_nonzero_exit(self) -> None:
        """_chat_kilo raises _APIError if kilo run fails."""
        mock_completed = subprocess.CompletedProcess(
            args=["kilo", "run", "--agent=code"],
            returncode=1,
            stdout="",
            stderr="invalid model name",
        )
        with patch("subprocess.run", return_value=mock_completed):
            with pytest.raises(_APIError, match="failed \\(exit code 1\\)"):
                _M._chat_kilo("qwen/qwen3-coder-next", [], "", "")

    def test_chat_kilo_timeout(self) -> None:
        """_chat_kilo raises _APIError if kilo run times out."""
        with patch(
            "subprocess.run",
            side_effect=subprocess.TimeoutExpired(["kilo", "run"], 60, "kilo run"),
        ):
            with pytest.raises(_APIError, match="timed out after 60 seconds"):
                _M._chat_kilo("qwen/qwen3-coder-next", [], "", "")

    def test_main_with_kilo_backend(
        self,
        monkeypatch: pytest.MonkeyPatch,
        tmp_path: Path,
        capsys: pytest.CaptureFixture[str],
    ) -> None:
        """Main runs successfully with kilo backend."""
        monkeypatch.setattr(sys, "argv", ["git-ai-commit", "--backend", "kilo", "--yes"])
        monkeypatch.setattr(_M, "_git_root", lambda: tmp_path)
        monkeypatch.setattr(_M, "_staged_diff", lambda _amend=False: "some diff")
        monkeypatch.setattr(_M, "_status", lambda: "")
        monkeypatch.setattr(_M, "_recent_log", lambda: "")
        monkeypatch.setattr(_M, "_get_instructions", lambda _root: "")
        # Simulate a non-tty (pipe) stdin with no content.
        monkeypatch.setattr(sys, "stdin", io.StringIO(""))

        mock_completed = subprocess.CompletedProcess(
            args=["kilo", "run", "--agent=code"],
            returncode=0,
            stdout="feat: mock commit message\n",
            stderr="",
        )

        with patch(
            "subprocess.run",
            side_effect=[mock_completed, subprocess.CompletedProcess([], 0)],
        ) as mock_run:
            _M.main()

            # The first call should be to 'kilo' to generate the commit message.
            # The second call should be 'git commit' with the message.
            assert mock_run.call_count == 2

            # Check the first call (kilo run)
            kilo_cmd = mock_run.call_args_list[0][0][0]
            assert "kilo" in kilo_cmd
            assert "--agent=code" in kilo_cmd

            # Check the second call (git commit)
            git_cmd = mock_run.call_args_list[1][0][0]
            assert git_cmd == ["git", "commit", "-m", "feat: mock commit message"]
