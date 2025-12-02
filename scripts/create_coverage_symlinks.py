#!/usr/bin/env python3
"""Create coverage symlinks."""
import argparse
import os
import pathlib
import shutil
import sys
from typing import Iterable

"""Populate the `.coverage-generated` directory with symlinks to generated sources.

This mirrors the helper previously implemented inside scripts/coverage.sh so that
CMake-driven coverage targets can prepare the mapping without relying on the
shell script.
"""

SUPPORTED_SUFFIXES = {
    ".c",
    ".C",
    ".cc",
    ".cpp",
    ".cxx",
    ".c++",
    ".icc",
    ".tcc",
    ".i",
    ".ii",
    ".h",
    ".H",
    ".hh",
    ".hpp",
    ".hxx",
    ".h++",
}


def should_link(path: pathlib.Path) -> bool:
    """Check if a path should be symlinked."""
    if not path.is_file():
        return False
    suffix = path.suffix
    if suffix in SUPPORTED_SUFFIXES:
        return True
    if path.name.endswith(".c++") or path.name.endswith(".h++"):
        return True
    if path.name.endswith(".icc") or path.name.endswith(".tcc"):
        return True
    if path.suffix in {".i", ".ii"}:
        return True
    return False


def iter_source_files(build_root: pathlib.Path) -> Iterable[pathlib.Path]:
    """Iterate over source files in a directory."""
    for root, _dirs, files in os.walk(build_root):
        root_path = pathlib.Path(root)
        for filename in files:
            file_path = root_path / filename
            if should_link(file_path):
                yield file_path


def create_symlinks(build_root: pathlib.Path, output_root: pathlib.Path) -> None:
    """Create symlinks for coverage."""
    if output_root.exists():
        shutil.rmtree(output_root)
    output_root.mkdir(parents=True, exist_ok=True)

    linked = 0
    for src in iter_source_files(build_root):
        try:
            rel = src.relative_to(build_root)
        except ValueError:
            rel = pathlib.Path(src.name)
        dest = output_root / rel
        dest.parent.mkdir(parents=True, exist_ok=True)
        if dest.is_symlink() or dest.exists():
            dest.unlink()
        dest.symlink_to(src)
        linked += 1

    print(f"[Coverage] Symlinked {linked} generated source files into {output_root}")


def parse_args(argv: Iterable[str]) -> argparse.Namespace:
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser(description="Create coverage symlink tree")
    parser.add_argument("--build-root", required=True)
    parser.add_argument("--output-root", required=True)
    return parser.parse_args(list(argv)[1:])


def main(argv: Iterable[str]) -> int:
    """Main entry point for the script."""
    args = parse_args(argv)
    build_root = pathlib.Path(args.build_root).resolve()
    output_root = pathlib.Path(args.output_root).resolve()

    if not build_root.exists():
        print(f"error: build root not found: {build_root}", file=sys.stderr)
        return 1

    create_symlinks(build_root, output_root)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
