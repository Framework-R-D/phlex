"""Run markdownlint on tracked files without loading repository plugins."""

from __future__ import annotations

import os
import shutil
import subprocess
import tempfile
from pathlib import Path, PurePosixPath
from typing import Mapping


def _git_tracked_markdown(repository: Path, env: Mapping[str, str]) -> list[Path]:
    result = subprocess.run(
        ["git", "-c", "core.fsmonitor=false", "ls-files", "-z", "--", ":(glob)**/*.md"],
        cwd=repository,
        env=dict(env),
        capture_output=True,
        check=False,
    )
    if result.returncode:
        raise ValueError("Could not enumerate tracked Markdown files")
    paths = []
    for raw_path in result.stdout.split(b"\0"):
        if not raw_path:
            continue
        path = PurePosixPath(os.fsdecode(raw_path))
        if path.is_absolute() or ".." in path.parts or ".git" in path.parts:
            raise ValueError("Git returned an unsafe Markdown path")
        if path.name != "CHANGELOG.md":
            paths.append(Path(*path.parts))
    return paths


def _contains_symlink(repository: Path, relative_path: Path) -> bool:
    current = repository
    for part in relative_path.parts:
        current /= part
        if current.is_symlink():
            return True
    return False


def run_markdownlint_sandbox(
    repository: Path,
    trusted_config: Path,
    executable: Path,
    *,
    temporary_root: Path,
    inherited_path: str,
) -> list[str]:
    """Format tracked Markdown in isolation and copy only edited files back."""
    if repository.is_symlink() or trusted_config.is_symlink() or temporary_root.is_symlink():
        raise ValueError(
            "Repository, trusted configuration, and staging root must not be symlinks"
        )
    executable_is_symlink = executable.is_symlink()
    repository = repository.resolve(strict=True)
    trusted_config = trusted_config.resolve(strict=True)
    executable_parent = executable.parent.parent.resolve(strict=True)
    executable = executable.resolve(strict=True)
    temporary_root = temporary_root.resolve(strict=True)
    if (
        not repository.is_dir()
        or not trusted_config.is_file()
        or not executable.is_file()
        or not temporary_root.is_dir()
    ):
        raise ValueError("Invalid Markdown formatter paths")
    if executable_is_symlink and not executable.is_relative_to(executable_parent):
        raise ValueError("Markdown formatter symlinks must resolve inside trusted node_modules")

    with tempfile.TemporaryDirectory(
        prefix="phlex-markdownlint-home.", dir=temporary_root
    ) as temporary_home:
        env = {
            "PATH": inherited_path,
            "HOME": temporary_home,
            "GIT_CONFIG_GLOBAL": os.devnull,
            "GIT_CONFIG_NOSYSTEM": "1",
            "GIT_TERMINAL_PROMPT": "0",
            "GIT_OPTIONAL_LOCKS": "0",
            "LC_ALL": "C",
        }
        markdown_files = _git_tracked_markdown(repository, env)
        stage = Path(tempfile.mkdtemp(prefix="phlex-markdownlint-stage.", dir=temporary_root))
        shutil.copyfile(trusted_config, stage / ".markdownlint-cli2.yaml")
        staged_files: dict[Path, Path] = {}
        for relative_path in markdown_files:
            source = repository / relative_path
            if _contains_symlink(repository, relative_path) or not source.is_file():
                raise ValueError(f"Tracked Markdown path is not a regular file: {relative_path}")
            destination = stage / relative_path
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, destination)
            shutil.copymode(source, destination)
            staged_files[relative_path] = destination

        result = subprocess.run(
            [
                str(executable),
                "--config",
                str(stage / ".markdownlint-cli2.yaml"),
                "--fix",
                "**/*.md",
                "!**/CHANGELOG.md",
            ],
            cwd=stage,
            env=env,
            capture_output=True,
            check=False,
        )
        if result.returncode not in {0, 1}:
            raise ValueError("Isolated Markdown formatter failed")

        changed = []
        for relative_path, staged_path in staged_files.items():
            if staged_path.is_symlink() or not staged_path.is_file():
                raise ValueError(f"Markdown formatter replaced a regular file: {relative_path}")
            content = staged_path.read_bytes()
            original = repository / relative_path
            if _contains_symlink(repository, relative_path) or not original.is_file():
                raise ValueError(
                    f"Tracked Markdown path changed during formatting: {relative_path}"
                )
            if content != original.read_bytes():
                original.write_bytes(content)
                changed.append(relative_path.as_posix())
        return changed


def main() -> None:
    """Run the isolated formatter using workflow-provided trusted paths."""
    changed = run_markdownlint_sandbox(
        Path(os.environ["SOURCE_PATH"]),
        Path(os.environ["TRUSTED_CONFIG"]),
        Path(os.environ["MARKDOWNLINT_EXECUTABLE"]),
        temporary_root=Path(os.environ["RUNNER_TEMP"]),
        inherited_path=os.environ["PATH"],
    )
    print(f"Formatted {len(changed)} tracked Markdown files in isolated staging.")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, subprocess.SubprocessError):
        raise SystemExit("Isolated Markdown formatting failed closed") from None
