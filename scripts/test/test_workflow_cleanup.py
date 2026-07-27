# -*- coding: utf-8 -*-
"""Tests for PR comment cleanup steps in GitHub workflows.

The workflows add a marker comment and a cleanup step that removes stale
comments which do not have replies. These tests verify that the expected
YAML structure and markers are present.
"""

import pathlib

import yaml

WORKFLOWS_DIR = pathlib.Path(__file__).resolve().parent.parent.parent / ".github" / "workflows"

EXPECTED = {
    "clang-tidy-report.yaml": {
        "marker": "## Clang-Tidy Check Results\\n\\n",
        "cleanup_step_name": "Cleanup old clang-tidy comments",
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
