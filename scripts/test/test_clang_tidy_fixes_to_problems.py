"""Tests for clang_tidy_fixes_to_problems.py.

Coverage strategy
-----------------
Unit tests cover every public and module-private function.

Integration tests exercise main() end-to-end using temporary YAML files and
real (temporary) source files so that offset_to_line_col can read them.  The
output format must match the gcc-compatible problem matcher used by VS Code:

    /abs/path/file.cpp:line:col: severity: message [check-name]
"""

from __future__ import annotations

import sys
from io import StringIO
from pathlib import Path
from unittest.mock import patch

import pytest
import yaml

# sys.path is set up by scripts/test/conftest.py.
import clang_tidy_fixes_to_problems as M  # noqa: E402

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _fixes_yaml(
    diagnostics: list[dict],
    main_source: str = "/src/main.cpp",
) -> str:
    """Minimal clang-tidy-fixes YAML string."""
    return yaml.dump(
        {
            "MainSourceFile": main_source,
            "Diagnostics": diagnostics,
        }
    )


def _diag(
    name: str = "readability-identifier-naming",
    message: str = "variable is not in camelCase",
    level: str = "warning",
    file_path: str = "/src/main.cpp",
    file_offset: int = 42,
    notes: list[dict] | None = None,
) -> dict:
    """Return a minimal diagnostic entry dict."""
    entry: dict = {
        "DiagnosticName": name,
        "Level": level,
        "DiagnosticMessage": {
            "FilePath": file_path,
            "FileOffset": file_offset,
            "Message": message,
        },
    }
    if notes is not None:
        entry["Notes"] = notes
    return entry


# ---------------------------------------------------------------------------
# parse_clang_tidy_fixes
# ---------------------------------------------------------------------------


class TestParseClangTidyFixes:
    """Tests for M.parse_clang_tidy_fixes."""

    def test_basic_parse(self) -> None:
        """A single diagnostic is parsed into one Diagnostic dataclass."""
        text = _fixes_yaml([_diag()])
        main_src, diags = M.parse_clang_tidy_fixes(text)
        assert main_src == "/src/main.cpp"
        assert len(diags) == 1
        assert diags[0].check == "readability-identifier-naming"
        assert diags[0].message == "variable is not in camelCase"
        assert diags[0].level == "warning"
        assert diags[0].file_path == "/src/main.cpp"
        assert diags[0].file_offset == 42

    def test_empty_diagnostics(self) -> None:
        """Empty Diagnostics list yields empty result."""
        text = _fixes_yaml([])
        main_src, diags = M.parse_clang_tidy_fixes(text)
        assert diags == []

    def test_invalid_yaml_returns_empty(self, capsys: pytest.CaptureFixture[str]) -> None:
        """Malformed YAML prints an error and returns (None, [])."""
        main_src, diags = M.parse_clang_tidy_fixes("{not: valid: yaml:")
        assert main_src is None
        assert diags == []
        assert "Failed to parse" in capsys.readouterr().err

    def test_non_dict_yaml_returns_empty(self) -> None:
        """Non-dict top-level YAML returns (None, [])."""
        main_src, diags = M.parse_clang_tidy_fixes(yaml.dump([1, 2, 3]))
        assert main_src is None
        assert diags == []

    def test_notes_parsed(self) -> None:
        """Notes list is parsed into DiagnosticNote objects."""
        note = {"FilePath": "/inc/foo.h", "FileOffset": 10, "Message": "Calling 'bar'"}
        text = _fixes_yaml([_diag(notes=[note])])
        _, diags = M.parse_clang_tidy_fixes(text)
        assert diags[0].notes is not None
        assert len(diags[0].notes) == 1
        assert diags[0].notes[0].message == "Calling 'bar'"

    def test_invalid_offset_treated_as_none(self) -> None:
        """Non-numeric FileOffset in a diagnostic is stored as None."""
        entry = {
            "DiagnosticName": "my-check",
            "Level": "warning",
            "DiagnosticMessage": {"FilePath": "/f.cpp", "FileOffset": "bad", "Message": "oops"},
        }
        raw = yaml.dump({"MainSourceFile": "/f.cpp", "Diagnostics": [entry]})
        _, diags = M.parse_clang_tidy_fixes(raw)
        assert diags[0].file_offset is None

    def test_level_lowercased(self) -> None:
        """Level field is stored in lowercase."""
        text = _fixes_yaml([_diag(level="Warning")])
        _, diags = M.parse_clang_tidy_fixes(text)
        assert diags[0].level == "warning"

    def test_missing_level_defaults_to_warning(self) -> None:
        """A diagnostic without Level defaults to 'warning'."""
        entry = {
            "DiagnosticName": "my-check",
            "DiagnosticMessage": {"FilePath": "/f.cpp", "FileOffset": 0, "Message": "msg"},
        }
        raw = yaml.dump({"MainSourceFile": "/f.cpp", "Diagnostics": [entry]})
        _, diags = M.parse_clang_tidy_fixes(raw)
        assert diags[0].level == "warning"

    def test_non_dict_diagnostic_entry_skipped(self) -> None:
        """A non-dict entry in Diagnostics is skipped gracefully."""
        raw = yaml.dump({"MainSourceFile": "/f.cpp", "Diagnostics": ["not-a-dict"]})
        _, diags = M.parse_clang_tidy_fixes(raw)
        assert diags == []


# ---------------------------------------------------------------------------
# apply_path_map
# ---------------------------------------------------------------------------


class TestApplyPathMap:
    """Tests for M.apply_path_map."""

    def test_matching_prefix_replaced(self) -> None:
        """A matching OLD prefix is replaced by NEW."""
        result = M.apply_path_map("/old/path/file.cpp", [("/old/path", "/new/path")])
        assert result == "/new/path/file.cpp"

    def test_first_matching_prefix_wins(self) -> None:
        """Only the first matching mapping is applied."""
        result = M.apply_path_map("/a/b.cpp", [("/a", "/x"), ("/a", "/y")])
        assert result == "/x/b.cpp"

    def test_no_match_returns_original(self) -> None:
        """A path that matches no mapping is returned unchanged."""
        result = M.apply_path_map("/unchanged/file.cpp", [("/other", "/z")])
        assert result == "/unchanged/file.cpp"

    def test_empty_mappings_returns_original(self) -> None:
        """Empty mappings list returns the original path."""
        assert M.apply_path_map("/foo/bar.cpp", []) == "/foo/bar.cpp"


# ---------------------------------------------------------------------------
# offset_to_line_col
# ---------------------------------------------------------------------------


class TestOffsetToLineCol:
    """Tests for M.offset_to_line_col."""

    def test_start_of_file(self, tmp_path: Path) -> None:
        """Offset 0 → line 1, col 1."""
        f = tmp_path / "f.cpp"
        f.write_bytes(b"int main() {}\n")
        assert M.offset_to_line_col(f, 0) == (1, 1)

    def test_middle_of_first_line(self, tmp_path: Path) -> None:
        """Offset 4 (after 'int ') → line 1, col 5."""
        f = tmp_path / "f.cpp"
        f.write_bytes(b"int main() {}\n")
        assert M.offset_to_line_col(f, 4) == (1, 5)

    def test_second_line(self, tmp_path: Path) -> None:
        """Offset immediately after the first newline → line 2, col 1."""
        f = tmp_path / "f.cpp"
        f.write_bytes(b"line1\nline2\n")
        assert M.offset_to_line_col(f, 6) == (2, 1)

    def test_beyond_eof_clamped(self, tmp_path: Path) -> None:
        """Offset beyond EOF is clamped and does not raise."""
        f = tmp_path / "f.cpp"
        f.write_bytes(b"ab\n")
        line, col = M.offset_to_line_col(f, 9999)
        assert line >= 1
        assert col >= 1

    def test_nonexistent_file_returns_1_1(self, tmp_path: Path) -> None:
        """A missing file returns (1, 1) without raising."""
        assert M.offset_to_line_col(tmp_path / "ghost.cpp", 10) == (1, 1)

    def test_empty_file_returns_1_1(self, tmp_path: Path) -> None:
        """An empty file always returns (1, 1)."""
        f = tmp_path / "empty.cpp"
        f.write_bytes(b"")
        assert M.offset_to_line_col(f, 0) == (1, 1)


# ---------------------------------------------------------------------------
# parse_path_map
# ---------------------------------------------------------------------------


class TestParsePathMap:
    """Tests for M.parse_path_map."""

    def test_single_mapping(self) -> None:
        """A single OLD=NEW string is parsed into one tuple."""
        result = M.parse_path_map(["/__w/phlex=/workspace"])
        assert result == [("/__w/phlex", "/workspace")]

    def test_multiple_mappings(self) -> None:
        """Multiple items produce multiple tuples, in order."""
        result = M.parse_path_map(["/a=/x", "/b=/y"])
        assert result == [("/a", "/x"), ("/b", "/y")]

    def test_empty_list_returns_empty(self) -> None:
        """An empty list returns an empty list."""
        assert M.parse_path_map([]) == []

    def test_missing_equals_raises_value_error(self) -> None:
        """A mapping without '=' raises ValueError."""
        with pytest.raises(ValueError, match="Invalid"):
            M.parse_path_map(["noequalssign"])

    def test_value_may_contain_equals(self) -> None:
        """Only the first '=' is used as the split point."""
        result = M.parse_path_map(["/old=/new=extra"])
        assert result == [("/old", "/new=extra")]


# ---------------------------------------------------------------------------
# is_within_workspace
# ---------------------------------------------------------------------------


class TestIsWithinWorkspace:
    """Tests for M.is_within_workspace."""

    def test_file_inside_workspace(self, tmp_path: Path) -> None:
        """A path inside workspace_root returns True."""
        child = tmp_path / "src" / "file.cpp"
        assert M.is_within_workspace(str(child), tmp_path)

    def test_file_outside_workspace(self, tmp_path: Path) -> None:
        """A path outside workspace_root returns False."""
        other = tmp_path.parent / "other" / "file.cpp"
        assert not M.is_within_workspace(str(other), tmp_path)

    def test_os_error_returns_false(self, tmp_path: Path) -> None:
        """An OSError (e.g. bad path chars) returns False without raising."""
        assert not M.is_within_workspace("\x00bad", tmp_path)


# ---------------------------------------------------------------------------
# choose_workspace_note
# ---------------------------------------------------------------------------


class TestChooseWorkspaceNote:
    """Tests for M.choose_workspace_note."""

    def test_returns_none_when_no_notes(self, tmp_path: Path) -> None:
        """Empty notes list returns None."""
        assert M.choose_workspace_note([], tmp_path) is None

    def test_returns_none_when_no_workspace_notes(self, tmp_path: Path) -> None:
        """No notes inside the workspace returns None."""
        note = M.DiagnosticNote(file_path="/external/foo.h", message="msg")
        assert M.choose_workspace_note([note], tmp_path) is None

    def test_prefers_calling_note(self, tmp_path: Path) -> None:
        """A note starting with 'Calling '' is preferred over other workspace notes."""
        calling = M.DiagnosticNote(file_path=str(tmp_path / "a.cpp"), message="Calling 'foo'")
        other = M.DiagnosticNote(file_path=str(tmp_path / "b.cpp"), message="Another note")
        result = M.choose_workspace_note([other, calling], tmp_path)
        assert result is calling

    def test_falls_back_to_first_workspace_note(self, tmp_path: Path) -> None:
        """Without a 'Calling' note, the first workspace note is returned."""
        n1 = M.DiagnosticNote(file_path=str(tmp_path / "a.cpp"), message="Note A")
        n2 = M.DiagnosticNote(file_path=str(tmp_path / "b.cpp"), message="Note B")
        result = M.choose_workspace_note([n1, n2], tmp_path)
        assert result is n1

    def test_note_with_none_file_path_excluded(self, tmp_path: Path) -> None:
        """A note with no file_path is excluded from workspace notes."""
        note = M.DiagnosticNote(file_path=None, message="orphan note")
        assert M.choose_workspace_note([note], tmp_path) is None


# ---------------------------------------------------------------------------
# build_arg_parser
# ---------------------------------------------------------------------------


class TestBuildArgParser:
    """Tests for M.build_arg_parser."""

    def test_defaults(self) -> None:
        """Parsing [] produces sensible defaults."""
        parser = M.build_arg_parser()
        args = parser.parse_args([])
        assert args.input is None
        assert args.output is None
        assert args.path_map == []
        assert isinstance(args.workspace_root, Path)

    def test_prog_name(self) -> None:
        """Parser prog name identifies the script by filename."""
        parser = M.build_arg_parser()
        assert "clang_tidy_fixes_to_problems" in parser.prog

    def test_input_parsed(self, tmp_path: Path) -> None:
        """Positional input is parsed as a Path."""
        p = tmp_path / "f.yaml"
        args = M.build_arg_parser().parse_args([str(p)])
        assert args.input == p

    def test_path_map_flag(self) -> None:
        """--path-map can be specified multiple times."""
        args = M.build_arg_parser().parse_args(["--path-map", "/a=/b", "--path-map", "/c=/d"])
        assert args.path_map == ["/a=/b", "/c=/d"]

    def test_help_mentions_path_map(self, capsys: pytest.CaptureFixture[str]) -> None:
        """--help output explains --path-map for users navigating CI artifacts."""
        with pytest.raises(SystemExit):
            M.build_arg_parser().parse_args(["--help"])
        out = capsys.readouterr().out
        assert "path-map" in out or "path_map" in out


# ---------------------------------------------------------------------------
# main() — integration tests
# ---------------------------------------------------------------------------


class TestMain:
    """Integration tests for M.main()."""

    def _write_fixes(self, path: Path, diagnostics: list[dict], main_source: str = "") -> Path:
        """Write a fixes YAML and return its path."""
        path.write_text(_fixes_yaml(diagnostics, main_source), encoding="utf-8")
        return path

    def test_no_diagnostics_produces_empty_output(
        self, tmp_path: Path, capsys: pytest.CaptureFixture[str]
    ) -> None:
        """An empty fixes file produces no output and returns 0."""
        p = self._write_fixes(tmp_path / "empty.yaml", [])
        with patch("sys.argv", ["prog", str(p)]):
            rc = M.main()
        assert rc == 0
        assert capsys.readouterr().out == ""

    def test_single_diagnostic_written_to_stdout(
        self, tmp_path: Path, capsys: pytest.CaptureFixture[str]
    ) -> None:
        """One diagnostic with a real source file prints a gcc-style line."""
        src = tmp_path / "main.cpp"
        src.write_bytes(b"int x;\n")
        diag = _diag(file_path=str(src), file_offset=0)
        p = self._write_fixes(tmp_path / "fixes.yaml", [diag], str(src))
        with patch("sys.argv", ["prog", str(p), "--workspace-root", str(tmp_path)]):
            rc = M.main()
        assert rc == 0
        out = capsys.readouterr().out
        assert "readability-identifier-naming" in out
        # gcc format: path:line:col: severity: message [check]
        assert ": warning:" in out

    def test_output_written_to_file(
        self, tmp_path: Path, capsys: pytest.CaptureFixture[str]
    ) -> None:
        """With -o, output is written to a file and a summary is printed to stdout."""
        src = tmp_path / "main.cpp"
        src.write_bytes(b"int x;\n")
        diag = _diag(file_path=str(src), file_offset=0)
        p = self._write_fixes(tmp_path / "fixes.yaml", [diag], str(src))
        out_file = tmp_path / "problems.txt"
        with patch(
            "sys.argv",
            ["prog", str(p), "-o", str(out_file), "--workspace-root", str(tmp_path)],
        ):
            rc = M.main()
        assert rc == 0
        assert out_file.exists()
        content = out_file.read_text(encoding="utf-8")
        assert "readability-identifier-naming" in content
        # The summary line uses "diagnostic(s)" (with the parenthetical s)
        stdout = capsys.readouterr().out
        assert "diagnostic" in stdout

    def test_stdout_output_ends_with_newline(
        self, tmp_path: Path, capsys: pytest.CaptureFixture[str]
    ) -> None:
        """When writing to stdout with content, output ends with newline."""
        src = tmp_path / "main.cpp"
        src.write_bytes(b"int x;\n")
        diag = _diag(file_path=str(src), file_offset=0)
        p = self._write_fixes(tmp_path / "fixes.yaml", [diag], str(src))
        with patch("sys.argv", ["prog", str(p), "--workspace-root", str(tmp_path)]):
            M.main()
        out = capsys.readouterr().out
        assert out.endswith("\n")

    def test_path_map_applied(self, tmp_path: Path, capsys: pytest.CaptureFixture[str]) -> None:
        """--path-map translates paths in the output."""
        src = tmp_path / "main.cpp"
        src.write_bytes(b"int x;\n")
        fake_path = "/__w/phlex/phlex/phlex-src/main.cpp"
        p = self._write_fixes(
            tmp_path / "fixes.yaml",
            [_diag(file_path=fake_path, file_offset=0)],
            fake_path,
        )
        with patch(
            "sys.argv",
            [
                "prog",
                str(p),
                "--path-map",
                f"/__w/phlex/phlex/phlex-src={tmp_path}",
                "--workspace-root",
                str(tmp_path),
            ],
        ):
            M.main()
        out = capsys.readouterr().out
        assert "/__w/phlex" not in out

    def test_stdin_input(
        self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]
    ) -> None:
        """Without a positional arg, the fixes YAML is read from stdin."""
        src = tmp_path / "main.cpp"
        src.write_bytes(b"void f();\n")
        yaml_text = _fixes_yaml([_diag(file_path=str(src), file_offset=0)], str(src))
        monkeypatch.setattr(sys, "stdin", StringIO(yaml_text))
        with patch("sys.argv", ["prog", "--workspace-root", str(tmp_path)]):
            rc = M.main()
        assert rc == 0

    def test_diagnostic_without_file_path_skipped(
        self, tmp_path: Path, capsys: pytest.CaptureFixture[str]
    ) -> None:
        """A diagnostic with no FilePath and no MainSourceFile is skipped."""
        raw = yaml.dump(
            {
                "MainSourceFile": "",
                "Diagnostics": [
                    {
                        "DiagnosticName": "some-check",
                        "Level": "warning",
                        "DiagnosticMessage": {"FilePath": "", "FileOffset": 0, "Message": "oops"},
                    }
                ],
            }
        )
        yaml_path = tmp_path / "fixes.yaml"
        yaml_path.write_text(raw, encoding="utf-8")
        with patch("sys.argv", ["prog", str(yaml_path), "--workspace-root", str(tmp_path)]):
            rc = M.main()
        assert rc == 0
        assert capsys.readouterr().out == ""

    def test_invalid_level_defaults_to_warning(
        self, tmp_path: Path, capsys: pytest.CaptureFixture[str]
    ) -> None:
        """An unrecognised severity level falls back to 'warning'."""
        src = tmp_path / "main.cpp"
        src.write_bytes(b"int x;\n")
        diag = _diag(level="fatal", file_path=str(src), file_offset=0)
        p = self._write_fixes(tmp_path / "fixes.yaml", [diag], str(src))
        with patch("sys.argv", ["prog", str(p), "--workspace-root", str(tmp_path)]):
            M.main()
        out = capsys.readouterr().out
        assert ": warning:" in out
