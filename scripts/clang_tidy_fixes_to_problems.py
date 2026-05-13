#!/usr/bin/env python3
r"""Convert a clang-tidy export-fixes YAML into compiler-style diagnostics.

PURPOSE
-------
``run-clang-tidy`` (and ``clang-tidy --export-fixes``) produces a YAML file
that describes every diagnostic it found together with suggested fixes.  The
file is machine-readable but not useful for quickly locating issues in an
editor or CI log.

This script translates that YAML into the compact gcc/clang compiler output
format that editors, terminals, and problem-matcher tools understand::

    /abs/path/to/file.cpp:line:col: warning: message [check-name]

VS Code's built-in ``$gcc`` problem matcher picks up this format from the
terminal panel, turning every line into a clickable link that jumps straight
to the flagged source location.

TYPICAL WORKFLOWS
-----------------
**Local development — navigate to each issue in VS Code:**

    # 1. Generate the fixes file (source root as working directory so
    #    .clang-tidy is found)
    cd /path/to/phlex
    run-clang-tidy -p build -export-fixes build/fixes.yaml phlex/ form/

    # 2. Convert to compiler-style diagnostics
    python3 scripts/clang_tidy_fixes_to_problems.py build/fixes.yaml

    # VS Code's terminal panel now shows clickable source links.

**CI — translate paths from the GitHub Actions runner to the local workspace:**

    python3 scripts/clang_tidy_fixes_to_problems.py build/fixes.yaml \\
        --workspace-root /local/checkout \\
        --path-map /__w/phlex/phlex/phlex-src=/local/checkout \\
        -o problems.txt

PATH MAPPING
------------
clang-tidy records absolute paths as it sees them during the build.  When
fixes are generated in CI and then inspected locally, the paths embedded in
the YAML refer to the CI runner's filesystem (e.g. ``/__w/phlex/...``) and
will not resolve on your machine.

Use ``--path-map OLD=NEW`` (repeatable) to rewrite path prefixes before
emitting diagnostic lines.  The first matching mapping wins.  Two default
mappings for the standard Phlex CI layout are applied after any explicit
``--path-map`` entries::

    /__w/phlex/phlex/phlex-src  →  <workspace-root>
    /__w/phlex/phlex/phlex-build  →  <workspace-root>/build

EXTERNAL HEADERS
----------------
clang-tidy sometimes reports a diagnostic whose primary location is inside a
system or third-party header that does not exist on the local machine.  When
this happens the script looks at the ``Notes`` attached to the diagnostic for
an entry that traces back to source code inside the workspace — preferring
notes whose message begins with ``Calling '`` (the typical trace for
analyzer-style checks) — and redirects the reported location to that note.
The original external-header location is included in the message for context.

USAGE
-----
    # Read fixes.yaml, write gcc-style lines to stdout
    python3 scripts/clang_tidy_fixes_to_problems.py build/fixes.yaml

    # Read from stdin
    python3 scripts/clang_tidy_fixes_to_problems.py < build/fixes.yaml

    # Write to a file (parent directories are created automatically)
    python3 scripts/clang_tidy_fixes_to_problems.py build/fixes.yaml \\
        -o build/problems.txt

    # Translate CI paths to local paths
    python3 scripts/clang_tidy_fixes_to_problems.py build/fixes.yaml \\
        --path-map /__w/phlex/phlex/phlex-src=/home/user/phlex \\
        --workspace-root /home/user/phlex
"""

from __future__ import annotations

import argparse
import sys
from dataclasses import dataclass
from pathlib import Path

import yaml


@dataclass
class Diagnostic:
    """A single clang-tidy diagnostic with its primary source location."""

    check: str = "clang-tidy"
    message: str = ""
    level: str = "warning"
    file_path: str | None = None
    file_offset: int | None = None
    notes: list["DiagnosticNote"] | None = None


@dataclass
class DiagnosticNote:
    """A note attached to a :class:`Diagnostic`, providing extra context."""

    file_path: str | None = None
    file_offset: int | None = None
    message: str = ""


def parse_clang_tidy_fixes(text: str) -> tuple[str | None, list[Diagnostic]]:
    """Parse a clang-tidy export-fixes YAML string into structured diagnostics.

    Args:
        text: Raw YAML text from a ``clang-tidy-fixes.yaml`` file.

    Returns:
        A ``(main_source_file, diagnostics)`` tuple.  *main_source_file* is
        the ``MainSourceFile`` field from the YAML (``None`` when absent or
        empty).  *diagnostics* is an empty list when the input is malformed.
    """
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
    """Rewrite *path* by replacing the first matching prefix.

    Mappings are tried in order; the first whose *old* prefix matches the
    start of *path* is applied and no further mappings are checked.

    Args:
        path: The original path string (typically from the YAML).
        mappings: Ordered list of ``(old_prefix, new_prefix)`` pairs.

    Returns:
        The rewritten path, or the original *path* if no mapping matches.
    """
    for old, new in mappings:
        if path.startswith(old):
            return new + path[len(old) :]
    return path


def offset_to_line_col(path: Path, offset: int) -> tuple[int, int]:
    """Convert a byte offset within a file to a 1-based (line, column) pair.

    The offset is clamped to the file length so out-of-range values never
    raise.  Returns ``(1, 1)`` when the file cannot be read (e.g. it does not
    exist on the local machine).

    Args:
        path: Path to the source file to read.
        offset: Byte offset from the start of the file (0-based).

    Returns:
        ``(line, column)`` both 1-based.
    """
    try:
        data = path.read_bytes()
    except OSError:
        return 1, 1

    if not data:
        return 1, 1

    bounded = max(0, min(offset, len(data)))
    line = data.count(b"\n", 0, bounded) + 1
    last_newline = data.rfind(b"\n", 0, bounded)
    col = bounded + 1 if last_newline < 0 else bounded - last_newline
    return line, max(col, 1)


def parse_path_map(items: list[str]) -> list[tuple[str, str]]:
    """Parse ``--path-map`` argument values into ``(old, new)`` tuples.

    Args:
        items: List of strings in ``OLD=NEW`` form, as supplied on the
            command line via ``--path-map``.

    Returns:
        List of ``(old_prefix, new_prefix)`` tuples in the same order as
        *items*.

    Raises:
        ValueError: When any item does not contain ``=``.
    """
    mappings: list[tuple[str, str]] = []
    for item in items:
        if "=" not in item:
            raise ValueError(f"Invalid --path-map value '{item}'. Expected OLD=NEW.")
        old, new = item.split("=", 1)
        mappings.append((old, new))
    return mappings


def is_within_workspace(path: str, workspace_root: Path) -> bool:
    """Return ``True`` when *path* resolves to a location under *workspace_root*.

    Both paths are resolved (symlinks followed) before comparison so that
    symlinked coverage trees are handled correctly.  Returns ``False`` on any
    :class:`OSError` (e.g. a path that contains null bytes on some platforms).

    Args:
        path: An absolute path string to test.
        workspace_root: The root directory to test containment against.

    Returns:
        ``True`` if *path* is inside *workspace_root*, ``False`` otherwise.
    """
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
    """Pick the most useful in-workspace note for an out-of-workspace diagnostic.

    When a diagnostic's primary location is in an external header, its
    ``Notes`` may contain a trace back to workspace source code.  This
    function selects the best such note for use as a proxy location.

    Selection priority:

    1. The first note whose message starts with ``"Calling '"`` (the typical
       call-trace note emitted by clang-analyzer checks).
    2. The first workspace note, regardless of message content.

    Args:
        notes: The ``Notes`` list from a :class:`Diagnostic`.
        workspace_root: Root of the local workspace for containment testing.
        mappings: Optional path mappings applied to each note's file path
            before the containment test.

    Returns:
        The chosen :class:`DiagnosticNote`, or ``None`` when no workspace
        note exists.
    """
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
        prog="clang_tidy_fixes_to_problems.py",
        description=(
            "Convert a clang-tidy export-fixes YAML file into compiler-style "
            "diagnostics (gcc/clang format) for use in VS Code, terminals, "
            "and CI problem matchers."
        ),
        epilog=(
            "output format:\n"
            "  /path/to/file.cpp:line:col: warning: message [check-name]\n\n"
            "examples:\n"
            "  # Basic: read fixes, print to stdout\n"
            "  %(prog)s build/clang-tidy-fixes.yaml\n\n"
            "  # Translate CI paths to local paths\n"
            "  %(prog)s build/fixes.yaml \\\n"
            "      --path-map /__w/phlex/phlex/phlex-src=/home/user/phlex \\\n"
            "      --workspace-root /home/user/phlex \\\n"
            "      -o build/problems.txt"
        ),
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "input",
        type=Path,
        nargs="?",
        metavar="FIXES_YAML",
        help=(
            "Path to the clang-tidy export-fixes YAML (e.g. "
            "build/clang-tidy-fixes.yaml). Reads from stdin when omitted."
        ),
    )
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        metavar="FILE",
        help=(
            "Write compiler-style diagnostic lines to FILE instead of stdout. "
            "Parent directories are created automatically."
        ),
    )
    parser.add_argument(
        "--workspace-root",
        type=Path,
        default=Path.cwd(),
        metavar="DIR",
        help=(
            "Root of the local workspace, used to determine whether a "
            "diagnostic location is inside the project (default: current "
            "working directory). Also used as the target for the built-in "
            "CI path mapping."
        ),
    )
    parser.add_argument(
        "--path-map",
        action="append",
        default=[],
        metavar="OLD=NEW",
        help=(
            "Rewrite path prefix OLD to NEW before emitting diagnostic lines. "
            "May be specified multiple times; the first matching mapping wins. "
            "Applied before the built-in CI mappings "
            "(/__w/phlex/phlex/phlex-src → <workspace-root>, etc.)."
        ),
    )
    return parser


def main() -> int:
    """Parse arguments, convert the fixes YAML, and write gcc-style diagnostics.

    Returns:
        0 on success.  Non-zero return codes are not currently used; errors
        are printed to stderr and the affected diagnostic is skipped.
    """
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
            # No usable location — skip rather than emit a meaningless line.
            continue

        mapped = apply_path_map(file_path, mappings)
        offset = diag.file_offset if diag.file_offset is not None else 0

        # When the primary location is outside the workspace, try to redirect
        # to a workspace-local note so the link is navigable.
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
        print(f"Wrote {len(lines)} diagnostic(s) to {args.output}")
    else:
        sys.stdout.write(output_text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
