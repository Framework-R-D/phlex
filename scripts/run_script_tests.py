#!/usr/bin/env python3
"""Run the scripts unit tests affected by the changed files."""

from __future__ import annotations

import fnmatch
import sys
from collections.abc import Iterable
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
TEST_DIR = Path("scripts/test")

# These files affect test collection or the hook's ability to select tests.
GLOBAL_DEPENDENCIES = {
    ".pre-commit-config.yaml",
    "pyproject.toml",
    "pytest.ini",
    "scripts/run_script_tests.py",
    "scripts/test/__init__.py",
    "scripts/test/conftest.py",
}

# Static and subprocess dependencies are not represented by Python imports.
TEST_DEPENDENCIES = {
    "scripts/test/test_verification.py": ("scripts/verify.sh",),
    "scripts/test/test_fix_header_guards.py": (
        "scripts/fix_header_guards.py",
        ".github/workflows/header-guards-check.yaml",
        ".github/workflows/header-guards-fix.yaml",
    ),
    "scripts/test/test_codeql_reset_dismissed_alerts.py": (
        "scripts/codeql_reset_dismissed_alerts.py",
    ),
    "scripts/test/test_upgrade_repos.py": ("ci/upgrade_repos.py",),
    "scripts/test/test_clang_tidy_check_summary.py": ("scripts/clang_tidy_check_summary.py",),
    "scripts/test/test_promote_phlex_dependencies.py": ("ci/promote_phlex_dependencies.py",),
    "scripts/test/test_check_codeql_alerts.py": (
        "scripts/check_codeql_alerts.py",
        ".github/workflows/codeql-analysis.yaml",
    ),
    "scripts/test/test_sarif_alerts.py": ("scripts/sarif-alerts.py",),
    "scripts/test/test_clang_tidy_fixes_to_problems.py": (
        "scripts/clang_tidy_fixes_to_problems.py",
    ),
    "scripts/test/test_clang_tidy_diff_issues.py": ("scripts/clang_tidy_diff_issues.py",),
    "scripts/test/test_workflow_cleanup.py": (".github/workflows/**",),
    "scripts/test/test_coverage_scripts.py": (
        "scripts/create_coverage_symlinks.py",
        "scripts/export_llvm_lcov.py",
        "scripts/normalize_coverage_lcov.py",
        "scripts/normalize_coverage_xml.py",
    ),
    "scripts/test/test_devcontainer_host.py": (
        ".devcontainer/**",
        "ci/Dockerfile",
        "ci/entrypoint.sh",
        "ci/packages.yaml",
        "ci/spack.yaml",
        ".dockerignore",
        "scripts/setup-env.sh",
        "scripts/generate-image",
    ),
}


def _repo_path(path: str) -> str | None:
    """Return a changed path relative to the repository, or None if external."""
    candidate = Path(path)
    if candidate.is_absolute():
        try:
            candidate = candidate.resolve().relative_to(ROOT)
        except ValueError:
            return None
    return candidate.as_posix().removeprefix("./")


def _all_tests() -> list[str]:
    """Return the currently present script test modules."""
    return sorted(path.as_posix() for path in TEST_DIR.glob("test_*.py"))


def select_tests(changed_paths: Iterable[str]) -> list[str]:
    """Select tests whose test module or declared dependency changed."""
    paths = {_repo_path(path) for path in changed_paths}
    paths.discard(None)
    changed = {path for path in paths if path is not None}
    all_tests = _all_tests()
    if not changed:
        return all_tests
    if changed & GLOBAL_DEPENDENCIES:
        return all_tests

    selected: set[str] = set()
    for path in changed:
        if path in all_tests:
            selected.add(path)
        elif path.startswith(f"{TEST_DIR.as_posix()}/"):
            # A non-test helper under the test directory may be shared.
            return all_tests

        for test_path, dependencies in TEST_DEPENDENCIES.items():
            if any(fnmatch.fnmatchcase(path, pattern) for pattern in dependencies):
                selected.add(test_path)

        mapped = any(
            fnmatch.fnmatchcase(path, pattern)
            for dependencies in TEST_DEPENDENCIES.values()
            for pattern in dependencies
        )
        if path.startswith("ci/") and not mapped:
            return all_tests
        if (
            path.startswith("scripts/")
            and not path.startswith(f"{TEST_DIR.as_posix()}/")
            and path.endswith((".py", ".sh"))
            and not mapped
        ):
            # Run everything when new or unmapped script code changes.
            return all_tests

    return sorted(selected)


def main(changed_paths: Iterable[str] | None = None) -> int:
    """Run selected tests; no arguments means run the complete suite."""
    paths = list(sys.argv[1:] if changed_paths is None else changed_paths)
    tests = select_tests(paths)
    if not tests:
        print("No script unit tests are affected by the changed files.")
        return 0
    print(f"Running {len(tests)} affected script test module(s).")
    return int(pytest.main(["-v", *tests]))


if __name__ == "__main__":
    raise SystemExit(main())
