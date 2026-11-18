"""Regression tests for the workflow gating checker."""

from __future__ import annotations

import shutil
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "scripts/check-gating-block.sh"
WORKFLOW = "pre-commit-fix.yaml"


@pytest.fixture
def workflows(tmp_path):
    """Copy workflow data without executing any workflow code."""
    directory = tmp_path / "workflows"
    shutil.copytree(ROOT / ".github/workflows", directory)
    return directory


def check(directory):
    """Execute the governance checker against a workflow directory."""
    return subprocess.run(
        ["bash", str(SCRIPT), str(directory)], capture_output=True, text=True, check=False
    )


def test_current_workflows_pass(workflows):
    """The comment-only workflow does not need reusable or manual entry points."""
    result = check(workflows)
    assert result.returncode == 0, result.stdout + result.stderr
    assert f"PASS: {WORKFLOW}" in result.stdout


@pytest.mark.parametrize(
    "phrase",
    [
        "github.event_name == 'issue_comment'",
        "github.event.issue.pull_request",
        'contains(fromJSON(\'["OWNER",',
        '"COLLABORATOR", "MEMBER"]\'), github.event.comment.author_association)',
        "startsWith(github.event.comment.body, "
        "format('@{0}bot pre-commit-fix', github.event.repository.name))",
        "startsWith(github.event.comment.body, "
        "format('@{0}bot format', github.event.repository.name))",
    ],
)
def test_rejects_missing_comment_gate_phrase(workflows, phrase):
    """Each required authorization and command fragment must remain present."""
    workflow = workflows / WORKFLOW
    content = workflow.read_text()
    assert phrase in content
    workflow.write_text(content.replace(phrase, "REMOVED"))
    result = check(workflows)
    assert result.returncode == 1, result.stdout + result.stderr
    assert f"FAIL: {WORKFLOW}" in result.stdout
    assert f"missing: {phrase}" in result.stdout


def test_rejects_missing_workflow(workflows):
    """Deleting the governed workflow fails rather than silently skipping it."""
    (workflows / WORKFLOW).unlink()
    result = check(workflows)
    assert result.returncode == 1, result.stdout + result.stderr
    assert f"ERROR: {workflows / WORKFLOW} not found" in result.stdout
