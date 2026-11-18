"""Check the shared workflow tooling contract without installing target dependencies."""

from __future__ import annotations

import json
import tomllib
import unittest
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parents[2]
TOOLS = ROOT / ".github/format-tools"


class WorkflowToolingTest(unittest.TestCase):
    """Keep updater inputs, installed tools, and hook versions aligned."""

    def test_hook_versions_match_manifests(self):
        """Require matching check and fix versions across tool ecosystems."""
        project = tomllib.loads((TOOLS / "pyproject.toml").read_text())
        npm = json.loads((TOOLS / "package.json").read_text())["devDependencies"]
        hooks = yaml.safe_load((ROOT / ".pre-commit-config.yaml").read_text())["repos"]
        versions = {
            entry["repo"].rsplit("/", 1)[-1]: entry["rev"].removeprefix("v")
            for entry in hooks
            if "rev" in entry
        }
        for name in ("mypy", "ruff"):
            dependency = next(
                value
                for value in project["dependency-groups"]["lint"]
                if value.startswith(name + "==")
            )
            hook = "mirrors-mypy" if name == "mypy" else "ruff-pre-commit"
            self.assertEqual(versions[hook], dependency.split("==")[1])
        self.assertEqual(
            versions["gersemi-pre-commit"],
            project["dependency-groups"]["format"][0].split("==")[1],
        )
        self.assertEqual(versions["mirrors-prettier"], npm["prettier"])
        self.assertEqual(versions["markdownlint-cli2"], npm["markdownlint-cli2"])

    def test_bootstrap_and_runtime_use_same_manifest(self):
        """Reject detached bootstrap pins and installs from target manifests."""
        for name in ("python-check", "python-fix", "cmake-format-check", "cmake-format-fix"):
            workflow = yaml.safe_load((ROOT / f".github/workflows/{name}.yaml").read_text())
            for job in workflow["jobs"].values():
                steps = job.get("steps", [])
                installs = [
                    step
                    for step in steps
                    if step.get("uses", "").startswith("astral-sh/setup-uv@")
                ]
                if not installs:
                    continue
                with self.subTest(workflow=name, job=job.get("name")):
                    self.assertEqual(
                        installs[0]["with"]["version-file"],
                        "workflow-tools/.github/format-tools/pyproject.toml",
                    )
                    self.assertNotIn("version", installs[0]["with"])
                    commands = "\n".join(step.get("run", "") for step in steps)
                    self.assertIn("--locked", commands)
                    self.assertIn("--no-build", commands)
                    self.assertNotIn("requirements-lint.txt", commands)
                    self.assertNotIn("uv tool install", commands)
                    checkout = next(
                        step
                        for step in steps
                        if step.get("with", {}).get("path") == "workflow-tools"
                    )
                    expected_ref = (
                        "${{ github.event.pull_request.base.sha || github.workflow_sha }}"
                        if name == "python-check"
                        else "${{ github.workflow_sha }}"
                    )
                    self.assertEqual(checkout["with"]["ref"], expected_ref)
                    self.assertEqual(checkout["with"]["repository"], "${{ github.repository }}")
                    self.assertIs(checkout["with"]["persist-credentials"], False)

    def test_lint_configuration_is_from_workflow_snapshot(self):
        """Do not load target-controlled plugins or lint configuration."""
        for name in ("python-check", "python-fix"):
            content = (ROOT / f".github/workflows/{name}.yaml").read_text()
            self.assertIn("$GITHUB_WORKSPACE/workflow-tools/pyproject.toml", content)

    def test_updater_owns_consumed_lock(self):
        """Verify that exact direct dependencies agree with the maintained lock."""
        updates = yaml.safe_load((ROOT / ".github/dependabot.yml").read_text())["updates"]
        self.assertTrue(
            any(
                update["package-ecosystem"] == "uv"
                and update["directory"] == "/.github/format-tools"
                for update in updates
            )
        )
        lock = tomllib.loads((TOOLS / "uv.lock").read_text())
        versions = {package["name"]: package["version"] for package in lock["package"]}
        project = tomllib.loads((TOOLS / "pyproject.toml").read_text())
        dependencies = project["project"]["dependencies"] + [
            dependency for group in project["dependency-groups"].values() for dependency in group
        ]
        for dependency in dependencies:
            if "==" in dependency:
                name, version = dependency.split("==")
                self.assertEqual(versions[name.lower().replace("_", "-")], version)
