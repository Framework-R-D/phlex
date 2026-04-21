#!/usr/bin/env python3

"""Convert clang-tidy export-fixes YAML into compiler-style diagnostics.

The output format is compatible with VS Code problem matchers such as "$gcc":

  /abs/path/file.cpp:line:column: warning: message [check-name]

"""

from __future__ import annotations

import argparse
import sys
from dataclasses import dataclass
from pathlib import Path

import yaml


@dataclass
class Diagnostic:
    """Represents a single clang-tidy diagnostic."""

    check: str = "clang-tidy"
    message: str = ""
    level: str = "warning"
    file_path: str | None = None
    file_offset: int | None = None
    notes: list["DiagnosticNote"] | None = None


@dataclass
class DiagnosticNote:
    """Represents a single clang-tidy note attached to a diagnostic."""

    file_path: str | None = None
    file_offset: int | None = None
    message: str = ""


def parse_clang_tidy_fixes(text: str) -> tuple[str | None, list[Diagnostic]]:
    """Parse a clang-tidy export-fixes YAML string into a list of diagnostics."""
    try:
        data = yaml.safe_load(text)
    except yaml.YAMLError as exc:
        print(f"Failed to parse clang-tidy fixes YAML: {exc}", file=sys.stderr)
        return None, []
    if not isinstance(data, dict):
        return None, []

    main_source_file: str | None = data.get("MainSourceFile") or None
    raw_diagnostics = data.get("Diagnostics") or []

    diagnostics: list[Diagnostic] = []
    for entry in raw_diagnostics:
        if not isinstance(entry, dict):
            continue

        msg = entry.get("DiagnosticMessage") or {}
        file_path = msg.get("FilePath") or None
        file_offset = msg.get("FileOffset")
        message = msg.get("Message") or ""

        check = str(entry.get("DiagnosticName") or "clang-tidy").strip() or "clang-tidy"
        level = str(entry.get("Level") or "warning").strip().lower() or "warning"

        parsed_file_offset: int | None = None
        if file_offset is not None:
            try:
                parsed_file_offset = int(file_offset)
            except (TypeError, ValueError):
                # Invalid or non-numeric offsets are treated as unavailable.
                parsed_file_offset = None

        notes: list[DiagnosticNote] = []
        for raw_note in entry.get("Notes") or []:
            if not isinstance(raw_note, dict):
                continue

            note_offset = raw_note.get("FileOffset")
            parsed_note_offset: int | None = None
            if note_offset is not None:
                try:
                    parsed_note_offset = int(note_offset)
                except (TypeError, ValueError):
                    parsed_note_offset = None

            notes.append(
                DiagnosticNote(
                    file_path=raw_note.get("FilePath") or None,
                    file_offset=parsed_note_offset,
                    message=raw_note.get("Message") or "",
                )
            )

        diagnostics.append(
            Diagnostic(
                check=check,
                message=message,
                level=level,
                file_path=file_path,
                file_offset=parsed_file_offset,
                notes=notes,
            )
        )

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


def is_within_workspace(path: str, workspace_root: Path) -> bool:
    """Return True when path resolves under workspace_root."""
    try:
        Path(path).resolve().relative_to(workspace_root.resolve())
    except ValueError:
        return False
    except OSError:
        return False
    return True


def choose_workspace_note(
    notes: list[DiagnosticNote],
    workspace_root: Path,
    mappings: list[tuple[str, str]] | None = None,
) -> DiagnosticNote | None:
    """Choose the most helpful in-workspace note for external diagnostics."""
    effective_mappings = mappings or []
    workspace_notes = [
        note
        for note in notes
        if note.file_path
        and is_within_workspace(apply_path_map(note.file_path, effective_mappings), workspace_root)
    ]
    if not workspace_notes:
        return None

    for note in workspace_notes:
        if note.message.startswith("Calling '"):
            return note

    return workspace_notes[0]


def build_arg_parser() -> argparse.ArgumentParser:
    """Build and return the argument parser for this script."""
    parser = argparse.ArgumentParser(
        description="Convert clang-tidy export-fixes YAML into compiler-style diagnostics."
    )
    parser.add_argument(
        "input",
        type=Path,
        nargs="?",
        help="Path to clang-tidy-fixes.yaml (default: stdin)",
    )
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        help="Output text file with compiler-style diagnostics (default: stdout)",
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

    if args.input is not None:
        text = args.input.read_text(encoding="utf-8")
    else:
        text = sys.stdin.read()
    main_source, diagnostics = parse_clang_tidy_fixes(text)

    default_mappings = [
        ("/__w/phlex/phlex/phlex-src", str(args.workspace_root.resolve())),
        ("/__w/phlex/phlex/phlex-build", str((args.workspace_root / "build").resolve())),
    ]
    extra_mappings = parse_path_map(args.path_map)
    mappings = extra_mappings + default_mappings
    workspace_root = args.workspace_root.resolve()

    lines: list[str] = []
    for diag in diagnostics:
        file_path = diag.file_path or main_source
        if not file_path:
            # Skip diagnostics with no usable location.
            continue

        mapped = apply_path_map(file_path, mappings)
        offset = diag.file_offset if diag.file_offset is not None else 0

        chosen_note = None
        original_location: tuple[str, int, int] | None = None
        if not is_within_workspace(mapped, workspace_root):
            chosen_note = choose_workspace_note(diag.notes or [], workspace_root, mappings)
            if chosen_note is not None:
                chosen_note_path = chosen_note.file_path
                if chosen_note_path is None:
                    chosen_note = None
                else:
                    original_resolved = Path(mapped)
                    original_location = (
                        str(original_resolved),
                        *offset_to_line_col(original_resolved, offset),
                    )
                    mapped = apply_path_map(chosen_note_path, mappings)
                    offset = chosen_note.file_offset if chosen_note.file_offset is not None else 0

        resolved = Path(mapped)
        line, col = offset_to_line_col(resolved, offset)

        message = diag.message or "clang-tidy diagnostic"
        if original_location is not None and chosen_note is not None:
            original_file, original_line, original_col = original_location
            message = (
                f"{message} (reported in external header at "
                f"{original_file}:{original_line}:{original_col}; "
                f"trace note: {chosen_note.message})"
            )
        check = diag.check or "clang-tidy"
        severity = diag.level if diag.level in {"error", "warning", "note"} else "warning"
        lines.append(f"{resolved}:{line}:{col}: {severity}: {message} [{check}]")

    output_text = "\n".join(lines)
    if output_text:
        output_text += "\n"

    if args.output is not None:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(output_text, encoding="utf-8")
        print(f"Wrote {len(lines)} diagnostics to {args.output}")
    else:
        sys.stdout.write(output_text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
