"""Validate an untrusted formatting patch and create a commit without running target code."""

from __future__ import annotations

import os
import re
import subprocess
import sys
from pathlib import Path


def prepare_commit(repository: Path, patch: Path, sha: str) -> str:
    """Accept only content modifications to existing regular files at the captured SHA."""
    if not re.fullmatch(r"[0-9a-f]{40}", sha):
        raise ValueError("Invalid target SHA")
    if patch.is_symlink() or not patch.is_file() or patch.stat().st_size > 10 * 1024 * 1024:
        raise ValueError("Expected a regular patch file no larger than 10 MiB")

    def git(*args: str, input_data: bytes | None = None) -> bytes:
        return subprocess.run(
            ["git", "-c", "core.hooksPath=/dev/null", "-C", str(repository), *args],
            input=input_data,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=True,
            env={
                **os.environ,
                "GIT_AUTHOR_NAME": "github-actions[bot]",
                "GIT_AUTHOR_EMAIL": "41898282+github-actions[bot]@users.noreply.github.com",
                "GIT_COMMITTER_NAME": "github-actions[bot]",
                "GIT_COMMITTER_EMAIL": "41898282+github-actions[bot]@users.noreply.github.com",
            },
        ).stdout

    if git("rev-parse", "HEAD").decode().strip() != sha:
        raise ValueError("Checkout does not match the captured SHA")
    if git("status", "--porcelain"):
        raise ValueError("Publishing checkout must be clean")
    if patch.stat().st_size == 0:
        return ""
    # Apply only to the index: no checkout filters, hooks, or patched scripts execute.
    git("apply", "--cached", "--check", "--", str(patch.resolve()))
    git("apply", "--cached", "--", str(patch.resolve()))
    changes = git("diff", "--cached", "--raw", "-z", "--no-renames", sha, "--").split(b"\0")
    for metadata, path in zip(changes[0:-1:2], changes[1:-1:2], strict=True):
        old_mode, new_mode, _, _, status = metadata.split()
        if status != b"M" or old_mode[1:] not in {b"100644", b"100755"}:
            raise ValueError("Formatting may only modify existing regular files")
        if old_mode[1:] != new_mode:
            raise ValueError("Formatting may not change file modes")
        if any(part.lower() == b".git" for part in path.split(b"/")):
            raise ValueError("Formatting may not modify Git metadata")
    if not git("diff", "--cached", "--name-only", sha, "--"):
        return ""
    tree = git("write-tree").decode().strip()
    return (
        git("commit-tree", tree, "-p", sha, input_data=b"style: apply automatic formatting\n")
        .decode()
        .strip()
    )


if __name__ == "__main__":
    try:
        print(prepare_commit(Path(sys.argv[1]), Path(sys.argv[2]), sys.argv[3]))
    except (ValueError, subprocess.CalledProcessError) as error:
        print(f"Formatting patch rejected: {error}", file=sys.stderr)
        sys.exit(1)
