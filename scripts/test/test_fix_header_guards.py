"""Tests for fix_header_guards.py.

Coverage strategy
-----------------
Unit tests cover compute_expected_guard, check_header_guard, and fix_header_guard
using temporary files.

Integration tests exercise main() end-to-end, matching how the script is
invoked from .github/workflows/header-guards-check.yaml and
.github/workflows/header-guards-fix.yaml:

    # Check mode
    python3 scripts/fix_header_guards.py --check --root . phlex plugins form

    # Fix mode
    python3 scripts/fix_header_guards.py --root . phlex plugins form
"""

from __future__ import annotations

import textwrap
from pathlib import Path
from unittest.mock import patch

import pytest

# sys.path is set up by scripts/test/conftest.py.
import fix_header_guards as M  # noqa: E402

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _make_header(
    tmp_path: Path,
    subdir: str,
    filename: str,
    content: str,
) -> Path:
    """Write content to tmp_path/<subdir>/<filename> and return the path."""
    parent = tmp_path / subdir
    parent.mkdir(parents=True, exist_ok=True)
    f = parent / filename
    f.write_text(content, encoding="utf-8")
    return f


def _correct_guard_content(guard: str) -> str:
    """Return a minimal header file whose guard matches the given macro."""
    return textwrap.dedent(
        f"""\
        #ifndef {guard}
        #define {guard}

        // content

        #endif // {guard}
        """
    )


def _wrong_guard_content(guard: str, wrong_macro: str = "WRONG_GUARD_HPP") -> str:
    """Return a minimal header with an incorrect guard macro."""
    return textwrap.dedent(
        f"""\
        #ifndef {wrong_macro}
        #define {wrong_macro}

        // content

        #endif // {wrong_macro}
        """
    )


# ---------------------------------------------------------------------------
# compute_expected_guard
# ---------------------------------------------------------------------------


class TestComputeExpectedGuard:
    """Tests for M.compute_expected_guard."""

    def test_simple_two_level_path(self, tmp_path: Path) -> None:
        """phlex/foo.hpp → PHLEX_FOO_HPP."""
        f = tmp_path / "phlex" / "foo.hpp"
        assert M.compute_expected_guard(f, tmp_path) == "PHLEX_FOO_HPP"

    def test_three_level_path(self, tmp_path: Path) -> None:
        """phlex/detail/bar.hpp → PHLEX_DETAIL_BAR_HPP."""
        f = tmp_path / "phlex" / "detail" / "bar.hpp"
        assert M.compute_expected_guard(f, tmp_path) == "PHLEX_DETAIL_BAR_HPP"

    def test_four_level_path(self, tmp_path: Path) -> None:
        """phlex/a/b/c.hpp → PHLEX_A_B_C_HPP."""
        f = tmp_path / "phlex" / "a" / "b" / "c.hpp"
        assert M.compute_expected_guard(f, tmp_path) == "PHLEX_A_B_C_HPP"

    def test_h_extension(self, tmp_path: Path) -> None:
        """A .h file produces an _H suffix."""
        f = tmp_path / "phlex" / "c_compat.h"
        assert M.compute_expected_guard(f, tmp_path) == "PHLEX_C_COMPAT_H"

    def test_hyphen_in_subdir_converted_to_underscore(self, tmp_path: Path) -> None:
        """Hyphens in directory or file names are replaced with underscores."""
        f = tmp_path / "my-dir" / "my-file.hpp"
        assert M.compute_expected_guard(f, tmp_path) == "MY_DIR_MY_FILE_HPP"

    def test_hyphen_in_filename_converted_to_underscore(self, tmp_path: Path) -> None:
        """Hyphens in the file stem are replaced with underscores."""
        f = tmp_path / "phlex" / "my-util.hpp"
        assert M.compute_expected_guard(f, tmp_path) == "PHLEX_MY_UTIL_HPP"

    def test_uppercase_components_preserved(self, tmp_path: Path) -> None:
        """Components are always uppercased regardless of input case."""
        f = tmp_path / "Phlex" / "FooBar.hpp"
        assert M.compute_expected_guard(f, tmp_path) == "PHLEX_FOOBAR_HPP"

    def test_plugins_subdirectory(self, tmp_path: Path) -> None:
        """plugins/python/module.hpp → PLUGINS_PYTHON_MODULE_HPP."""
        f = tmp_path / "plugins" / "python" / "module.hpp"
        assert M.compute_expected_guard(f, tmp_path) == "PLUGINS_PYTHON_MODULE_HPP"


# ---------------------------------------------------------------------------
# check_header_guard
# ---------------------------------------------------------------------------


class TestCheckHeaderGuard:
    """Tests for M.check_header_guard."""

    def test_correct_guard_returns_true(self, tmp_path: Path) -> None:
        """A file whose guard matches the expected macro is valid."""
        guard = "PHLEX_FOO_HPP"
        f = _make_header(tmp_path, "phlex", "foo.hpp", _correct_guard_content(guard))
        valid, expected = M.check_header_guard(f, tmp_path)
        assert valid is True
        assert expected is None

    def test_wrong_guard_returns_false(self, tmp_path: Path) -> None:
        """A file with a wrong guard name is invalid and returns the expected macro."""
        f = _make_header(tmp_path, "phlex", "foo.hpp", _wrong_guard_content("PHLEX_FOO_HPP"))
        valid, expected = M.check_header_guard(f, tmp_path)
        assert valid is False
        assert expected == "PHLEX_FOO_HPP"

    def test_too_short_file_is_valid(self, tmp_path: Path) -> None:
        """A file with fewer than 3 lines is treated as valid (no guard expected)."""
        f = _make_header(tmp_path, "phlex", "empty.hpp", "// no guard\n")
        valid, expected = M.check_header_guard(f, tmp_path)
        assert valid is True
        assert expected is None

    def test_no_ifndef_is_valid(self, tmp_path: Path) -> None:
        """A file without #ifndef is treated as valid (pragmatic approach)."""
        content = textwrap.dedent(
            """\
            #pragma once
            // header content
            void foo();
            """
        )
        f = _make_header(tmp_path, "phlex", "pragma.hpp", content)
        valid, expected = M.check_header_guard(f, tmp_path)
        assert valid is True

    def test_missing_endif_comment_makes_guard_invalid(self, tmp_path: Path) -> None:
        """If #endif has no comment, endif_macro is None and the guard is invalid."""
        content = textwrap.dedent(
            """\
            #ifndef PHLEX_FOO_HPP
            #define PHLEX_FOO_HPP

            void foo();

            #endif
            """
        )
        f = _make_header(tmp_path, "phlex", "foo.hpp", content)
        valid, _ = M.check_header_guard(f, tmp_path)
        # endif_macro is None, so ifndef==define==expected but endif_macro≠expected
        assert valid is False

    def test_mismatched_ifndef_and_define(self, tmp_path: Path) -> None:
        """When #ifndef and #define use different macros, the guard is invalid."""
        content = textwrap.dedent(
            """\
            #ifndef PHLEX_FOO_HPP
            #define PHLEX_BAR_HPP

            void foo();

            #endif // PHLEX_FOO_HPP
            """
        )
        f = _make_header(tmp_path, "phlex", "foo.hpp", content)
        valid, _ = M.check_header_guard(f, tmp_path)
        assert valid is False


# ---------------------------------------------------------------------------
# fix_header_guard
# ---------------------------------------------------------------------------


class TestFixHeaderGuard:
    """Tests for M.fix_header_guard."""

    def test_wrong_guard_is_fixed(self, tmp_path: Path) -> None:
        """A file with a wrong guard is updated to the expected macro."""
        f = _make_header(tmp_path, "phlex", "bar.hpp", _wrong_guard_content("PHLEX_BAR_HPP"))
        modified = M.fix_header_guard(f, tmp_path)
        assert modified is True
        content = f.read_text(encoding="utf-8")
        assert "#ifndef PHLEX_BAR_HPP" in content
        assert "#define PHLEX_BAR_HPP" in content
        assert "#endif // PHLEX_BAR_HPP" in content

    def test_correct_guard_not_modified(self, tmp_path: Path) -> None:
        """A file with the correct guard is not written."""
        guard = "PHLEX_BAR_HPP"
        original = _correct_guard_content(guard)
        f = _make_header(tmp_path, "phlex", "bar.hpp", original)
        modified = M.fix_header_guard(f, tmp_path)
        assert modified is False
        assert f.read_text(encoding="utf-8") == original

    def test_short_file_not_modified(self, tmp_path: Path) -> None:
        """A file with fewer than 3 lines is not touched."""
        f = _make_header(tmp_path, "phlex", "short.hpp", "// one line\n")
        assert M.fix_header_guard(f, tmp_path) is False

    def test_no_ifndef_not_modified(self, tmp_path: Path) -> None:
        """A file without a guard structure is not modified."""
        content = textwrap.dedent(
            """\
            #pragma once
            void foo();
            // end
            """
        )
        f = _make_header(tmp_path, "phlex", "prag.hpp", content)
        assert M.fix_header_guard(f, tmp_path) is False

    def test_only_ifndef_fixed(self, tmp_path: Path) -> None:
        """Only the #ifndef line is replaced when it has the wrong macro."""
        content = textwrap.dedent(
            """\
            #ifndef WRONG
            #define PHLEX_OK_HPP

            // body

            #endif // PHLEX_OK_HPP
            """
        )
        f = _make_header(tmp_path, "phlex", "ok.hpp", content)
        M.fix_header_guard(f, tmp_path)
        lines = f.read_text(encoding="utf-8").splitlines()
        assert lines[0] == "#ifndef PHLEX_OK_HPP"

    def test_endif_line_fixed(self, tmp_path: Path) -> None:
        """The #endif comment is updated when it has the wrong macro."""
        content = textwrap.dedent(
            """\
            #ifndef PHLEX_END_HPP
            #define PHLEX_END_HPP

            void foo();

            #endif // OLD_MACRO
            """
        )
        f = _make_header(tmp_path, "phlex", "end.hpp", content)
        M.fix_header_guard(f, tmp_path)
        last_line = f.read_text(encoding="utf-8").splitlines()[-1]
        assert last_line == "#endif // PHLEX_END_HPP"

    def test_fix_preserves_body_content(self, tmp_path: Path) -> None:
        """The body content between the guard and #endif is preserved unchanged."""
        content = textwrap.dedent(
            """\
            #ifndef WRONG_GUARD
            #define WRONG_GUARD

            class Foo {
              int x;
            };

            #endif // WRONG_GUARD
            """
        )
        f = _make_header(tmp_path, "phlex", "body.hpp", content)
        M.fix_header_guard(f, tmp_path)
        body = f.read_text(encoding="utf-8")
        assert "class Foo {" in body
        assert "  int x;" in body


# ---------------------------------------------------------------------------
# main() — integration tests
# ---------------------------------------------------------------------------


class TestMain:
    """Integration tests for M.main() (check and fix modes)."""

    # ------------------------------------------------------------------
    # Check mode
    # ------------------------------------------------------------------

    def test_check_mode_all_correct_returns_normally(
        self, tmp_path: Path, capsys: pytest.CaptureFixture[str]
    ) -> None:
        """All correct guards → main() returns without sys.exit."""
        guard = "PHLEX_GOOD_HPP"
        _make_header(tmp_path, "phlex", "good.hpp", _correct_guard_content(guard))
        argv = ["prog", "--check", "--root", str(tmp_path), str(tmp_path / "phlex")]
        with patch("sys.argv", argv):
            M.main()  # must not raise
        assert "All header guards are correct" in capsys.readouterr().out

    def test_check_mode_wrong_guard_exits_one(
        self, tmp_path: Path, capsys: pytest.CaptureFixture[str]
    ) -> None:
        """A file with a wrong guard causes main() to sys.exit(1)."""
        _make_header(tmp_path, "phlex", "bad.hpp", _wrong_guard_content("PHLEX_BAD_HPP"))
        argv = ["prog", "--check", "--root", str(tmp_path), str(tmp_path / "phlex")]
        with patch("sys.argv", argv):
            with pytest.raises(SystemExit) as exc_info:
                M.main()
        assert exc_info.value.code == 1
        out = capsys.readouterr().out
        assert "Found" in out
        assert "bad.hpp" in out

    def test_check_mode_reports_expected_guard(
        self, tmp_path: Path, capsys: pytest.CaptureFixture[str]
    ) -> None:
        """The expected guard name is shown in the check-mode failure message."""
        _make_header(tmp_path, "phlex", "thing.hpp", _wrong_guard_content("PHLEX_THING_HPP"))
        argv = ["prog", "--check", "--root", str(tmp_path), str(tmp_path / "phlex")]
        with patch("sys.argv", argv):
            with pytest.raises(SystemExit):
                M.main()
        assert "PHLEX_THING_HPP" in capsys.readouterr().out

    def test_check_mode_multiple_files(
        self, tmp_path: Path, capsys: pytest.CaptureFixture[str]
    ) -> None:
        """Multiple files are checked; only the bad ones are reported."""
        guard_a = "PHLEX_A_HPP"
        _make_header(tmp_path, "phlex", "a.hpp", _correct_guard_content(guard_a))
        _make_header(tmp_path, "phlex", "b.hpp", _wrong_guard_content("PHLEX_B_HPP"))
        argv = ["prog", "--check", "--root", str(tmp_path), str(tmp_path / "phlex")]
        with patch("sys.argv", argv):
            with pytest.raises(SystemExit) as exc_info:
                M.main()
        assert exc_info.value.code == 1
        out = capsys.readouterr().out
        assert "b.hpp" in out
        assert "a.hpp" not in out

    def test_check_mode_skips_non_header_files(
        self, tmp_path: Path, capsys: pytest.CaptureFixture[str]
    ) -> None:
        """Non-.hpp/.h files are skipped in directory scan mode."""
        phlex_dir = tmp_path / "phlex"
        phlex_dir.mkdir()
        (phlex_dir / "source.cpp").write_text("int x;\n", encoding="utf-8")
        with patch("sys.argv", ["prog", "--check", "--root", str(tmp_path), str(phlex_dir)]):
            M.main()  # must not raise — .cpp is not checked
        assert "All header guards are correct" in capsys.readouterr().out

    # ------------------------------------------------------------------
    # Fix mode
    # ------------------------------------------------------------------

    def test_fix_mode_fixes_wrong_guard(
        self, tmp_path: Path, capsys: pytest.CaptureFixture[str]
    ) -> None:
        """Fix mode rewrites the wrong guard and prints the file name."""
        f = _make_header(tmp_path, "phlex", "fix_me.hpp", _wrong_guard_content("PHLEX_FIX_ME_HPP"))
        with patch("sys.argv", ["prog", "--root", str(tmp_path), str(tmp_path / "phlex")]):
            M.main()
        content = f.read_text(encoding="utf-8")
        assert "#ifndef PHLEX_FIX_ME_HPP" in content
        assert "Fixed" in capsys.readouterr().out

    def test_fix_mode_nothing_to_fix(
        self, tmp_path: Path, capsys: pytest.CaptureFixture[str]
    ) -> None:
        """Fix mode prints 'No header guards needed fixing.' when nothing changed."""
        guard = "PHLEX_CLEAN_HPP"
        _make_header(tmp_path, "phlex", "clean.hpp", _correct_guard_content(guard))
        with patch("sys.argv", ["prog", "--root", str(tmp_path), str(tmp_path / "phlex")]):
            M.main()
        assert "No header guards needed fixing" in capsys.readouterr().out

    def test_fix_mode_multiple_directories(
        self, tmp_path: Path, capsys: pytest.CaptureFixture[str]
    ) -> None:
        """Multiple directory arguments are all processed."""
        f1 = _make_header(tmp_path, "phlex", "p.hpp", _wrong_guard_content("PHLEX_P_HPP"))
        f2 = _make_header(tmp_path, "plugins", "q.hpp", _wrong_guard_content("PLUGINS_Q_HPP"))
        with patch(
            "sys.argv",
            [
                "prog",
                "--root",
                str(tmp_path),
                str(tmp_path / "phlex"),
                str(tmp_path / "plugins"),
            ],
        ):
            M.main()
        assert "#ifndef PHLEX_P_HPP" in f1.read_text(encoding="utf-8")
        assert "#ifndef PLUGINS_Q_HPP" in f2.read_text(encoding="utf-8")

    def test_fix_mode_direct_file_argument(
        self, tmp_path: Path, capsys: pytest.CaptureFixture[str]
    ) -> None:
        """A direct file path argument (not a directory) is processed."""
        f = _make_header(tmp_path, "phlex", "direct.hpp", _wrong_guard_content("PHLEX_DIRECT_HPP"))
        with patch("sys.argv", ["prog", "--root", str(tmp_path), str(f)]):
            M.main()
        assert "#ifndef PHLEX_DIRECT_HPP" in f.read_text(encoding="utf-8")

    # ------------------------------------------------------------------
    # Workflow-level invocation tests (matching CI commands)
    # ------------------------------------------------------------------

    def test_workflow_check_invocation_all_correct(
        self, tmp_path: Path, capsys: pytest.CaptureFixture[str]
    ) -> None:
        """Simulate: python3 scripts/fix_header_guards.py --check --root . phlex plugins form."""
        for subdir, stem, ext in [
            ("phlex", "alpha", "hpp"),
            ("plugins", "beta", "hpp"),
            ("form", "gamma", "h"),
        ]:
            guard = f"{subdir.upper()}_{stem.upper()}_{ext.upper()}"
            _make_header(tmp_path, subdir, f"{stem}.{ext}", _correct_guard_content(guard))

        subdirs = [str(tmp_path / d) for d in ("phlex", "plugins", "form")]
        with patch("sys.argv", ["prog", "--check", "--root", str(tmp_path)] + subdirs):
            M.main()  # must not raise
        assert "All header guards are correct" in capsys.readouterr().out

    def test_workflow_fix_invocation(
        self, tmp_path: Path, capsys: pytest.CaptureFixture[str]
    ) -> None:
        """Simulate: python3 scripts/fix_header_guards.py --root . phlex plugins form."""
        bad_files = [
            ("phlex", "x.hpp", "PHLEX_X_HPP"),
            ("form", "y.h", "FORM_Y_H"),
        ]
        for subdir, fname, expected_guard in bad_files:
            _make_header(tmp_path, subdir, fname, _wrong_guard_content(expected_guard))

        plugins_dir = tmp_path / "plugins"
        plugins_dir.mkdir()
        subdirs = [str(tmp_path / d) for d in ("phlex", "plugins", "form")]
        with patch("sys.argv", ["prog", "--root", str(tmp_path)] + subdirs):
            M.main()
        out = capsys.readouterr().out
        assert "Fixed" in out
