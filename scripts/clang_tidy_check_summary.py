#!/usr/bin/env python3
"""Summarize clang-tidy diagnostics as a markdown checklist.

Reads a clang-tidy export-fixes YAML file and writes an alphabetically ordered
markdown checklist of diagnostic check names with unique occurrence counts:

    - [ ] check-name (n)

A unique occurrence is determined by the (DiagnosticName, FilePath, FileOffset)
triplet; identical triplets are counted only once.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import yaml


def _load_diagnostics(path: Path | None) -> list[dict]:
    """Load the Diagnostics list from a clang-tidy YAML file or stdin."""
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
    triplet.  Identical triplets are counted only once.

    Args:
        diagnostics: List of diagnostic entries from the clang-tidy YAML.

    Returns:
        Mapping from check name to count of unique occurrences.
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
    """Derive the clang-tidy documentation URL for a check name."""
    if name.startswith("clang-analyzer-"):
        rest = name[len("clang-analyzer-") :]
        return f"https://clang.llvm.org/extra/clang-tidy/checks/clang-analyzer/{rest}.html"
    if "-" in name:
        category, rest = name.split("-", 1)
        return f"https://clang.llvm.org/extra/clang-tidy/checks/{category}/{rest}.html"
    return f"https://clang.llvm.org/extra/clang-tidy/checks/{name}.html"


def format_checklist(counts: dict[str, int], *, links: bool = False) -> str:
    """Format diagnostic counts as a markdown checklist.

    Args:
        counts: Mapping from check name to unique occurrence count.
        links: If True, hyperlink each check name to its documentation page.

    Returns:
        Markdown-formatted checklist string with entries alphabetically ordered.
    """
    lines = []
    for name, count in sorted(counts.items()):
        label = f"[{name}]({_check_url(name)})" if links else name
        lines.append(f"- [ ] {label} ({count})")
    return "\n".join(lines)


def build_arg_parser() -> argparse.ArgumentParser:
    """Build and return the argument parser."""
    parser = argparse.ArgumentParser(
        description=(
            "Summarize clang-tidy diagnostics as an alphabetically ordered"
            " markdown checklist with unique occurrence counts."
        )
    )
    parser.add_argument(
        "input",
        type=Path,
        nargs="?",
        help="Path to the clang-tidy YAML file (default: stdin)",
    )
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        help="Output file for the markdown checklist (default: stdout)",
    )
    parser.add_argument(
        "--links",
        action="store_true",
        default=False,
        help="Hyperlink each check name to its clang-tidy documentation page.",
    )
    return parser


def main() -> int:
    """Parse arguments and write the diagnostic checklist."""
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
