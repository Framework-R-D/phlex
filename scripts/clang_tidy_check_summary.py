#!/usr/bin/env python3
"""Summarize clang-tidy diagnostics as a markdown checklist.

PURPOSE
-------
After running clang-tidy with ``-export-fixes`` (or ``run-clang-tidy``), you
have a YAML file listing every diagnostic the tool found.  This script reads
that file and produces a compact markdown task list — one line per *check
name* with its unique occurrence count — that you can paste directly into a
GitHub issue or PR comment to track remediation progress.

OUTPUT FORMAT
-------------
Each line is an unchecked GitHub-Flavored Markdown task item::

    - [ ] cert-err34-c (3)
    - [ ] modernize-use-nullptr (12)
    - [ ] readability-identifier-naming (47)

With ``--links`` each check name becomes a hyperlink to its clang-tidy
documentation page::

    - [ ] [cert-err34-c](https://clang.llvm.org/extra/clang-tidy/checks/cert/err34-c.html) (3)

UNIQUENESS
----------
Two diagnostic entries are considered the same occurrence if they share the
same (DiagnosticName, FilePath, FileOffset) triplet.  Duplicate triplets are
collapsed so that the reported count reflects distinct locations, not the
number of times the check fired per translation unit.

USAGE
-----
    # Read from a file, write to stdout
    python3 scripts/clang_tidy_check_summary.py build/clang-tidy-fixes.yaml

    # Read from stdin (e.g. piped from run-clang-tidy)
    run-clang-tidy ... | python3 scripts/clang_tidy_check_summary.py

    # Write to a file instead of stdout
    python3 scripts/clang_tidy_check_summary.py fixes.yaml -o summary.md

    # Add documentation hyperlinks for each check name
    python3 scripts/clang_tidy_check_summary.py fixes.yaml --links

    # Typical post-clang-tidy workflow
    run-clang-tidy -p build -export-fixes build/fixes.yaml phlex/ form/
    python3 scripts/clang_tidy_check_summary.py build/fixes.yaml --links -o summary.md
    cat summary.md   # paste into GitHub issue
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import yaml


def _load_diagnostics(path: Path | None) -> list[dict]:
    """Load the Diagnostics list from a clang-tidy YAML file or stdin.

    Args:
        path: Path to the clang-tidy YAML file, or ``None`` to read stdin.

    Returns:
        The list of diagnostic entry dicts, or an empty list on any error.
    """
    text = path.read_text(encoding="utf-8") if path is not None else sys.stdin.read()
    try:
        data = yaml.safe_load(text)
    except yaml.YAMLError as exc:
        print(f"Failed to parse clang-tidy YAML: {exc}", file=sys.stderr)
        return []
    if not isinstance(data, dict):
        return []
    diagnostics = data.get("Diagnostics")
    return diagnostics if isinstance(diagnostics, list) else []


def count_unique_diagnostics(diagnostics: list[dict]) -> dict[str, int]:
    """Count unique occurrences of each diagnostic check name.

    Uniqueness is determined by the (DiagnosticName, FilePath, FileOffset)
    triplet.  Identical triplets are counted only once, so the count reflects
    the number of distinct source locations flagged by each check rather than
    the number of translation units that reported it.

    Args:
        diagnostics: List of diagnostic entry dicts from the clang-tidy YAML.

    Returns:
        Mapping from check name to count of unique occurrences, suitable for
        passing directly to :func:`format_checklist`.
    """
    seen: set[tuple[str, str, int | None]] = set()
    counts: dict[str, int] = {}

    for entry in diagnostics:
        name = str(entry.get("DiagnosticName") or "clang-tidy")
        msg = entry.get("DiagnosticMessage") or {}
        file_path = str(msg.get("FilePath") or "")
        raw_offset = msg.get("FileOffset")
        file_offset: int | None = None
        try:
            if raw_offset is not None:
                file_offset = int(raw_offset)
        except (TypeError, ValueError):
            # Keep invalid/malformed offsets as None so processing remains robust.
            file_offset = None

        key = (name, file_path, file_offset)
        if key not in seen:
            seen.add(key)
            counts[name] = counts.get(name, 0) + 1

    return counts


def _check_url(name: str) -> str:
    """Derive the clang-tidy documentation URL for a check name.

    Args:
        name: A clang-tidy check name such as ``readability-identifier-naming``
            or ``clang-analyzer-core.NullDereference``.

    Returns:
        The canonical LLVM documentation URL for the check.
    """
    if name.startswith("clang-analyzer-"):
        rest = name[len("clang-analyzer-") :]
        return f"https://clang.llvm.org/extra/clang-tidy/checks/clang-analyzer/{rest}.html"
    if "-" in name:
        category, rest = name.split("-", 1)
        return f"https://clang.llvm.org/extra/clang-tidy/checks/{category}/{rest}.html"
    return f"https://clang.llvm.org/extra/clang-tidy/checks/{name}.html"


def format_checklist(counts: dict[str, int], *, links: bool = False) -> str:
    """Format diagnostic counts as a markdown task list.

    Entries are sorted alphabetically by check name.  Each entry is an
    unchecked GFM task-list item::

        - [ ] check-name (n)

    With ``links=True`` the check name becomes a Markdown hyperlink to the
    clang-tidy documentation page for that check.

    Args:
        counts: Mapping from check name to unique occurrence count, as
            returned by :func:`count_unique_diagnostics`.
        links: When ``True``, hyperlink each check name to its documentation.

    Returns:
        A newline-joined string of task-list items (no trailing newline).
        Returns an empty string when *counts* is empty.
    """
    lines = []
    for name, count in sorted(counts.items()):
        label = f"[{name}]({_check_url(name)})" if links else name
        lines.append(f"- [ ] {label} ({count})")
    return "\n".join(lines)


def build_arg_parser() -> argparse.ArgumentParser:
    """Build and return the argument parser for this script."""
    parser = argparse.ArgumentParser(
        prog="clang_tidy_check_summary.py",
        description=(
            "Summarize clang-tidy diagnostics as an alphabetically ordered "
            "markdown task list with unique occurrence counts."
        ),
        epilog=(
            "examples:\n"
            "  %(prog)s build/clang-tidy-fixes.yaml\n"
            "  %(prog)s build/fixes.yaml --links -o summary.md\n"
            "  run-clang-tidy -export-fixes fixes.yaml phlex/ && %(prog)s fixes.yaml"
        ),
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "input",
        type=Path,
        nargs="?",
        metavar="FIXES_YAML",
        help=(
            "Path to the clang-tidy export-fixes YAML file produced by "
            "run-clang-tidy or clang-tidy --export-fixes. "
            "Reads from stdin when omitted."
        ),
    )
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        metavar="FILE",
        help="Write the markdown checklist to FILE instead of stdout.",
    )
    parser.add_argument(
        "--links",
        action="store_true",
        default=False,
        help=("Hyperlink each check name to its clang-tidy documentation page on clang.llvm.org."),
    )
    return parser


def main() -> int:
    """Parse arguments and write the diagnostic checklist.

    Returns:
        0 on success (including when no diagnostics are found).
    """
    args = build_arg_parser().parse_args()

    diagnostics = _load_diagnostics(args.input)
    counts = count_unique_diagnostics(diagnostics)

    if not counts:
        print("No diagnostics found.", file=sys.stderr)
        return 0

    checklist = format_checklist(counts, links=args.links)

    if args.output is not None:
        args.output.write_text(checklist + "\n", encoding="utf-8")
    else:
        print(checklist)

    return 0


if __name__ == "__main__":
    sys.exit(main())
