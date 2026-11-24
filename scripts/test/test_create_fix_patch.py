"""Tests for immutable, patch-only formatter output."""

from __future__ import annotations  # noqa: I001

import json
import os
import subprocess
from pathlib import Path

import pytest

from create_fix_patch import create_fix_patch


def git(repository: Path, *args: str) -> str:
    """Run Git with external signing configuration disabled."""
    env = {key: value for key, value in os.environ.items() if not key.startswith("GIT_")}
    env.update({"GIT_CONFIG_GLOBAL": os.devnull, "GIT_CONFIG_NOSYSTEM": "1"})
    result = subprocess.run(
        ["git", *args],
        cwd=repository,
        env=env,
        capture_output=True,
        check=True,
        text=True,
    )
    return result.stdout.strip()


@pytest.fixture
def checkout(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> tuple[Path, dict[str, str]]:
    """Create an isolated Git checkout with a valid approved snapshot."""
    for key in tuple(os.environ):
        if key.startswith("GIT_"):
            monkeypatch.delenv(key)
    monkeypatch.setenv("GIT_CONFIG_GLOBAL", os.devnull)
    monkeypatch.setenv("GIT_CONFIG_NOSYSTEM", "1")
    repository = tmp_path / "source"
    repository.mkdir()
    git(repository, "init", "--quiet")
    git(repository, "config", "core.hooksPath", os.devnull)
    git(repository, "config", "user.name", "Test")
    git(repository, "config", "user.email", "test@example.invalid")
    git(repository, "config", "commit.gpgsign", "false")
    (repository / "file.cpp").write_text("int value;\n", encoding="utf-8")
    git(repository, "add", "file.cpp")
    git(repository, "commit", "--quiet", "-m", "initial")
    source_sha = git(repository, "rev-parse", "HEAD")
    runner_temp = tmp_path / "runner-temp"
    runner_temp.mkdir()
    monkeypatch.setenv("RUNNER_TEMP", str(runner_temp))
    monkeypatch.setenv("PATH", os.environ["PATH"])
    settings = {
        "SOURCE_SHA": source_sha,
        "TARGET_REPO": "Framework-R-D/phlex",
        "TARGET_REF": "topic/security",
        "WORKFLOW_REF": (
            "Framework-R-D/phlex/.github/workflows/clang-format-fix.yaml@refs/heads/main"
        ),
        "RUN_ID": "1234",
        "RUN_ATTEMPT": "2",
        "TOOL": "clang-format",
    }
    return repository, settings


def test_writes_a_binary_patch_with_exact_provenance(
    checkout: tuple[Path, dict[str, str]],
) -> None:
    """Emits a patch and metadata without changing the checked-out commit."""
    repository, settings = checkout
    (repository / "file.cpp").write_text("int value{};\n", encoding="utf-8")

    result = create_fix_patch(repository, settings)

    assert result["changes"] == "true"
    assert result["pushed"] == "false"
    assert result["patch_name"] == "clang-format.patch"
    artifact = Path(result["artifact_path"])
    assert {path.name for path in artifact.iterdir()} == {"clang-format.patch", "provenance.json"}
    assert b"int value{};" in (artifact / result["patch_name"]).read_bytes()
    metadata = json.loads((artifact / "provenance.json").read_text(encoding="utf-8"))
    assert metadata["source_sha"] == settings["SOURCE_SHA"]
    assert metadata["target_ref"] == settings["TARGET_REF"]
    assert metadata["run_attempt"] == settings["RUN_ATTEMPT"]
    assert git(repository, "rev-parse", "HEAD") == settings["SOURCE_SHA"]


def test_no_change_does_not_create_an_artifact(checkout: tuple[Path, dict[str, str]]) -> None:
    """Avoids producing an artifact when the formatter changed no files."""
    repository, settings = checkout

    result = create_fix_patch(repository, settings)

    assert result == {
        "changes": "false",
        "pushed": "false",
        "commit_sha": "",
        "commit_sha_short": "",
        "patch_name": "",
    }


@pytest.mark.parametrize(
    ("key", "value"),
    [
        ("SOURCE_SHA", "a" * 39),
        ("SOURCE_SHA", "g" * 40),
        ("TARGET_REPO", "owner/name\nmalicious"),
        ("TARGET_REF", "refs/heads/main"),
        ("TARGET_REF", "main\nmalicious"),
        ("TOOL", "tool\nmalicious"),
    ],
)
def test_rejects_invalid_metadata(
    checkout: tuple[Path, dict[str, str]], key: str, value: str
) -> None:
    """Rejects malformed source, repository, branch, and formatter metadata."""
    repository, settings = checkout
    settings[key] = value

    with pytest.raises(ValueError):
        create_fix_patch(repository, settings)


def test_rejects_checkout_at_a_different_commit(checkout: tuple[Path, dict[str, str]]) -> None:
    """Rejects a checkout that moved after source approval."""
    repository, settings = checkout
    (repository / "file.cpp").write_text("changed\n", encoding="utf-8")
    git(repository, "add", "file.cpp")
    git(repository, "commit", "--quiet", "-m", "moved")

    with pytest.raises(ValueError, match="does not match"):
        create_fix_patch(repository, settings)


def test_external_diff_driver_cannot_execute(checkout: tuple[Path, dict[str, str]]) -> None:
    """Disables repository-configured external diff commands."""
    repository, settings = checkout
    marker = repository.parent / "external-diff-ran"
    (repository / ".gitattributes").write_text("*.cpp diff=spy\n", encoding="utf-8")
    (repository / "file.cpp").write_text("int value{};\n", encoding="utf-8")
    git(repository, "add", ".gitattributes")
    git(repository, "config", "diff.spy.command", f"touch {marker}")

    result = create_fix_patch(repository, settings)

    assert result["changes"] == "true"
    assert not marker.exists()
