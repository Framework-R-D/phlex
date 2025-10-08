#!/usr/bin/env python3
"""Normalize LCOV coverage paths for editor tooling compatibility."""

from __future__ import annotations

import argparse
import sys
import os
from pathlib import Path


def _relative_subpath(path: Path, base: Path | None) -> Path | None:
    """Return *path* relative to *base* if it's contained within *base*.

    Attempts comparisons using both the provided paths and their resolved
    physical counterparts, falling back to :func:`os.path.relpath` while
    ensuring the result does not traverse outside *base*.
    """

    if base is None:
        return None

    try:
        return path.relative_to(base)
    except ValueError:
        pass

    try:
        base_resolved = base.resolve()
    except FileNotFoundError:
        base_resolved = base
    try:
        path_resolved = path.resolve()
    except FileNotFoundError:
        path_resolved = path
    try:
        return path_resolved.relative_to(base_resolved)
    except ValueError:
        pass

    rel_str = os.path.relpath(path, base)
    if not rel_str.startswith(".."):
        return Path(rel_str)

    rel_str = os.path.relpath(path_resolved, base_resolved)
    if rel_str.startswith(".."):
        return None
    return Path(rel_str)


def _is_repo_content(relative_path: Path) -> bool:
    """Return True when the path should remain in the coverage report."""

    parts = relative_path.parts
    if len(parts) >= 2 and parts[0] == "srcs" and parts[1] == "phlex":
        return True
    if len(parts) >= 2 and parts[0] == "build" and parts[1] == "phlex":
        return True
    if ".coverage-generated" in parts:
        return True
    return False


def normalize(
    report_path: Path,
    repo_root: Path,
    *,
    coverage_root: Path | None = None,
    coverage_alias: Path | None = None,
    absolute_paths: bool = False,
) -> tuple[list[str], list[str]]:
    """Rewrite SF entries inside an LCOV report to align with the repo layout.

    Args:
        report_path: Path to the LCOV file (coverage.info).
        repo_root: Root of the checked-out repository as seen by the editor.
        coverage_root: Directory used when lcov captured the report. Defaults
            to ``repo_root``. When relative paths are encountered in the
            report, they are interpreted relative to this directory.
        coverage_alias: Alternate path that should map to ``repo_root`` (for
            example, a symlinked project directory inside the workspace).
            When coverage tools resolve to the alias, paths are rewritten using
            the repository-relative form.
        absolute_paths: If ``True``, keep rewritten paths absolute (grounded in
            ``repo_root``). Otherwise, emit repository-relative paths.

    Returns:
        ``(missing, external)`` lists containing paths that could not be
        resolved inside the repository and paths that live outside the repo,
        respectively.
    """

    repo_root = repo_root.absolute()
    repo_root_physical = repo_root.resolve()
    coverage_root_path = (coverage_root or repo_root).absolute()
    coverage_root_physical = coverage_root_path.resolve()

    coverage_root_relative = _relative_subpath(coverage_root_path, repo_root)
    if coverage_root_relative is None:
        coverage_root_relative = _relative_subpath(
            coverage_root_physical, repo_root_physical
        )

    alias_symlink: Path | None = None
    alias_physical: Path | None = None
    alias_relative: Path | None = None
    if coverage_alias is not None:
        alias_symlink = coverage_alias.absolute()
        alias_physical = alias_symlink.resolve()
        alias_relative = _relative_subpath(alias_symlink, repo_root)
        if alias_relative is None:
            alias_relative = _relative_subpath(
                alias_physical, repo_root_physical
            )

    missing: list[str] = []
    external: list[str] = []

    lines = report_path.read_text().splitlines()
    rewritten: list[str] = []
    record: list[str] = []

    def flush_record(chunk: list[str]) -> None:
        if not chunk:
            return

        sf_index = next(
            (
                idx
                for idx, value in enumerate(chunk)
                if value.startswith("SF:")
            ),
            None,
        )
        if sf_index is None:
            rewritten.extend(chunk)
            return

        raw_path = chunk[sf_index][3:].strip()
        if not raw_path:
            rewritten.extend(chunk)
            return

        raw_path_obj = Path(raw_path)

        if raw_path_obj.is_absolute():
            primary = raw_path_obj
        else:
            primary = coverage_root_path / raw_path_obj

        candidates: list[Path] = []

        def _add_candidate(path: Path) -> None:
            if path not in candidates:
                candidates.append(path)

        _add_candidate(primary)
        try:
            resolved_primary = primary.resolve()
        except FileNotFoundError:
            resolved_primary = None
        except RuntimeError:
            resolved_primary = None
        if resolved_primary is not None:
            _add_candidate(resolved_primary)

        selected_candidate: Path | None = None
        relative: Path | None = None

        for candidate in candidates:
            relative = _relative_subpath(candidate, repo_root)
            if relative is None:
                relative = _relative_subpath(candidate, repo_root_physical)

            if relative is None and alias_symlink is not None:
                for base in (alias_symlink, alias_physical):
                    if base is None:
                        continue
                    subpath = _relative_subpath(candidate, base)
                    if subpath is None:
                        continue
                    if alias_relative is not None:
                        relative = alias_relative / subpath
                    else:
                        relative = subpath
                    break

            if (
                relative is None
                and coverage_root_relative is not None
            ):
                subpath = _relative_subpath(candidate, coverage_root_path)
                if subpath is None:
                    subpath = _relative_subpath(
                        candidate, coverage_root_physical
                    )
                if subpath is not None:
                    relative = coverage_root_relative / subpath

            if relative is not None:
                selected_candidate = candidate
                break

        if relative is None or selected_candidate is None:
            external.append(candidates[-1].as_posix())
            return

        if not _is_repo_content(relative):
            external.append((repo_root / relative).as_posix())
            return

        candidate_path = (repo_root / relative).resolve()
        if not candidate_path.exists():
            missing.append(relative.as_posix())

        if absolute_paths:
            new_path = candidate_path.as_posix()
        else:
            new_path = relative.as_posix()

        chunk[sf_index] = f"SF:{new_path}"
        rewritten.extend(chunk)

    for line in lines:
        record.append(line)
        if line == "end_of_record":
            flush_record(record)
            record = []

    # Handle any trailing lines without an end_of_record (unlikely but safe).
    flush_record(record)

    report_path.write_text("\n".join(rewritten) + "\n")
    return missing, external


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Normalize LCOV coverage files so that source file paths match "
            "the repository layout."
        )
    )
    parser.add_argument(
        "report",
        type=Path,
        help="Path to the LCOV report (e.g., coverage.info.final)",
    )
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=Path.cwd(),
        help="Repository root as visible to the editor (default: cwd)",
    )
    parser.add_argument(
        "--coverage-root",
        type=Path,
        default=None,
        help=(
            "Root directory used when the report was generated (default: repo "
            "root)"
        ),
    )
    parser.add_argument(
        "--coverage-alias",
        type=Path,
        default=None,
        help=(
            "Alternate path inside the workspace that should map to repo root"
        ),
    )
    parser.add_argument(
        "--absolute-paths",
        action="store_true",
        help=(
            "Emit absolute paths rooted at --repo-root instead of relative "
            "paths"
        ),
    )
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    report = args.report.resolve()
    repo_root = args.repo_root.absolute()
    coverage_root = (
        args.coverage_root.absolute() if args.coverage_root else repo_root
    )
    coverage_alias = (
        args.coverage_alias.absolute() if args.coverage_alias else None
    )

    missing, external = normalize(
        report,
        repo_root,
        coverage_root=coverage_root,
        coverage_alias=coverage_alias,
        absolute_paths=args.absolute_paths,
    )

    if external:
        unique_external = sorted(set(external))
        preview = "\n".join(f"  - {path}" for path in unique_external[:10])
        if len(unique_external) > 10:
            preview += f"\n  ... {len(unique_external) - 10} more"
        sys.stderr.write(
            "Removed LCOV records referencing files outside the repository "
            "root. Adjust lcov filters/excludes to avoid collecting them:\n"
            f"{preview}\n"
        )

    if missing:
        joined = "\n".join(f"  - {path}" for path in missing)
        sys.stderr.write(
            "LCOV report references files that do not exist inside the "
            "repository:\n"
            f"{joined}\n"
        )
        return 1

    print("LCOV coverage paths normalized successfully.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
