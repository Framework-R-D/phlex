#!/usr/bin/env python3

"""Convert clang-tidy export-fixes YAML into compiler-style diagnostics.

The output format is compatible with VS Code problem matchers such as "$gcc":

  /abs/path/file.cpp:line:column: warning: message [check-name]

This script intentionally uses a lightweight line-based parser so it does not
depend on PyYAML.
"""

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass
from pathlib import Path


@dataclass
class Diagnostic:
    """Represents a single clang-tidy diagnostic."""

    check: str = "clang-tidy"
    message: str = ""
    level: str = "warning"
    file_path: str | None = None
    file_offset: int | None = None


def _strip_yaml_string(value: str) -> str:
    value = value.strip()
    if len(value) >= 2 and value[0] == "'" and value[-1] == "'":
        # YAML single-quoted escape sequence: '' -> '
        return value[1:-1].replace("''", "'")
    return value


def _parse_kv(line: str) -> tuple[str, str] | None:
    match = re.match(r"^\s*([^:]+):\s*(.*)$", line)
    if not match:
        return None
    return match.group(1).strip(), match.group(2).rstrip("\n")


def parse_clang_tidy_fixes(text: str) -> tuple[str | None, list[Diagnostic]]:
    """Parse a clang-tidy export-fixes YAML string into a list of diagnostics."""
    main_source_file: str | None = None
    diagnostics: list[Diagnostic] = []

    current: Diagnostic | None = None
    in_diag_message = False

    for raw_line in text.splitlines():
        line = raw_line.rstrip("\n")

        if line.startswith("MainSourceFile:"):
            kv = _parse_kv(line)
            if kv:
                main_source_file = _strip_yaml_string(kv[1])
            continue

        if re.match(r"^\s*-\s+DiagnosticName:\s+", line):
            if current is not None:
                diagnostics.append(current)
            check_name = re.sub(r"^\s*-\s+DiagnosticName:\s+", "", line)
            current = Diagnostic(check=_strip_yaml_string(check_name).strip())
            in_diag_message = False
            continue

        if current is None:
            continue

        if re.match(r"^\s*DiagnosticMessage:\s*$", line):
            in_diag_message = True
            continue

        if re.match(r"^\s*Notes:\s*$", line):
            # Notes are supplementary and do not define the primary location.
            in_diag_message = False
            continue

        kv = _parse_kv(line)
        if not kv:
            continue

        key, value = kv

        if in_diag_message:
            if key == "Message":
                current.message = _strip_yaml_string(value)
            elif key == "FilePath":
                current.file_path = _strip_yaml_string(value)
            elif key == "FileOffset":
                try:
                    current.file_offset = int(value.strip())
                except ValueError:
                    current.file_offset = None
            continue

        if key == "Level":
            level = _strip_yaml_string(value).strip().lower()
            if level:
                current.level = level

    if current is not None:
        diagnostics.append(current)

    return main_source_file, diagnostics


def apply_path_map(path: str, mappings: list[tuple[str, str]]) -> str:
    """Apply the first matching prefix mapping to translate a path."""
    normalized = path
    for old, new in mappings:
        if normalized.startswith(old):
            normalized = new + normalized[len(old) :]
            break
    return normalized


def offset_to_line_col(path: Path, offset: int) -> tuple[int, int]:
    """Convert a byte offset in a file to a (line, column) pair."""
    try:
        data = path.read_bytes()
    except OSError:
        return 1, 1

    if not data:
        return 1, 1

    bounded = max(0, min(offset, len(data)))
    line = data.count(b"\n", 0, bounded) + 1
    last_newline = data.rfind(b"\n", 0, bounded)
    if last_newline < 0:
        col = bounded + 1
    else:
        col = bounded - last_newline
    return line, max(col, 1)


def parse_path_map(items: list[str]) -> list[tuple[str, str]]:
    """Parse a list of OLD=NEW path mapping strings into (old, new) tuples."""
    mappings: list[tuple[str, str]] = []
    for item in items:
        if "=" not in item:
            raise ValueError(f"Invalid --path-map value '{item}'. Expected OLD=NEW.")
        old, new = item.split("=", 1)
        mappings.append((old, new))
    return mappings


def build_arg_parser() -> argparse.ArgumentParser:
    """Build and return the argument parser for this script."""
    parser = argparse.ArgumentParser(
        description="Convert clang-tidy export-fixes YAML into compiler-style diagnostics."
    )
    parser.add_argument("input", type=Path, help="Path to clang-tidy-fixes.yaml")
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        required=True,
        help="Output text file with compiler-style diagnostics",
    )
    parser.add_argument(
        "--workspace-root",
        type=Path,
        default=Path.cwd(),
        help="Workspace root used by default path mapping",
    )
    parser.add_argument(
        "--path-map",
        action="append",
        default=[],
        help="Path mapping in OLD=NEW form. Can be specified multiple times.",
    )
    return parser


def main() -> int:
    """Parse arguments, process the fixes YAML, and write compiler-style diagnostics."""
    args = build_arg_parser().parse_args()

    text = args.input.read_text(encoding="utf-8")
    main_source, diagnostics = parse_clang_tidy_fixes(text)

    default_mappings = [
        ("/__w/phlex/phlex/phlex-src", str(args.workspace_root.resolve())),
        ("/__w/phlex/phlex/phlex-build", str((args.workspace_root / "build").resolve())),
    ]
    extra_mappings = parse_path_map(args.path_map)
    mappings = extra_mappings + default_mappings

    lines: list[str] = []
    for diag in diagnostics:
        file_path = diag.file_path or main_source
        if not file_path:
            # Skip diagnostics with no usable location.
            continue

        mapped = apply_path_map(file_path, mappings)
        resolved = Path(mapped)

        offset = diag.file_offset if diag.file_offset is not None else 0
        line, col = offset_to_line_col(resolved, offset)

        message = diag.message or "clang-tidy diagnostic"
        check = diag.check or "clang-tidy"
        severity = diag.level if diag.level in {"error", "warning", "note"} else "warning"
        lines.append(f"{resolved}:{line}:{col}: {severity}: {message} [{check}]")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    output_text = "\n".join(lines)
    if output_text:
        output_text += "\n"
    args.output.write_text(output_text, encoding="utf-8")

    print(f"Wrote {len(lines)} diagnostics to {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
