"""Tests for the changed-file script test selector."""

from __future__ import annotations

from run_script_tests import _all_tests, select_tests


def test_no_changed_paths_selects_all_tests() -> None:
    """Run the full suite when no paths are supplied."""
    assert select_tests([]) == _all_tests()


def test_changed_test_selects_only_that_test() -> None:
    """Select a test directly when its own file changes."""
    assert select_tests(["scripts/test/test_verification.py"]) == [
        "scripts/test/test_verification.py"
    ]


def test_changed_code_selects_its_test() -> None:
    """Select only the test associated with changed source code."""
    assert select_tests(["scripts/clang_tidy_check_summary.py"]) == [
        "scripts/test/test_clang_tidy_check_summary.py"
    ]


def test_changed_ci_script_selects_its_code_test() -> None:
    """Select a CI script's direct test without unrelated suites."""
    assert select_tests(["ci/upgrade_repos.py"]) == [
        "scripts/test/test_upgrade_repos.py",
    ]


def test_changed_unmapped_ci_file_selects_all_tests() -> None:
    """Run all tests for CI dependencies absent from the explicit map."""
    assert select_tests(["ci/new_helper.py"]) == _all_tests()


def test_changed_static_dependencies_select_their_tests() -> None:
    """Select tests that read changed container or workflow assets."""
    assert select_tests([".devcontainer/devcontainer.json"]) == [
        "scripts/test/test_devcontainer_host.py"
    ]
    assert select_tests([".github/workflows/coverage.yaml"]) == [
        "scripts/test/test_workflow_cleanup.py"
    ]


def test_changed_shared_configuration_selects_all_tests() -> None:
    """Run all tests when shared Python or hook configuration changes."""
    assert select_tests(["pyproject.toml"]) == _all_tests()


def test_changed_unmapped_script_code_selects_all_tests() -> None:
    """Run all tests for script code not represented in the dependency map."""
    assert select_tests(["scripts/new_helper.py"]) == _all_tests()


def test_unrelated_file_selects_no_tests() -> None:
    """Skip this suite for paths outside its declared dependencies."""
    assert select_tests(["docs/design.md"]) == []
