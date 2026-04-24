"""Tests for clang_tidy_diff_issues.py."""

from __future__ import annotations

import sys
from pathlib import Path

# Make the scripts directory importable.
sys.path.insert(0, str(Path(__file__).parent.parent))

from clang_tidy_diff_issues import filter_new_issues, parse_diff, parse_log  # noqa: E402


class TestParseDiff:
    """Tests for the parse_diff function."""

    def test_simple_addition(self) -> None:
        """Lines in the added hunk range are captured."""
        diff = (
            "--- a/foo.cpp\n"
            "+++ b/foo.cpp\n"
            "@@ -5,0 +6,3 @@\n"
            "+added line 1\n"
            "+added line 2\n"
            "+added line 3\n"
        )
        result = parse_diff(diff)
        assert result == {"foo.cpp": {6, 7, 8}}

    def test_hunk_count_omitted_defaults_to_one(self) -> None:
        """A missing hunk count defaults to 1."""
        diff = "--- a/bar.hpp\n+++ b/bar.hpp\n@@ -1 +1 @@\n+line\n"
        result = parse_diff(diff)
        assert result == {"bar.hpp": {1}}

    def test_pure_deletion_contributes_no_lines(self) -> None:
        """A hunk with zero new-file lines (pure deletion) adds nothing."""
        diff = "--- a/baz.cpp\n+++ b/baz.cpp\n@@ -3,2 +3,0 @@\n-removed\n-removed\n"
        result = parse_diff(diff)
        assert "baz.cpp" not in result or len(result.get("baz.cpp", set())) == 0

    def test_dev_null_new_file(self) -> None:
        """New files (--- /dev/null) have all their lines captured."""
        diff = "--- /dev/null\n+++ b/new.cpp\n@@ -0,0 +1,2 @@\n+line1\n+line2\n"
        result = parse_diff(diff)
        assert result == {"new.cpp": {1, 2}}

    def test_deleted_file_skipped(self) -> None:
        """Deleted files (+++ /dev/null) produce no added-line entries."""
        diff = "--- a/gone.cpp\n+++ /dev/null\n@@ -1,1 +0,0 @@\n-gone\n"
        result = parse_diff(diff)
        assert result == {}

    def test_multiple_files(self) -> None:
        """Added lines are tracked separately per file."""
        diff = (
            "--- a/a.cpp\n+++ b/a.cpp\n@@ -1 +1,2 @@\n+x\n+y\n"
            "--- a/b.hpp\n+++ b/b.hpp\n@@ -10 +10 @@\n+z\n"
        )
        result = parse_diff(diff)
        assert result == {"a.cpp": {1, 2}, "b.hpp": {10}}

    def test_empty_diff(self) -> None:
        """An empty diff produces an empty mapping."""
        assert parse_diff("") == {}


class TestParseLog:
    """Tests for the parse_log function."""

    def test_warning_extracted(self) -> None:
        """Warning lines are extracted with correct fields."""
        log = "/src/phlex/core/filter.cpp:42:5: warning: use auto [modernize-use-auto]\n"
        issues = parse_log(log)
        assert len(issues) == 1
        assert issues[0] == ("/src/phlex/core/filter.cpp", 42, "warning", "use auto")

    def test_error_extracted(self) -> None:
        """Error lines are extracted with level 'error'."""
        log = "/src/a.cpp:7:1: error: undefined variable [clang-diagnostic-error]\n"
        issues = parse_log(log)
        assert len(issues) == 1
        assert issues[0][2] == "error"

    def test_note_excluded(self) -> None:
        """Note lines are not returned (only warnings and errors are issues)."""
        log = "/src/a.cpp:7:1: note: see declaration here\n"
        assert parse_log(log) == []

    def test_runner_output_excluded(self) -> None:
        """run-clang-tidy runner output lines are not parsed as diagnostics."""
        log = "Running clang-tidy for /src/a.cpp\n2 warnings generated.\n"
        assert parse_log(log) == []

    def test_deduplication(self) -> None:
        """Identical diagnostics from multiple TUs are deduplicated."""
        line = "/src/a.cpp:10:3: warning: msg [check]\n"
        log = line * 3
        issues = parse_log(log)
        assert len(issues) == 1

    def test_multiple_issues(self) -> None:
        """Multiple distinct issues are all returned."""
        log = (
            "/src/a.cpp:1:1: warning: first [c1]\n"
            "/src/a.cpp:2:1: warning: second [c2]\n"
            "/src/b.hpp:5:1: error: third [c3]\n"
        )
        assert len(parse_log(log)) == 3


class TestFilterNewIssues:
    """Tests for the filter_new_issues function."""

    SOURCE_DIR = "/workspace/src"

    def _log(self, filepath: str, line: int, level: str = "warning") -> str:
        return f"{filepath}:{line}:1: {level}: msg [check]\n"

    def test_issue_on_added_line_reported(self) -> None:
        """An issue on an added line is included in the output."""
        source_file = f"{self.SOURCE_DIR}/phlex/core/a.cpp"
        diff = "--- a/phlex/core/a.cpp\n+++ b/phlex/core/a.cpp\n@@ -0,0 +1,5 @@\n+added\n" * 5
        log = self._log(source_file, 3)
        issues = filter_new_issues(log, diff, self.SOURCE_DIR)
        assert len(issues) == 1

    def test_issue_on_unchanged_line_not_reported(self) -> None:
        """An issue on a line not touched by the diff is excluded."""
        source_file = f"{self.SOURCE_DIR}/phlex/core/a.cpp"
        diff = "--- a/phlex/core/a.cpp\n+++ b/phlex/core/a.cpp\n@@ -10,0 +10,1 @@\n+new\n"
        # Issue is on line 5, but only line 10 was added.
        log = self._log(source_file, 5)
        issues = filter_new_issues(log, diff, self.SOURCE_DIR)
        assert issues == []

    def test_issue_in_external_header_excluded(self) -> None:
        """Issues in files outside the source directory are excluded."""
        diff = "--- a/phlex/core/a.cpp\n+++ b/phlex/core/a.cpp\n@@ -1 +1 @@\n+x\n"
        log = "/usr/include/some_header.h:1:1: warning: msg [check]\n"
        issues = filter_new_issues(log, diff, self.SOURCE_DIR)
        assert issues == []

    def test_empty_diff_returns_no_issues(self) -> None:
        """An empty diff always produces an empty result."""
        source_file = f"{self.SOURCE_DIR}/phlex/core/a.cpp"
        log = self._log(source_file, 1)
        issues = filter_new_issues(log, "", self.SOURCE_DIR)
        assert issues == []

    def test_empty_log_returns_no_issues(self) -> None:
        """An empty log always produces an empty result."""
        diff = "--- a/phlex/core/a.cpp\n+++ b/phlex/core/a.cpp\n@@ -1 +1 @@\n+x\n"
        issues = filter_new_issues("", diff, self.SOURCE_DIR)
        assert issues == []

    def test_multiple_new_issues(self) -> None:
        """Only issues on added lines are returned; others are excluded."""
        source_file = f"{self.SOURCE_DIR}/phlex/core/a.cpp"
        diff = "--- a/phlex/core/a.cpp\n+++ b/phlex/core/a.cpp\n@@ -1,0 +1,10 @@\n"
        log = (
            self._log(source_file, 2)
            + self._log(source_file, 7)
            + self._log(source_file, 15)  # outside diff range
        )
        issues = filter_new_issues(log, diff, self.SOURCE_DIR)
        lines = {ln for _, ln, _, _ in issues}
        assert lines == {2, 7}
