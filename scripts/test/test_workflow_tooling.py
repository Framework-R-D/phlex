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
    """Keep workflow tool manifests and local hook inputs predictable."""

    def test_ci_tool_versions_are_pinned_in_manifests(self):
        """Require exact pins for tools installed directly by CI."""
        project = tomllib.loads((TOOLS / "pyproject.toml").read_text())
        npm = json.loads((TOOLS / "package.json").read_text())["devDependencies"]

        python_dependencies = [
            *project["project"]["dependencies"],
            *project["dependency-groups"]["lint"],
            *project["dependency-groups"]["format"],
        ]
        for name in ("uv", "PyYAML", "yamllint", "mypy", "ruff", "gersemi"):
            with self.subTest(name=name):
                self.assertTrue(
                    any(dependency.startswith(f"{name}==") for dependency in python_dependencies)
                )
        for name in ("prettier", "markdownlint-cli2"):
            with self.subTest(name=name):
                self.assertRegex(npm[name], r"^\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?$")

    def test_precommit_hooks_have_explicit_revisions(self):
        """Allow local hooks to update independently from the CI tool manifests."""
        hooks = yaml.safe_load((ROOT / ".pre-commit-config.yaml").read_text())["repos"]
        for entry in hooks:
            if entry["repo"] == "local":
                continue
            with self.subTest(repo=entry["repo"]):
                self.assertTrue(entry.get("rev"))
                self.assertNotIn(entry["rev"].lower(), {"main", "master", "head"})

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
                    self.assertEqual(
                        installs[0]["with"]["working-directory"], "${{ runner.temp }}"
                    )
                    self.assertNotIn("version", installs[0]["with"])
                    commands = "\n".join(step.get("run", "") for step in steps)
                    self.assertIn("--locked", commands)
                    self.assertIn("--no-build", commands)
                    self.assertIn("$RUNNER_TEMP/workflow-tools/.github/format-tools", commands)
                    self.assertNotIn("requirements-lint.txt", commands)
                    self.assertNotIn("uv tool install", commands)
                    checkout = next(
                        step
                        for step in steps
                        if step.get("with", {}).get("path") == "workflow-tools"
                    )
                    self.assertEqual(checkout["with"]["ref"], "${{ job.workflow_sha }}")
                    self.assertEqual(
                        checkout["with"]["repository"], "${{ job.workflow_repository }}"
                    )
                    self.assertIs(checkout["with"]["persist-credentials"], False)

    def test_tooling_checkout_is_relocated_before_target_checkout(self):
        """Keep trusted tooling outside the caller-controlled workspace."""
        workflows = (
            "python-check",
            "python-fix",
            "cmake-format-check",
            "cmake-format-fix",
            "markdown-fix",
            "yaml-fix",
        )
        for name in workflows:
            workflow = yaml.safe_load((ROOT / f".github/workflows/{name}.yaml").read_text())
            for job in workflow["jobs"].values():
                steps = job.get("steps", [])
                tooling = next(
                    (
                        index
                        for index, step in enumerate(steps)
                        if step.get("with", {}).get("path") == "workflow-tools"
                    ),
                    None,
                )
                if tooling is None:
                    continue
                target = next(
                    (
                        index
                        for index, step in enumerate(steps)
                        if step.get("with", {}).get("path")
                        == "${{ needs.setup.outputs.checkout_path }}"
                    ),
                    None,
                )
                if target is None:
                    continue
                relocated = next(
                    (
                        index
                        for index, step in enumerate(steps)
                        if step.get("name") == "Move trusted workflow tools outside the workspace"
                    ),
                    None,
                )
                self.assertIsNotNone(relocated, msg=name)
                if relocated is None:
                    continue
                self.assertLess(tooling, relocated, msg=name)
                self.assertLess(relocated, target, msg=name)
                checkout = steps[tooling]["with"]
                self.assertEqual(checkout["path"], "workflow-tools")
                self.assertEqual(checkout["repository"], "${{ job.workflow_repository }}")
                self.assertEqual(checkout["ref"], "${{ job.workflow_sha }}")
                self.assertEqual(
                    steps[relocated]["run"],
                    'mv "$GITHUB_WORKSPACE/workflow-tools" "$RUNNER_TEMP/workflow-tools"',
                )
                self.assertFalse(
                    any(step.get("name", "").startswith("Exclude ") for step in steps)
                )

    def test_node_installs_use_the_tool_checkout(self):
        """Keep npm ci anchored to the locked workflow-tool package."""
        for name in ("markdown-fix", "yaml-fix"):
            workflow = yaml.safe_load((ROOT / f".github/workflows/{name}.yaml").read_text())
            job = "apply_fixes" if name == "markdown-fix" else "apply_yaml_fixes"
            steps = workflow["jobs"][job]["steps"]
            install = next(step for step in steps if "npm ci" in step.get("run", ""))
            self.assertEqual(
                install["working-directory"],
                "${{ runner.temp }}/workflow-tools/.github/format-tools",
            )

    def test_clang_tidy_validator_is_outside_source_checkout(self):
        """Keep the trusted validator separate from the selected source tree."""
        workflow = yaml.safe_load((ROOT / ".github/workflows/clang-tidy-fix.yaml").read_text())
        steps = workflow["jobs"]["apply_tidy_fixes"]["steps"]
        tooling = next(
            index
            for index, step in enumerate(steps)
            if step.get("with", {}).get("path") == "workflow-tools"
        )
        relocated = next(
            index
            for index, step in enumerate(steps)
            if step.get("name") == "Move trusted workflow tools outside the workspace"
        )
        source = next(
            index
            for index, step in enumerate(steps)
            if step.get("name") == "Checkout reviewed source snapshot"
        )
        validation = next(
            step for step in steps if step.get("name") == "Validate and apply replacements"
        )
        self.assertLess(tooling, relocated)
        self.assertLess(relocated, source)
        self.assertIn(
            "$RUNNER_TEMP/workflow-tools/scripts/validate_clang_tidy_fixes.py", validation["run"]
        )

    def test_relevance_filter_covers_tooling_contract_inputs(self):
        """Run tooling checks when their workflow and version inputs change."""
        workflow = yaml.safe_load((ROOT / ".github/workflows/python-check.yaml").read_text())
        setup = next(
            step
            for step in workflow["jobs"]["setup"]["steps"]
            if step.get("uses", "").startswith("Framework-R-D/action-workflow-setup@")
        )
        globs = set(setup["with"]["include-globs"].splitlines())
        self.assertTrue(
            {
                ".github/workflows/**",
                ".github/actionlint.yaml",
                ".github/dependabot.yml",
                ".pre-commit-config.yaml",
            }.issubset(globs)
        )

    def test_actionlint_ignores_only_unsupported_workflow_identity_fields(self):
        """Keep actionlint's stale context schema exception narrowly scoped."""
        config = yaml.safe_load((ROOT / ".github/actionlint.yaml").read_text())
        ignores = config["paths"][".github/{workflows,actions}/**/*.y{a,}ml"]["ignore"]
        self.assertEqual(
            ignores,
            [
                'property "workflow_repository" is not defined in object type',
                'property "workflow_sha" is not defined in object type',
            ],
        )

    def test_ci_manifest_updates_are_grouped_and_hooks_update_weekly(self):
        """Group npm/uv manifests but keep pre-commit revisions independent."""
        config = yaml.safe_load((ROOT / ".github/dependabot.yml").read_text())
        self.assertEqual(
            config["multi-ecosystem-groups"]["format-tools"]["schedule"]["interval"],
            "weekly",
        )
        updates = {
            update["package-ecosystem"]: update
            for update in config["updates"]
            if update["package-ecosystem"] in {"npm", "uv", "pre-commit"}
        }
        self.assertEqual(set(updates), {"npm", "uv", "pre-commit"})
        for ecosystem in ("npm", "uv"):
            update = updates[ecosystem]
            self.assertEqual(update["multi-ecosystem-group"], "format-tools")
            self.assertEqual(update["patterns"], ["*"])
            self.assertNotIn("schedule", update)
        self.assertEqual(updates["pre-commit"]["schedule"]["interval"], "weekly")
        self.assertNotIn("multi-ecosystem-group", updates["pre-commit"])

    def test_lint_configuration_is_from_workflow_snapshot(self):
        """Do not load target-controlled plugins or lint configuration."""
        for name in ("python-check", "python-fix"):
            content = (ROOT / f".github/workflows/{name}.yaml").read_text()
            self.assertIn("$RUNNER_TEMP/workflow-tools/pyproject.toml", content)
            self.assertNotIn("$GITHUB_WORKSPACE/workflow-tools/pyproject.toml", content)

    def test_untrusted_formatter_workflows_disable_uv_cache(self):
        """Prevent source-processing jobs from writing reusable uv caches."""
        for name in ("python-fix", "cmake-format-fix"):
            workflow = yaml.safe_load((ROOT / f".github/workflows/{name}.yaml").read_text())
            uv_steps = [
                step
                for job in workflow["jobs"].values()
                for step in job.get("steps", [])
                if step.get("uses", "").startswith("astral-sh/setup-uv@")
            ]
            with self.subTest(workflow=name):
                self.assertEqual(len(uv_steps), 1)
                self.assertIs(uv_steps[0].get("with", {}).get("enable-cache"), False)

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
