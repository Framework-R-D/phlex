"""Tests for running Markdown formatting without repository configuration."""

from __future__ import annotations

import os
import subprocess
from pathlib import Path

import pytest

from run_markdownlint_sandbox import run_markdownlint_sandbox


@pytest.fixture
def sandbox(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> tuple[Path, Path, Path, Path]:
    """Build a Git checkout with hostile nested formatter configuration."""
    for key in tuple(os.environ):
        if key.startswith("GIT_"):
            monkeypatch.delenv(key)
    monkeypatch.setenv("GIT_CONFIG_GLOBAL", os.devnull)
    monkeypatch.setenv("GIT_CONFIG_NOSYSTEM", "1")
    repository = tmp_path / "source"
    repository.mkdir()
    env = os.environ.copy()

    def git(*args: str) -> None:
        subprocess.run(["git", *args], cwd=repository, env=env, check=True, capture_output=True)

    git("init", "--quiet")
    git("config", "core.hooksPath", os.devnull)
    git("config", "user.name", "Test")
    git("config", "user.email", "test@example.invalid")
    git("config", "commit.gpgsign", "false")
    (repository / "docs").mkdir()
    (repository / "docs/guide.md").write_text("before\n", encoding="utf-8")
    (repository / "docs/.markdownlint-cli2.mjs").write_text(
        "throw new Error('untrusted module was loaded');\n", encoding="utf-8"
    )
    (repository / ".markdownlint-cli2.cjs").write_text(
        "throw new Error('untrusted root module was loaded');\n", encoding="utf-8"
    )
    git("add", ".")
    git("commit", "--quiet", "-m", "initial")

    config = tmp_path / ".markdownlint-cli2.yaml"
    config.write_text("config: {}\n", encoding="utf-8")
    executable = tmp_path / "fake-markdownlint"
    executable.write_text(
        "#!/usr/bin/env python3\n"
        "from pathlib import Path\n"
        "assert not Path('docs/.markdownlint-cli2.mjs').exists()\n"
        "assert not Path('.markdownlint-cli2.cjs').exists()\n"
        "Path('docs/guide.md').write_text('after\\n', encoding='utf-8')\n",
        encoding="utf-8",
    )
    executable.chmod(0o755)
    temp_root = tmp_path / "runner-temp"
    temp_root.mkdir()
    return repository, config, executable, temp_root


def test_formats_only_tracked_markdown_without_source_configs(sandbox: tuple[Path, ...]) -> None:
    """Uses the trusted configuration and copies only the formatted file back."""
    repository, config, executable, temp_root = sandbox

    changed = run_markdownlint_sandbox(
        repository,
        config,
        executable,
        temporary_root=temp_root,
        inherited_path=os.environ["PATH"],
    )

    assert changed == ["docs/guide.md"]
    assert (repository / "docs/guide.md").read_text(encoding="utf-8") == "after\n"
    assert (repository / ".markdownlint-cli2.cjs").is_file()
    assert (repository / "docs/.markdownlint-cli2.mjs").is_file()


def test_refuses_a_symlinked_markdown_file(sandbox: tuple[Path, ...], tmp_path: Path) -> None:
    """Fails closed instead of following a tracked Markdown symlink."""
    repository, config, executable, temp_root = sandbox
    outside = tmp_path / "outside.md"
    outside.write_text("private\n", encoding="utf-8")
    target = repository / "docs/guide.md"
    target.unlink()
    target.symlink_to(outside)
    subprocess.run(
        ["git", "add", "docs/guide.md"],
        cwd=repository,
        check=True,
        capture_output=True,
        env=os.environ | {"GIT_CONFIG_GLOBAL": os.devnull, "GIT_CONFIG_NOSYSTEM": "1"},
    )

    with pytest.raises(ValueError, match="regular file"):
        run_markdownlint_sandbox(
            repository,
            config,
            executable,
            temporary_root=temp_root,
            inherited_path=os.environ["PATH"],
        )
