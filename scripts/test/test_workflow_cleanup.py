# -*- coding: utf-8 -*-
"""Tests for PR comment cleanup steps in GitHub workflows.

The workflows add a marker comment and a cleanup step that removes stale
comments which do not have replies. These tests verify that the expected
YAML structure and markers are present.
"""

import os
import pathlib
import subprocess
import sys

import pytest
import yaml

WORKFLOWS_DIR = pathlib.Path(__file__).resolve().parent.parent.parent / ".github" / "workflows"

EXPECTED = {
    "clang-tidy-report.yaml": {
        "marker": "<!-- phlex-clang-tidy-check -->",
        "cleanup_step_name": "Update clang-tidy PR comment",
    },
    "codeql-comment.yaml": {
        "marker": "<!-- codeql-alerts -->",
        "cleanup_step_name": "Cleanup old CodeQL comments",
    },
    "coverage.yaml": {
        "marker": "<!-- coverage-comment -->",
        "cleanup_step_name": "Cleanup old coverage comments",
    },
}


def _load_yaml(path: pathlib.Path) -> dict:
    """Load a YAML file and return its dict representation."""
    with path.open("r", encoding="utf-8") as f:
        return yaml.safe_load(f)


def test_workflow_cleanup_steps():
    """Assert each workflow defines the marker env var and a cleanup step.

    The test is lightweight and does not invoke any GitHub API. It simply
    inspects the static workflow definitions.
    """
    for wf_name, expectations in EXPECTED.items():
        wf_path = WORKFLOWS_DIR / wf_name
        assert wf_path.is_file(), f"Workflow {wf_name} not found"
        data = _load_yaml(wf_path)
        jobs = data.get("jobs", {})
        assert jobs, f"No jobs defined in {wf_name}"
        marker_found = False
        cleanup_found = False
        for job in jobs.values():
            for step in job.get("steps", []):
                # Check env vars for marker
                env = step.get("env", {})
                if any(expectations["marker"] in v for v in env.values()):
                    marker_found = True
                # For clang-tidy, the marker is in the script body
                if wf_name == "clang-tidy-report.yaml":
                    script = step.get("with", {}).get("script", "")
                    if expectations["marker"] in script:
                        marker_found = True
                if step.get("name", "") == expectations["cleanup_step_name"]:
                    cleanup_found = True
        assert marker_found, f"Marker {expectations['marker']} not present in {wf_name}"
        assert cleanup_found, (
            f"Cleanup step {expectations['cleanup_step_name']} not present in {wf_name}"
        )


def test_clang_tidy_marks_completed_only_after_tidy_succeeds():
    """Ensure an incomplete clang-tidy run cannot clear an existing report."""
    workflow = (WORKFLOWS_DIR / "clang-tidy-check.yaml").read_text(encoding="utf-8")
    tidy_failure = workflow.index('if [ "$TIDY_EXIT" -ne 0 ]')
    issues_found = workflow.index('elif [ "$DIFF_EXIT" -eq 1 ];')
    completed_marker = workflow.index(
        ': > "$BUILD_DIR/clang-tidy-analysis-complete.txt"', issues_found
    )

    assert tidy_failure < issues_found < completed_marker


@pytest.mark.parametrize("category", ["formatting", "validation", "tests"])
def test_pre_commit_coverage_is_ci_only(tmp_path, category):
    """Only the CI-generated script test hook collects coverage."""
    repo_dir = WORKFLOWS_DIR.parent.parent
    config_path = repo_dir / ".pre-commit-config.yaml"
    original_config = config_path.read_text(encoding="utf-8")
    action = _load_yaml(repo_dir / ".github" / "actions" / "run-pre-commit" / "action.yaml")
    step = next(step for step in action["runs"]["steps"] if step["name"] == "Select and run hooks")
    script = step["run"].split("<<'PY'\n", 1)[1].split("\nPY\n", 1)[0]
    generated_path = tmp_path / "pre-commit.yaml"
    result = subprocess.run(
        [sys.executable, "-c", script, str(generated_path)],
        cwd=repo_dir,
        env={**os.environ, "HOOK_CATEGORY": category},
        text=True,
        capture_output=True,
    )
    assert result.returncode == 0, result.stderr
    generated = _load_yaml(generated_path)
    hooks = {hook["id"]: hook for repo in generated["repos"] for hook in repo["hooks"]}
    if category == "tests":
        hook = hooks["scripts-unit-tests"]
        assert hook["entry"] == "pytest scripts/test/ -v"
        assert hook["additional_dependencies"] == ["pytest", "PyYAML", "pytest-cov"]
        assert hook["args"] == [
            "--cov=scripts",
            "--cov=ci",
            "--cov=.devcontainer",
            "--cov-report=xml:coverage-scripts.xml",
            "--cov-report=term-missing",
        ]
    else:
        assert "scripts-unit-tests" not in hooks
        assert all(
            "pytest-cov" not in hook.get("additional_dependencies", []) for hook in hooks.values()
        )
    assert config_path.read_text(encoding="utf-8") == original_config
    local_config = yaml.safe_load(original_config)
    local_hook = next(
        hook
        for repo in local_config["repos"]
        for hook in repo["hooks"]
        if hook["id"] == "scripts-unit-tests"
    )
    assert "pytest-cov" not in local_hook["additional_dependencies"]
    assert not local_hook.get("args")


def test_script_coverage_upload_uses_matrix_architecture():
    """Both script-test runners upload their generated report without gating on Codecov."""
    job = _load_yaml(WORKFLOWS_DIR / "pre-commit-validation.yaml")["jobs"]["scripts-test"]
    assert {entry["arch"] for entry in job["strategy"]["matrix"]["include"]} == {"amd64", "arm64"}
    steps = job["steps"]
    test_step = next(step for step in steps if step["name"] == "Run unit tests")
    assert test_step["with"]["category"] == "tests"
    upload = next(step for step in steps if step["name"] == "Upload scripts coverage to Codecov")
    assert steps.index(upload) > steps.index(test_step)
    assert upload["if"] == "${{ hashFiles('coverage-scripts.xml') != '' }}"
    assert upload["uses"].startswith("codecov/codecov-action@")
    assert upload["with"]["files"] == "coverage-scripts.xml"
    assert upload["with"]["flags"] == "${{ matrix.arch }}"
    assert upload["with"]["name"] == "phlex-scripts-coverage-${{ matrix.arch }}"
    assert upload["with"]["token"] == "${{ secrets.CODECOV_TOKEN }}"
    assert upload["with"]["fail_ci_if_error"] is False


@pytest.mark.parametrize(
    "coverage_result,available,expected_status",
    [
        ("success", ("amd64", "arm64"), 0),
        ("success", ("amd64",), 1),
        ("success", ("arm64",), 1),
        ("success", (), 1),
        ("failure", ("amd64",), 0),
        ("failure", ("arm64",), 0),
        ("failure", (), 1),
    ],
)
def test_coverage_upload_requires_reports_from_successful_matrix(
    tmp_path, coverage_result, available, expected_status
):
    """Missing successful reports fail collection without hiding available upload targets."""
    workflow = _load_yaml(WORKFLOWS_DIR / "coverage.yaml")
    steps = workflow["jobs"]["coverage-upload"]["steps"]
    downloads = [step for step in steps if step["name"].startswith("Download ")]
    assert len(downloads) == 2
    assert all(step["continue-on-error"] and step["if"] == "always()" for step in downloads)
    collector = next(step for step in steps if step.get("id") == "codecov_targets")
    assert collector["env"]["COVERAGE_RESULT"] == "${{ needs.coverage.result }}"
    for arch in available:
        report_dir = tmp_path / "coverage-artifacts" / f"coverage-generated-{arch}"
        report_dir.mkdir(parents=True)
        (report_dir / "coverage.xml").write_text("<coverage/>\n")
    output = tmp_path / "output"
    result = subprocess.run(
        ["bash", "-c", collector["run"]],
        cwd=tmp_path,
        env={**os.environ, "GITHUB_OUTPUT": str(output), "COVERAGE_RESULT": coverage_result},
        text=True,
        capture_output=True,
    )
    assert result.returncode == expected_status, result.stderr
    targets = dict(line.split("=", 1) for line in output.read_text().splitlines())
    for arch in ("amd64", "arm64"):
        assert targets[f"{arch}_found"] == str(arch in available).lower()
        if arch in available:
            assert targets[f"{arch}_files"].endswith("/coverage.xml")
    uploads = [step for step in steps if step["name"].endswith("coverage to Codecov")]
    assert all("always()" in step["if"] for step in uploads)
