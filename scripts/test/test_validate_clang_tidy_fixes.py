"""Adversarial path tests for clang-tidy replacement validation."""

from __future__ import annotations

import os
import subprocess
from pathlib import Path
from typing import Any

import pytest
import yaml

import validate_clang_tidy_fixes as M


@pytest.fixture(autouse=True)
def _isolate_git_environment(monkeypatch: pytest.MonkeyPatch) -> None:
    for key in tuple(os.environ):
        if key.startswith("GIT_"):
            monkeypatch.delenv(key)
    monkeypatch.setenv("GIT_CONFIG_GLOBAL", os.devnull)
    monkeypatch.setenv("GIT_CONFIG_NOSYSTEM", "1")


def _repository(tmp_path: Path) -> tuple[Path, Path]:
    root = tmp_path / "checkout"
    root.mkdir()
    source = root / "src" / "unit.cpp"
    source.parent.mkdir()
    source.write_text("int main() {}\n", encoding="utf-8")
    subprocess.run(["git", "init", "--quiet", str(root)], check=True)
    subprocess.run(["git", "-C", str(root), "config", "core.hooksPath", os.devnull], check=True)
    subprocess.run(["git", "-C", str(root), "add", "src/unit.cpp"], check=True)
    return root, source


def _fixes(path: str, **overrides: Any) -> dict[str, Any]:
    replacement = {
        "FilePath": path,
        "Offset": 0,
        "Length": 3,
        "ReplacementText": "long",
    }
    replacement.update(overrides)
    return {
        "MainSourceFile": path,
        "Diagnostics": [{"DiagnosticMessage": {"Replacements": [replacement]}}],
    }


def _write_fixes(tmp_path: Path, document: dict[str, Any]) -> Path:
    fixes = tmp_path / "fixes.yaml"
    fixes.write_text(yaml.safe_dump(document), encoding="utf-8")
    return fixes


def test_accepts_tracked_source_and_copies_only_fixes(tmp_path: Path) -> None:
    """Accept tracked C++ replacements and stage only their YAML source."""
    root, source = _repository(tmp_path)
    fixes = _write_fixes(tmp_path, _fixes(str(source)))
    output = tmp_path / "staging"

    assert M.validate_and_copy(fixes, root, output) == 1
    assert (output / "clang-tidy-fixes.yaml").read_bytes() == fixes.read_bytes()
    assert list(output.iterdir()) == [output / "clang-tidy-fixes.yaml"]


@pytest.mark.parametrize("target", ["../outside.cpp", "/tmp/outside.cpp", "C:\\runner\\state.cpp"])
def test_rejects_relative_traversal_external_and_windows_paths(
    tmp_path: Path, target: str
) -> None:
    """Reject paths that are relative, outside the checkout, or Windows-based."""
    root, _ = _repository(tmp_path)
    fixes = _write_fixes(tmp_path, _fixes(target))

    with pytest.raises(M.FixValidationError):
        M.validate_and_copy(fixes, root, tmp_path / "staging")


def test_rejects_symlink_escape(tmp_path: Path) -> None:
    """Reject symlinks that redirect a replacement outside the checkout."""
    root, _ = _repository(tmp_path)
    outside = tmp_path / "outside.cpp"
    outside.write_text("int outside;\n", encoding="utf-8")
    escape = root / "src" / "escape.cpp"
    escape.symlink_to(outside)
    fixes = _write_fixes(tmp_path, _fixes(str(escape)))

    with pytest.raises(M.FixValidationError, match="escapes|symlink"):
        M.validate_and_copy(fixes, root, tmp_path / "staging")


def test_rejects_untracked_or_non_cpp_target(tmp_path: Path) -> None:
    """Reject untracked files and tracked non-C++ files as replacement targets."""
    root, _ = _repository(tmp_path)
    untracked = root / "src" / "other.cpp"
    untracked.write_text("int other;\n", encoding="utf-8")
    fixes = _write_fixes(tmp_path, _fixes(str(untracked)))
    with pytest.raises(M.FixValidationError, match="not tracked"):
        M.validate_and_copy(fixes, root, tmp_path / "staging")

    header = root / "src" / "config.yaml"
    header.write_text("value: 1\n", encoding="utf-8")
    subprocess.run(["git", "-C", str(root), "add", "src/config.yaml"], check=True)
    fixes = _write_fixes(tmp_path, _fixes(str(header)))
    with pytest.raises(M.FixValidationError, match=r"not a C\+\+ source"):
        M.validate_and_copy(fixes, root, tmp_path / "staging")


def test_rejects_build_and_trusted_tooling_paths(tmp_path: Path) -> None:
    """Reject paths into build outputs or trusted workflow tooling."""
    root, _ = _repository(tmp_path)
    for target in ("/opt/phlex-build/generated.cpp", "/runner/work/phlex/workflow-tools/tool.cpp"):
        fixes = _write_fixes(tmp_path, _fixes(target))
        with pytest.raises(M.FixValidationError, match="escapes or is missing"):
            M.validate_and_copy(fixes, root, tmp_path / "staging")


def test_rejects_invalid_replacement_ranges(tmp_path: Path) -> None:
    """Reject offsets and lengths extending beyond the source file."""
    root, source = _repository(tmp_path)
    fixes = _write_fixes(tmp_path, _fixes(str(source), Offset=99, Length=3))

    with pytest.raises(M.FixValidationError, match="range or text"):
        M.validate_and_copy(fixes, root, tmp_path / "staging")


def test_rejects_yaml_python_object_tags(tmp_path: Path) -> None:
    """Reject YAML object constructors rather than evaluating tagged data."""
    root, source = _repository(tmp_path)
    fixes = tmp_path / "fixes.yaml"
    fixes.write_text("!!python/object/apply:os.system ['true']\n", encoding="utf-8")

    with pytest.raises(M.FixValidationError, match="invalid or too deeply nested"):
        M.validate_and_copy(fixes, root, tmp_path / "staging")


def test_rejects_nonempty_staging_directory(tmp_path: Path) -> None:
    """Reject staging directories that already contain another file."""
    root, source = _repository(tmp_path)
    fixes = _write_fixes(tmp_path, _fixes(str(source)))
    output = tmp_path / "staging"
    output.mkdir()
    (output / "unexpected.yaml").touch()

    with pytest.raises(M.FixValidationError, match="real, empty directory"):
        M.validate_and_copy(fixes, root, output)
