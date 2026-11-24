#!/usr/bin/env python3
"""Validate clang-tidy replacement paths before applying an artifact."""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path, PureWindowsPath
from typing import Any

import yaml

MAX_FIXES_BYTES = 32 * 1024 * 1024
MAX_REPLACEMENTS = 100_000
SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".icc"}


class FixValidationError(ValueError):
    """Raised when a fixes document is not safe for the intended checkout."""


def _tracked_source_path(value: Any, source_root: Path) -> tuple[Path, int]:
    """Resolve one replacement target and require a tracked C++ source file.

    Returns:
        The resolved path and its current byte size.
    """
    if not isinstance(value, str) or not value or "\x00" in value or "\\" in value:
        raise FixValidationError("replacement FilePath must be a non-empty POSIX path")

    path = Path(value)
    if not path.is_absolute() or ".." in path.parts or PureWindowsPath(value).drive:
        raise FixValidationError(f"replacement FilePath is not a safe absolute path: {value!r}")

    try:
        relative = path.relative_to(source_root)
        resolved = path.resolve(strict=True)
        resolved.relative_to(source_root)
    except (OSError, ValueError) as exc:
        raise FixValidationError(f"replacement target escapes or is missing: {value!r}") from exc

    current = source_root
    for component in relative.parts:
        current /= component
        if current.is_symlink():
            raise FixValidationError(f"replacement target traverses a symlink: {value!r}")

    if not resolved.is_file() or resolved.suffix.lower() not in SOURCE_SUFFIXES:
        raise FixValidationError(f"replacement target is not a C++ source file: {value!r}")

    relative_name = resolved.relative_to(source_root).as_posix()
    environment = os.environ | {
        "GIT_CONFIG_NOSYSTEM": "1",
        "GIT_CONFIG_GLOBAL": os.devnull,
        "GIT_OPTIONAL_LOCKS": "0",
    }
    result = subprocess.run(
        [
            "git",
            "--literal-pathspecs",
            "-C",
            str(source_root),
            "ls-files",
            "-z",
            "--error-unmatch",
            "--",
            relative_name,
        ],
        check=False,
        capture_output=True,
        env=environment,
    )
    if result.returncode != 0 or result.stdout != os.fsencode(relative_name) + b"\0":
        raise FixValidationError(f"replacement target is not tracked: {value!r}")
    return resolved, resolved.stat().st_size


def _replacement_entries(document: Any) -> list[dict[str, Any]]:
    """Find all replacement lists without following YAML alias cycles.

    Returns:
        All replacement mappings reachable from the document.
    """
    pending = [document]
    visited: set[int] = set()
    entries: list[dict[str, Any]] = []
    nodes = 0
    while pending:
        node = pending.pop()
        if isinstance(node, (dict, list)):
            identity = id(node)
            if identity in visited:
                continue
            visited.add(identity)
            nodes += 1
            if nodes > MAX_REPLACEMENTS * 4:
                raise FixValidationError("fixes YAML contains too many nested values")
        if isinstance(node, dict):
            if "Replacements" in node:
                raw_entries = node["Replacements"]
                if not isinstance(raw_entries, list):
                    raise FixValidationError("Replacements must be a sequence")
                for entry in raw_entries:
                    if not isinstance(entry, dict):
                        raise FixValidationError("each replacement must be a mapping")
                    entries.append(entry)
                    if len(entries) > MAX_REPLACEMENTS:
                        raise FixValidationError("fixes YAML contains too many replacements")
            pending.extend(node.values())
        elif isinstance(node, list):
            pending.extend(node)
        elif not isinstance(node, (str, int, float, bool, type(None))):
            raise FixValidationError("fixes YAML contains an unsupported value")
    return entries


def validate_and_copy(fixes_file: Path, source_root: Path, output_dir: Path) -> int:
    """Validate replacements and copy the accepted YAML into staging.

    Returns:
        The number of validated replacement entries.
    """
    source_root = source_root.resolve(strict=True)
    if fixes_file.is_symlink():
        raise FixValidationError("fixes file must not be a symlink")
    fixes_file = fixes_file.resolve(strict=True)
    if not fixes_file.is_file() or fixes_file.stat().st_size > MAX_FIXES_BYTES:
        raise FixValidationError("fixes file is missing, not regular, or exceeds the size limit")

    raw = fixes_file.read_bytes()
    try:
        documents = list(yaml.safe_load_all(raw))
    except (yaml.YAMLError, RecursionError) as exc:
        raise FixValidationError("fixes file is invalid or too deeply nested YAML") from exc

    if len(documents) > 1:
        raise FixValidationError("fixes file must contain at most one YAML document")

    replacements: list[dict[str, Any]] = []
    for document in documents:
        if not isinstance(document, dict):
            raise FixValidationError("each YAML document must be a mapping")
        diagnostics = document.get("Diagnostics")
        if not isinstance(diagnostics, list):
            raise FixValidationError("Diagnostics must be a sequence")
        for diagnostic in diagnostics:
            if (
                not isinstance(diagnostic, dict)
                or not isinstance(diagnostic.get("DiagnosticName"), str)
                or not isinstance(diagnostic.get("DiagnosticMessage"), dict)
                or not isinstance(diagnostic["DiagnosticMessage"].get("Message"), str)
            ):
                raise FixValidationError(
                    "each diagnostic requires DiagnosticName and DiagnosticMessage.Message"
                )
        replacements.extend(_replacement_entries(document))
        if len(replacements) > MAX_REPLACEMENTS:
            raise FixValidationError("fixes YAML contains too many replacements")

    for document in documents:
        main_source = document.get("MainSourceFile")
        if main_source:
            _tracked_source_path(main_source, source_root)

    for replacement in replacements:
        target, size = _tracked_source_path(replacement.get("FilePath"), source_root)
        offset = replacement.get("Offset")
        length = replacement.get("Length")
        replacement_text = replacement.get("ReplacementText")
        if type(offset) is not int or type(length) is not int or offset < 0 or length < 0:
            raise FixValidationError("replacement Offset and Length must be non-negative integers")
        if offset + length > size or not isinstance(replacement_text, str):
            raise FixValidationError(f"replacement range or text is invalid for {target}")

    if output_dir.is_symlink():
        raise FixValidationError("staging directory must be a real, empty directory")
    output_dir.mkdir(parents=True, exist_ok=True)
    if any(output_dir.iterdir()):
        raise FixValidationError("staging directory must be a real, empty directory")
    shutil.copyfile(fixes_file, output_dir / "clang-tidy-fixes.yaml")
    return len(replacements)


def main() -> int:
    """Validate and stage a fixes document, failing closed on any ambiguity.

    Returns:
        The process exit status.
    """
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixes_file", type=Path)
    parser.add_argument("--source-root", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    args = parser.parse_args()
    try:
        count = validate_and_copy(args.fixes_file, args.source_root, args.output_dir)
    except (FixValidationError, OSError, subprocess.SubprocessError) as exc:
        print(f"Unsafe clang-tidy replacements: {exc}", file=sys.stderr)
        return 1
    print(f"Validated {count} clang-tidy replacement(s)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
