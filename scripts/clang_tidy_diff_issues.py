#!/usr/bin/env python3
r"""Identify clang-tidy issues on lines added or modified by a patch.

Given a unified git diff file and a clang-tidy log file, reports any warning
or error whose primary location falls within lines that were added or modified
by the patch.

Usage:
    python3 clang_tidy_diff_issues.py \
        --log PATH/TO/clang-tidy.log \
        --diff PATH/TO/patch.diff \
        --source-dir PATH/TO/source/checkout \
        [--output PATH/TO/new-issues.txt]

Exit code:
    0  no new issues found
    1  at least one new issue found
    2  usage or file error
"""

from __future__ import annotations

import argparse
import re
import sys
from collections import defaultdict
from pathlib import Path

# Matches a clang-tidy warning or error line:
#   /abs/path/file.ext:LINE:COL: warning|error: message [check-name]
# Notes are intentionally excluded.
_ISSUE_RE = re.compile(
    r"^(/\S[^:]*\.(?:cpp|hpp|c|h|cxx|cc|hxx|icc)):(\d+):\d+:"
    r" (warning|error): (.+?)(?:\s+\[[\w,.\-]+\])?\s*$"
)


def parse_diff(diff_text: str) -> dict[str, set[int]]:
    """Parse a unified diff to extract added/changed line numbers per file.

    Keys are file paths relative to the repo root (without the leading ``b/``
    prefix from the diff header).  Values are sets of line numbers in the
    post-patch (new) file that were added or modified by the diff.

    Pure deletions (hunk count == 0 in the new file) contribute no lines.

    Args:
        diff_text: Contents of a unified ``git diff`` output.

    Returns:
        Mapping from repo-relative file path to set of new-file line numbers.
    """
    added: dict[str, set[int]] = defaultdict(set)
    current_file: str | None = None

    for line in diff_text.splitlines():
        if line.startswith("+++ b/"):
            current_file = line[6:]
        elif line.startswith("+++ "):
            # '/dev/null' or unexpected header: nothing to track for this file.
            current_file = None
        elif current_file is not None and line.startswith("@@ "):
            m = re.match(r"@@ -\d+(?:,\d+)? \+(\d+)(?:,(\d+))? @@", line)
            if m:
                new_start = int(m.group(1))
                count_str = m.group(2)
                new_count = int(count_str) if count_str is not None else 1
                for ln in range(new_start, new_start + new_count):
                    added[current_file].add(ln)

    return dict(added)


def parse_log(log_text: str) -> list[tuple[str, int, str, str]]:
    """Parse clang-tidy log output to extract warning and error locations.

    Extracts deduplicated ``(filepath, line, level, message)`` tuples for
    ``warning`` and ``error`` diagnostics.  ``note`` lines are excluded.

    Args:
        log_text: Full text of the ``clang-tidy.log`` file.

    Returns:
        Deduplicated list of ``(filepath, line, level, message)`` tuples.
    """
    seen: set[tuple[str, int, str, str]] = set()
    issues: list[tuple[str, int, str, str]] = []

    for line in log_text.splitlines():
        m = _ISSUE_RE.match(line)
        if not m:
            continue
        filepath = m.group(1)
        lineno = int(m.group(2))
        level = m.group(3)
        message = m.group(4).strip()
        key = (filepath, lineno, level, message)
        if key not in seen:
            seen.add(key)
            issues.append((filepath, lineno, level, message))

    return issues


def filter_new_issues(
    log_text: str,
    diff_text: str,
    source_dir: str,
) -> list[tuple[str, int, str, str]]:
    """Return log issues that fall on added or changed lines in the diff.

    Args:
        log_text: Full text of ``clang-tidy.log``.
        diff_text: Full text of a unified ``git diff``.
        source_dir: Absolute path to the source checkout root, used to convert
            absolute diagnostic file paths to repo-relative paths for matching
            against the diff.

    Returns:
        Subset of log issues whose (file, line) intersects the diff's added
        or modified line ranges.
    """
    added_lines = parse_diff(diff_text)
    if not added_lines:
        return []

    issues = parse_log(log_text)
    source_path = Path(source_dir).resolve()
    new_issues: list[tuple[str, int, str, str]] = []

    for filepath, lineno, level, message in issues:
        try:
            rel = str(Path(filepath).resolve().relative_to(source_path))
        except ValueError:
            # File is outside the source checkout (e.g., a system header). Skip.
            continue
        if rel in added_lines and lineno in added_lines[rel]:
            new_issues.append((filepath, lineno, level, message))

    return new_issues


def build_arg_parser() -> argparse.ArgumentParser:
    """Build and return the argument parser."""
    parser = argparse.ArgumentParser(
        description=(
            "Report clang-tidy warnings and errors that fall on lines"
            " added or modified by a patch."
        )
    )
    parser.add_argument(
        "--log",
        required=True,
        metavar="FILE",
        help="Path to the clang-tidy log file.",
    )
    parser.add_argument(
        "--diff",
        required=True,
        metavar="FILE",
        help="Path to the unified git diff file (e.g. produced by"
        " ``git diff --unified=0 BASE HEAD``).",
    )
    parser.add_argument(
        "--source-dir",
        required=True,
        metavar="DIR",
        help="Absolute path to the source checkout directory.",
    )
    parser.add_argument(
        "--output",
        metavar="FILE",
        help="Write new-issue lines to this file instead of stdout."
        " The file is always created (empty when no new issues are found).",
    )
    return parser


def main() -> int:
    """Parse arguments, filter issues, and report results."""
    args = build_arg_parser().parse_args()

    log_path = Path(args.log)
    diff_path = Path(args.diff)

    if not log_path.exists():
        print(f"Error: log file not found: {log_path}", file=sys.stderr)
        return 2
    if not diff_path.exists():
        print(f"Error: diff file not found: {diff_path}", file=sys.stderr)
        return 2

    log_text = log_path.read_text(encoding="utf-8", errors="replace")
    diff_text = diff_path.read_text(encoding="utf-8", errors="replace")

    new_issues = filter_new_issues(log_text, diff_text, args.source_dir)

    output_lines = [f"{fp}:{ln}: {lvl}: {msg}" for fp, ln, lvl, msg in new_issues]
    output_text = "\n".join(output_lines)

    if args.output:
        out_path = Path(args.output)
        out_path.write_text(
            output_text + "\n" if output_text else "",
            encoding="utf-8",
        )
    elif output_text:
        print(output_text)

    if new_issues:
        print(
            f"Found {len(new_issues)} new clang-tidy issue(s) on modified lines.",
            file=sys.stderr,
        )
        return 1

    print("No new clang-tidy issues on modified lines.", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
