#!/usr/bin/env python3
"""Check and fix header guards to enforce naming convention."""

import argparse
import re
import sys
from pathlib import Path


def compute_expected_guard(file_path: Path, root: Path) -> str:
    """Compute expected guard macro: X_Y_HEADER_EXT."""
    rel = file_path.relative_to(root)
    # X is the first subdirectory, Y is remaining path components
    parts = [rel.parts[0].upper().replace("-", "_")]
    if len(rel.parts) > 2:
        parts.extend(p.upper().replace("-", "_") for p in rel.parts[1:-1])
    parts.extend([rel.stem.upper().replace("-", "_"), rel.suffix[1:].upper()])
    return "_".join(parts)


def check_header_guard(file_path: Path, root: Path) -> tuple[bool, str | None]:
    """Check if header guard is correct. Returns (is_valid, expected_guard)."""
    content = file_path.read_text()
    lines = content.splitlines(keepends=True)

    if len(lines) < 3:
        return True, None

    expected = compute_expected_guard(file_path, root)

    ifndef_idx = define_idx = None
    ifndef_macro = define_macro = endif_macro = None

    for i in range(min(10, len(lines))):
        if m := re.match(r"#ifndef\s+(\w+)\s*$", lines[i]):
            ifndef_idx, ifndef_macro = i, m.group(1)
        elif m := re.match(r"#define\s+(\w+)\s*$", lines[i]):
            define_idx, define_macro = i, m.group(1)
            break

    for i in range(len(lines) - 1, -1, -1):
        line = lines[i].strip()
        if line.startswith("#endif"):
            if m := re.match(r"#endif\s*//\s*(\w+)\s*$", lines[i]):
                endif_macro = m.group(1)
            break

    if ifndef_idx is None or define_idx is None:
        return True, None

    if ifndef_macro == define_macro == endif_macro == expected:
        return True, None

    return False, expected


def fix_header_guard(file_path: Path, root: Path) -> bool:
    """Fix header guard. Returns True if modified."""
    content = file_path.read_text()
    lines = content.splitlines(keepends=True)

    if len(lines) < 3:
        return False

    expected = compute_expected_guard(file_path, root)

    ifndef_idx = define_idx = endif_idx = None

    for i in range(min(10, len(lines))):
        if re.match(r"#ifndef\s+\w+\s*$", lines[i]):
            ifndef_idx = i
        elif re.match(r"#define\s+\w+\s*$", lines[i]):
            define_idx = i
            break

    for i in range(len(lines) - 1, -1, -1):
        line = lines[i].strip()
        if line.startswith("#endif"):
            endif_idx = i
            break

    if ifndef_idx is None or define_idx is None:
        return False

    modified = False
    if lines[ifndef_idx] != f"#ifndef {expected}\n":
        lines[ifndef_idx] = f"#ifndef {expected}\n"
        modified = True
    if lines[define_idx] != f"#define {expected}\n":
        lines[define_idx] = f"#define {expected}\n"
        modified = True
    if endif_idx is not None and lines[endif_idx] != f"#endif // {expected}\n":
        lines[endif_idx] = f"#endif // {expected}\n"
        modified = True

    if modified:
        file_path.write_text("".join(lines))
    return modified


def main() -> None:
    """Check or fix header guards in C++ header files."""
    parser = argparse.ArgumentParser(description="Check/fix header guards")
    parser.add_argument("paths", nargs="+", help="Files or directories")
    parser.add_argument("--check", action="store_true", help="Check only")
    parser.add_argument("--root", type=Path, default=Path.cwd(), help="Root path")
    args = parser.parse_args()

    root = args.root.resolve()
    bad_files = []

    for arg in args.paths:
        path = Path(arg).resolve()
        files = [path] if path.is_file() else [*path.rglob("*.hpp"), *path.rglob("*.h")]
        for f in files:
            if f.suffix not in {".hpp", ".h"}:
                continue
            if args.check:
                valid, expected = check_header_guard(f, root)
                if not valid:
                    bad_files.append((f, expected))
            else:
                if fix_header_guard(f, root):
                    bad_files.append((f, None))

    if args.check:
        if bad_files:
            print(f"Found {len(bad_files)} files with incorrect guards:")
            for f, expected in bad_files:
                print(f"  {f.relative_to(root)}: expected {expected}")
            sys.exit(1)
        print("All header guards are correct.")
    else:
        if bad_files:
            print(f"Fixed {len(bad_files)} files:")
            for f, _ in bad_files:
                print(f"  {f.relative_to(root)}")
        else:
            print("No header guards needed fixing.")


if __name__ == "__main__":
    main()
