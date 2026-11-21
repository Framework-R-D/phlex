"""Create a provenance-bound patch artifact without changing the source branch."""

from __future__ import annotations

import json
import os
import re
import subprocess
import tempfile
from pathlib import Path
from typing import Mapping

MAX_PATCH_SIZE = 16 * 1024 * 1024
SHA_PATTERN = re.compile(r"[0-9a-f]{40}\Z")
REPOSITORY_PATTERN = re.compile(r"[A-Za-z0-9][A-Za-z0-9-]*/[A-Za-z0-9_.-]+\Z")
TOOL_PATTERN = re.compile(r"[a-z0-9-]{1,40}\Z")


def _run_git(repository: Path, args: list[str], env: Mapping[str, str]) -> bytes:
    result = subprocess.run(
        ["git", "-c", "core.fsmonitor=false", *args],
        cwd=repository,
        env=dict(env),
        capture_output=True,
        check=False,
    )
    if result.returncode:
        raise ValueError("Could not inspect the formatter checkout")
    return result.stdout


def create_fix_patch(repository: Path, settings: Mapping[str, str]) -> dict[str, str]:
    """Write only a bounded binary diff and provenance to a unique temp directory."""
    source_sha = settings.get("SOURCE_SHA", "")
    target_repo = settings.get("TARGET_REPO", "")
    target_ref = settings.get("TARGET_REF", "")
    tool = settings.get("TOOL", "")
    if SHA_PATTERN.fullmatch(source_sha) is None:
        raise ValueError("Expected a full approved source SHA")
    if REPOSITORY_PATTERN.fullmatch(target_repo) is None or target_repo.split("/")[1] in {
        ".",
        "..",
    }:
        raise ValueError("Invalid patch target repository")
    if (
        not target_ref
        or target_ref.startswith(("-", "refs/"))
        or any(ord(char) < 32 for char in target_ref)
    ):
        raise ValueError("Invalid patch target branch")
    if TOOL_PATTERN.fullmatch(tool) is None:
        raise ValueError("Invalid formatter label")

    if repository.is_symlink():
        raise ValueError("Formatter checkout must not be a symlink")
    repository = repository.resolve(strict=True)
    if not repository.is_dir():
        raise ValueError("Formatter checkout must be a directory")

    with tempfile.TemporaryDirectory(prefix="phlex-fix-patch-") as temporary_home:
        env = {
            "PATH": os.environ["PATH"],
            "HOME": temporary_home,
            "GIT_CONFIG_GLOBAL": os.devnull,
            "GIT_CONFIG_NOSYSTEM": "1",
            "GIT_TERMINAL_PROMPT": "0",
            "GIT_OPTIONAL_LOCKS": "0",
            "LC_ALL": "C",
        }
        head = (
            _run_git(repository, ["rev-parse", "--verify", "HEAD^{commit}"], env).decode().strip()
        )
        if head != source_sha:
            raise ValueError("Formatter checkout does not match the approved source SHA")
        _run_git(repository, ["check-ref-format", f"refs/heads/{target_ref}"], env)
        patch = _run_git(
            repository,
            [
                "--no-pager",
                "diff",
                "--binary",
                "--no-ext-diff",
                "--no-textconv",
                source_sha,
                "--",
                ".",
            ],
            env,
        )

    if len(patch) > MAX_PATCH_SIZE:
        raise ValueError("Formatter patch exceeds the artifact size limit")
    if not patch:
        return {
            "changes": "false",
            "pushed": "false",
            "commit_sha": "",
            "commit_sha_short": "",
            "patch_name": "",
        }

    artifact_root = Path(os.environ["RUNNER_TEMP"])
    if artifact_root.is_symlink() or not artifact_root.is_dir():
        raise ValueError("Runner temporary directory is invalid")
    artifact = Path(tempfile.mkdtemp(prefix=f"{tool}-patch.", dir=artifact_root))
    patch_name = f"{tool}.patch"
    (artifact / patch_name).write_bytes(patch)
    metadata = {
        "source_sha": source_sha,
        "target_repo": target_repo,
        "target_ref": target_ref,
        "workflow_ref": settings.get("WORKFLOW_REF", ""),
        "run_id": settings.get("RUN_ID", ""),
        "run_attempt": settings.get("RUN_ATTEMPT", ""),
        "tool": tool,
    }
    (artifact / "provenance.json").write_text(
        json.dumps(metadata, sort_keys=True) + "\n", encoding="utf-8"
    )
    return {
        "changes": "true",
        "pushed": "false",
        "commit_sha": "",
        "commit_sha_short": "",
        "patch_name": patch_name,
        "artifact_name": f"{tool}-patch-{metadata['run_attempt']}",
        "artifact_path": str(artifact),
    }


def main() -> None:
    """Publish only fixed-name outputs for the workflow artifact step."""
    outputs = create_fix_patch(Path(os.environ["SOURCE_PATH"]), os.environ)
    with Path(os.environ["GITHUB_OUTPUT"]).open("a", encoding="utf-8") as output:
        for name, value in outputs.items():
            output.write(f"{name}={value}\n")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, subprocess.SubprocessError):
        raise SystemExit(
            "Patch preparation failed closed; verify the source snapshot and artifact paths"
        ) from None
