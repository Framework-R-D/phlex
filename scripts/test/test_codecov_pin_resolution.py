"""Check trusted Codecov CLI pin resolution in workflow steps."""

from __future__ import annotations

import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parents[2]


class CodecovPinResolutionTest(unittest.TestCase):
    """Exercise the inline version-resolution scripts from workflow YAML."""

    def test_missing_base_manifest_uses_verified_fallback(self):
        """Use the trusted pin when the base branch lacks the manifest."""
        for workflow in ("coverage", "python-check"):
            with self.subTest(workflow=workflow), tempfile.TemporaryDirectory() as temp_dir:
                result = self.run_version_step(workflow, Path(temp_dir))
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(result.stdout.strip(), "version=v11.3.2")

    def test_existing_manifest_version_takes_precedence(self):
        """Prefer the exact version from the trusted manifest when available."""
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_root = Path(temp_dir)
            manifest = temp_root / "codecov-tools/.github/codecov/requirements.txt"
            manifest.parent.mkdir(parents=True)
            manifest.write_text("codecov-cli==11.3.7\n", encoding="utf-8")

            result = self.run_version_step("coverage", temp_root)

        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.strip(), "version=v11.3.7")

    def test_invalid_existing_manifest_fails_closed(self):
        """Reject ranges rather than silently using an untrusted CLI version."""
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_root = Path(temp_dir)
            manifest = temp_root / "codecov-tools/.github/codecov/requirements.txt"
            manifest.parent.mkdir(parents=True)
            manifest.write_text("codecov-cli>=11.3\n", encoding="utf-8")

            result = self.run_version_step("python-check", temp_root)

        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Expected one exact codecov-cli version pin", result.stderr)

    def test_trusted_tooling_moves_before_source_checkout(self):
        """Keep Codecov tools outside caller-owned checkout paths."""
        for workflow, job_name in (
            ("coverage", "coverage-upload"),
            ("python-check", "scripts-coverage-upload"),
        ):
            with self.subTest(workflow=workflow):
                config = yaml.safe_load((ROOT / f".github/workflows/{workflow}.yaml").read_text())
                steps = config["jobs"][job_name]["steps"]
                checkout = next(
                    index
                    for index, step in enumerate(steps)
                    if step.get("with", {}).get("path") == "codecov-tools"
                )
                move = next(
                    index
                    for index, step in enumerate(steps)
                    if step.get("name") == "Move trusted Codecov tools outside the workspace"
                )
                source = next(
                    index
                    for index, step in enumerate(steps)
                    if step.get("name")
                    in {
                        "Check out source code for Codecov mapping",
                        "Check out exact source snapshot",
                    }
                )
                self.assertLess(checkout, move)
                self.assertLess(move, source)

    @staticmethod
    def run_version_step(workflow: str, cwd: Path) -> subprocess.CompletedProcess[str]:
        """Execute the embedded Codecov-version resolver in an isolated directory."""
        config = yaml.safe_load((ROOT / f".github/workflows/{workflow}.yaml").read_text())
        step = next(
            step
            for job in config["jobs"].values()
            for step in job.get("steps", [])
            if step.get("name") == "Resolve pinned Codecov CLI version"
        )
        script = step["run"].split("\n", 1)[1].rsplit("\nPY", 1)[0]
        environment = os.environ.copy()
        environment["RUNNER_TEMP"] = str(cwd)
        return subprocess.run(
            [sys.executable, "-c", script],
            cwd=cwd,
            check=False,
            capture_output=True,
            env=environment,
            text=True,
        )
