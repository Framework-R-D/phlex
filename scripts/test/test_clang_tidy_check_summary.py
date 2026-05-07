"""Tests for clang_tidy_check_summary.py.

Coverage strategy
-----------------
Unit tests cover each public and module-private function in isolation.

Integration tests exercise main() end-to-end matching how the script is used
from the clang-tidy check/fix workflows:

    python3 scripts/clang_tidy_check_summary.py [input.yaml] [-o output.md] [--links]
"""

from __future__ import annotations

import sys
from io import StringIO
from pathlib import Path
from unittest.mock import patch
from urllib.parse import urlparse

import pytest

# sys.path is set up by scripts/test/conftest.py.
import clang_tidy_check_summary as M  # noqa: E402

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _yaml(diagnostics: list[dict]) -> str:
    """Minimal clang-tidy YAML with a Diagnostics list."""
    import yaml

    return yaml.dump({"Diagnostics": diagnostics})


def _diag(
    name: str = "readability-identifier-naming",
    file: str = "/src/foo.cpp",
    offset: int = 42,
) -> dict:
    """Return a minimal diagnostic entry dict."""
    return {
        "DiagnosticName": name,
        "DiagnosticMessage": {"FilePath": file, "FileOffset": offset},
    }


# ---------------------------------------------------------------------------
# _load_diagnostics
# ---------------------------------------------------------------------------


class TestLoadDiagnostics:
    """Tests for M._load_diagnostics."""

    def test_reads_from_file(self, tmp_path: Path) -> None:
        """Valid YAML file returns the Diagnostics list."""
        p = tmp_path / "fixes.yaml"
        p.write_text(_yaml([_diag()]), encoding="utf-8")
        result = M._load_diagnostics(p)
        assert len(result) == 1
        assert result[0]["DiagnosticName"] == "readability-identifier-naming"

    def test_reads_from_stdin(self, monkeypatch: pytest.MonkeyPatch) -> None:
        """Passing None reads from stdin."""
        monkeypatch.setattr(sys, "stdin", StringIO(_yaml([_diag("modernize-use-override")])))
        result = M._load_diagnostics(None)
        assert result[0]["DiagnosticName"] == "modernize-use-override"

    def test_invalid_yaml_returns_empty(
        self, tmp_path: Path, capsys: pytest.CaptureFixture[str]
    ) -> None:
        """Malformed YAML prints an error to stderr and returns []."""
        p = tmp_path / "bad.yaml"
        p.write_text("{not: valid: yaml:", encoding="utf-8")
        result = M._load_diagnostics(p)
        assert result == []
        assert "Failed to parse" in capsys.readouterr().err

    def test_non_dict_yaml_returns_empty(self, tmp_path: Path) -> None:
        """Top-level YAML that is not a dict returns []."""
        import yaml

        p = tmp_path / "list.yaml"
        p.write_text(yaml.dump([1, 2, 3]), encoding="utf-8")
        result = M._load_diagnostics(p)
        assert result == []

    def test_missing_diagnostics_key_returns_empty(self, tmp_path: Path) -> None:
        """Dict without a Diagnostics key returns []."""
        import yaml

        p = tmp_path / "no_diag.yaml"
        p.write_text(yaml.dump({"Other": "value"}), encoding="utf-8")
        result = M._load_diagnostics(p)
        assert result == []

    def test_non_list_diagnostics_value_returns_empty(self, tmp_path: Path) -> None:
        """Diagnostics key mapped to a non-list returns []."""
        import yaml

        p = tmp_path / "bad_diag.yaml"
        p.write_text(yaml.dump({"Diagnostics": "not-a-list"}), encoding="utf-8")
        result = M._load_diagnostics(p)
        assert result == []

    def test_empty_diagnostics_returns_empty_list(self, tmp_path: Path) -> None:
        """Diagnostics: [] returns an empty list."""
        p = tmp_path / "empty.yaml"
        p.write_text(_yaml([]), encoding="utf-8")
        result = M._load_diagnostics(p)
        assert result == []


# ---------------------------------------------------------------------------
# count_unique_diagnostics
# ---------------------------------------------------------------------------


class TestCountUniqueDiagnostics:
    """Tests for M.count_unique_diagnostics."""

    def test_single_entry_counted(self) -> None:
        """One diagnostic yields count of 1."""
        diags = [_diag("readability-braces-around-statements")]
        result = M.count_unique_diagnostics(diags)
        assert result == {"readability-braces-around-statements": 1}

    def test_identical_triplet_counted_once(self) -> None:
        """Duplicate (name, file, offset) triplets collapse to count of 1."""
        diags = [_diag("cert-err34-c", "/a.cpp", 10), _diag("cert-err34-c", "/a.cpp", 10)]
        result = M.count_unique_diagnostics(diags)
        assert result == {"cert-err34-c": 1}

    def test_different_offsets_counted_separately(self) -> None:
        """Same name and file, different offsets → count of 2."""
        diags = [_diag("cert-err34-c", "/a.cpp", 10), _diag("cert-err34-c", "/a.cpp", 20)]
        result = M.count_unique_diagnostics(diags)
        assert result == {"cert-err34-c": 2}

    def test_multiple_checks_counted_independently(self) -> None:
        """Two different check names produce two independent entries."""
        diags = [_diag("check-a", "/a.cpp", 1), _diag("check-b", "/b.cpp", 2)]
        result = M.count_unique_diagnostics(diags)
        assert result == {"check-a": 1, "check-b": 1}

    def test_empty_list_returns_empty_dict(self) -> None:
        """No diagnostics yields an empty dict."""
        assert M.count_unique_diagnostics([]) == {}

    def test_missing_diagnostic_name_defaults_to_clang_tidy(self) -> None:
        """An entry without DiagnosticName defaults to 'clang-tidy'."""
        diags = [{"DiagnosticMessage": {"FilePath": "/x.cpp", "FileOffset": 0}}]
        result = M.count_unique_diagnostics(diags)
        assert "clang-tidy" in result

    def test_invalid_offset_treated_as_none(self) -> None:
        """Non-numeric FileOffset is stored as None (not an error)."""
        diag_bad = {
            "DiagnosticName": "some-check",
            "DiagnosticMessage": {"FilePath": "/f.cpp", "FileOffset": "not-an-int"},
        }
        result = M.count_unique_diagnostics([diag_bad])
        assert result == {"some-check": 1}

    def test_none_offset_counted_distinctly(self) -> None:
        """Two entries with the same name+file but offset=None are one unique occurrence."""
        msg = {"FilePath": "/f.cpp", "FileOffset": None}
        diag_a = {"DiagnosticName": "x", "DiagnosticMessage": msg}
        diag_b = {"DiagnosticName": "x", "DiagnosticMessage": msg}
        result = M.count_unique_diagnostics([diag_a, diag_b])
        assert result == {"x": 1}

    def test_missing_diagnostic_message_does_not_raise(self) -> None:
        """Entry with no DiagnosticMessage key is handled gracefully."""
        diags = [{"DiagnosticName": "my-check"}]
        result = M.count_unique_diagnostics(diags)
        assert result == {"my-check": 1}


# ---------------------------------------------------------------------------
# _check_url
# ---------------------------------------------------------------------------


class TestCheckUrl:
    """Tests for M._check_url."""

    def test_clang_analyzer_prefix(self) -> None:
        """clang-analyzer-* checks map to the clang-analyzer category."""
        url = M._check_url("clang-analyzer-core.NullDereference")
        assert "clang-analyzer/core.NullDereference" in url

    def test_category_check_split_on_first_dash(self) -> None:
        """readability-identifier-naming maps to readability/identifier-naming."""
        url = M._check_url("readability-identifier-naming")
        assert "readability/identifier-naming" in url

    def test_no_dash_falls_back_to_root(self) -> None:
        """A check with no dash still produces a valid URL."""
        url = M._check_url("clang-tidy")
        parsed = urlparse(url)
        assert parsed.hostname == "clang.llvm.org"

    def test_url_starts_with_https(self) -> None:
        """All generated URLs start with https://."""
        assert M._check_url("modernize-use-override").startswith("https://")

    def test_url_ends_with_html(self) -> None:
        """Generated URLs end with .html (links to the check documentation page)."""
        assert M._check_url("cert-err34-c").endswith(".html")

    def test_check_url_in_format_checklist_link(self) -> None:
        """The URL embedded in --links output resolves correctly for a known check."""
        result = M.format_checklist({"modernize-use-nullptr": 1}, links=True)
        assert "modernize/use-nullptr.html" in result


# ---------------------------------------------------------------------------
# format_checklist
# ---------------------------------------------------------------------------


class TestFormatChecklist:
    """Tests for M.format_checklist."""

    def test_basic_entry(self) -> None:
        """Single count entry produces the expected markdown line."""
        result = M.format_checklist({"check-a": 3})
        assert result == "- [ ] check-a (3)"

    def test_entries_sorted_alphabetically(self) -> None:
        """Multiple entries appear in alphabetical order."""
        counts = {"z-check": 1, "a-check": 2}
        lines = M.format_checklist(counts).splitlines()
        assert lines[0].startswith("- [ ] a-check")
        assert lines[1].startswith("- [ ] z-check")

    def test_links_mode_wraps_name_in_markdown_link(self) -> None:
        """--links mode produces a Markdown hyperlink for the check name."""
        result = M.format_checklist({"readability-magic-numbers": 1}, links=True)
        assert "[readability-magic-numbers]" in result
        assert "http" in result

    def test_no_links_mode_plain_name(self) -> None:
        """Without links, the check name appears without markup."""
        result = M.format_checklist({"cert-err34-c": 2}, links=False)
        assert "[cert-err34-c](" not in result
        assert "cert-err34-c (2)" in result

    def test_empty_dict_returns_empty_string(self) -> None:
        """Empty counts dict produces an empty string (no newlines)."""
        assert M.format_checklist({}) == ""


# ---------------------------------------------------------------------------
# build_arg_parser
# ---------------------------------------------------------------------------


class TestBuildArgParser:
    """Tests for M.build_arg_parser."""

    def test_returns_parser(self) -> None:
        """build_arg_parser() returns a working ArgumentParser."""
        import argparse

        parser = M.build_arg_parser()
        assert isinstance(parser, argparse.ArgumentParser)

    def test_prog_name(self) -> None:
        """Parser prog name identifies the script by filename."""
        parser = M.build_arg_parser()
        assert "clang_tidy_check_summary" in parser.prog

    def test_defaults_without_args(self) -> None:
        """Parsing [] gives None for input, None for output, and False for links."""
        args = M.build_arg_parser().parse_args([])
        assert args.input is None
        assert args.output is None
        assert args.links is False

    def test_input_path_parsed(self, tmp_path: Path) -> None:
        """Positional input argument is parsed as a Path."""
        p = tmp_path / "f.yaml"
        args = M.build_arg_parser().parse_args([str(p)])
        assert args.input == p

    def test_output_flag_parsed(self, tmp_path: Path) -> None:
        """-o / --output flag is parsed as a Path."""
        p = tmp_path / "out.md"
        args = M.build_arg_parser().parse_args(["-o", str(p)])
        assert args.output == p

    def test_links_flag_sets_true(self) -> None:
        """--links sets links=True."""
        args = M.build_arg_parser().parse_args(["--links"])
        assert args.links is True

    def test_help_mentions_fixes_yaml(self, capsys: pytest.CaptureFixture[str]) -> None:
        """--help output mentions the YAML file to help users find the right file."""
        with pytest.raises(SystemExit):
            M.build_arg_parser().parse_args(["--help"])
        out = capsys.readouterr().out
        assert "yaml" in out.lower() or "YAML" in out


# ---------------------------------------------------------------------------
# main()
# ---------------------------------------------------------------------------


class TestMain:
    """Integration tests for M.main()."""

    def _write_yaml(self, path: Path, diagnostics: list[dict]) -> Path:
        """Write a minimal clang-tidy YAML to path and return it."""
        path.write_text(_yaml(diagnostics), encoding="utf-8")
        return path

    def test_no_diagnostics_prints_message_and_returns_zero(
        self, tmp_path: Path, capsys: pytest.CaptureFixture[str]
    ) -> None:
        """No diagnostics → informational message and exit code 0."""
        p = self._write_yaml(tmp_path / "empty.yaml", [])
        with patch("sys.argv", ["prog", str(p)]):
            rc = M.main()
        assert rc == 0
        assert "No diagnostics" in capsys.readouterr().err

    def test_single_diagnostic_written_to_stdout(
        self, tmp_path: Path, capsys: pytest.CaptureFixture[str]
    ) -> None:
        """One diagnostic prints the checklist to stdout."""
        p = self._write_yaml(tmp_path / "diag.yaml", [_diag("modernize-use-nullptr")])
        with patch("sys.argv", ["prog", str(p)]):
            rc = M.main()
        assert rc == 0
        out = capsys.readouterr().out
        assert "modernize-use-nullptr" in out
        assert "- [ ]" in out

    def test_output_written_to_file(self, tmp_path: Path) -> None:
        """With -o, the checklist is written to a file, not stdout."""
        p = self._write_yaml(tmp_path / "diag.yaml", [_diag("cert-err34-c")])
        out_file = tmp_path / "summary.md"
        with patch("sys.argv", ["prog", str(p), "-o", str(out_file)]):
            rc = M.main()
        assert rc == 0
        content = out_file.read_text(encoding="utf-8")
        assert "cert-err34-c" in content
        assert content.endswith("\n")

    def test_links_mode_produces_hyperlinks(
        self, tmp_path: Path, capsys: pytest.CaptureFixture[str]
    ) -> None:
        """--links flag produces Markdown hyperlinks in output."""
        p = self._write_yaml(tmp_path / "diag.yaml", [_diag("readability-magic-numbers")])
        with patch("sys.argv", ["prog", str(p), "--links"]):
            M.main()
        out = capsys.readouterr().out
        assert "http" in out

    def test_duplicate_diagnostics_collapsed(
        self, tmp_path: Path, capsys: pytest.CaptureFixture[str]
    ) -> None:
        """Identical (name, file, offset) triplets appear as count=1."""
        diag = _diag("check-x", "/f.cpp", 10)
        p = self._write_yaml(tmp_path / "dup.yaml", [diag, diag])
        with patch("sys.argv", ["prog", str(p)]):
            M.main()
        out = capsys.readouterr().out
        assert "check-x (1)" in out

    def test_checklist_alphabetically_ordered(
        self, tmp_path: Path, capsys: pytest.CaptureFixture[str]
    ) -> None:
        """Checklist entries appear in alphabetical order."""
        diags = [_diag("z-check", "/a.cpp", 1), _diag("a-check", "/b.cpp", 2)]
        p = self._write_yaml(tmp_path / "multi.yaml", diags)
        with patch("sys.argv", ["prog", str(p)]):
            M.main()
        out = capsys.readouterr().out
        lines = [ln for ln in out.splitlines() if ln.startswith("- [ ]")]
        assert lines[0].startswith("- [ ] a-check")
        assert lines[1].startswith("- [ ] z-check")

    def test_stdin_input(
        self, monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]
    ) -> None:
        """With no positional argument, input is read from stdin."""
        yaml_text = _yaml([_diag("cppcoreguidelines-avoid-goto")])
        monkeypatch.setattr(sys, "stdin", StringIO(yaml_text))
        with patch("sys.argv", ["prog"]):
            rc = M.main()
        assert rc == 0
        out = capsys.readouterr().out
        assert "cppcoreguidelines-avoid-goto" in out

    def test_output_file_has_trailing_newline(self, tmp_path: Path) -> None:
        """Output file ends with exactly one newline."""
        p = self._write_yaml(tmp_path / "d.yaml", [_diag("cert-err34-c")])
        out_file = tmp_path / "out.md"
        with patch("sys.argv", ["prog", str(p), "-o", str(out_file)]):
            M.main()
        raw = out_file.read_bytes()
        assert raw.endswith(b"\n")
        assert not raw.endswith(b"\n\n")
