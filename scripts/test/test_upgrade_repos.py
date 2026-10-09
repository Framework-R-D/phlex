"""Regression tests for upgrading Spack repositories and package imports."""

from __future__ import annotations

import runpy
import sys
from pathlib import Path
from types import ModuleType, SimpleNamespace

import pytest
import yaml

UPGRADE_SCRIPT = Path(__file__).resolve().parents[2] / "ci/upgrade_repos.py"


def load_upgrade_script(monkeypatch: pytest.MonkeyPatch):
    """Load the CI script with a PyYAML-backed stand-in for Spack's ruamel API."""
    spack = ModuleType("spack")
    spack.__path__ = []
    repo_module = ModuleType("spack.repo")
    repo_module.__dict__["PATH"] = SimpleNamespace(repos=[])
    repo_module.__dict__["Repo"] = object
    spack.__dict__["repo"] = repo_module

    vendor = ModuleType("spack.vendor")
    vendor.__path__ = []
    ruamel = ModuleType("spack.vendor.ruamel")
    ruamel.__path__ = []
    yaml_module = ModuleType("spack.vendor.ruamel.yaml")

    class YAML:
        preserve_quotes = False

        @staticmethod
        def load(stream):
            with stream.open(encoding="utf-8") as source:
                return yaml.safe_load(source)

        @staticmethod
        def dump(data, stream):
            yaml.safe_dump(data, stream, sort_keys=False)

    yaml_module.__dict__["YAML"] = YAML
    monkeypatch.setitem(sys.modules, "spack", spack)
    monkeypatch.setitem(sys.modules, "spack.repo", repo_module)
    monkeypatch.setitem(sys.modules, "spack.vendor", vendor)
    monkeypatch.setitem(sys.modules, "spack.vendor.ruamel", ruamel)
    monkeypatch.setitem(sys.modules, "spack.vendor.ruamel.yaml", yaml_module)
    return runpy.run_path(str(UPGRADE_SCRIPT))


def make_repo(root: Path, namespace: str = "test") -> SimpleNamespace:
    """Create the minimal repository object expected by the migration helpers."""
    return SimpleNamespace(root=str(root), namespace=namespace)


def test_upgrade_repo_api_migrates_old_version_and_preserves_other_fields(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]
) -> None:
    """Upgrade legacy metadata without discarding unrelated repository fields."""
    upgrade = load_upgrade_script(monkeypatch)
    repo = make_repo(tmp_path, "custom")
    repo_yaml = tmp_path / "repo.yaml"
    repo_yaml.write_text('repo:\n  namespace: custom\n  api: "v2.0"\n', encoding="utf-8")

    upgrade["upgrade_spack_repo_api"](repo)

    result = yaml.safe_load(repo_yaml.read_text(encoding="utf-8"))
    assert result == {"repo": {"namespace": "custom", "api": "v2.2"}}
    assert "Upgrading custom from API v2.0 to v2.2" in capsys.readouterr().out


@pytest.mark.parametrize("api", ["v2.2", "v3.0"])
def test_upgrade_repo_api_leaves_current_and_newer_files_unchanged(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch, api: str
) -> None:
    """Avoid rewriting repositories already on the target or a newer API."""
    upgrade = load_upgrade_script(monkeypatch)
    repo_yaml = tmp_path / "repo.yaml"
    original = f"repo:\n  api: {api}\n  owner: preserved\n"
    repo_yaml.write_text(original, encoding="utf-8")

    upgrade["upgrade_spack_repo_api"](make_repo(tmp_path))

    assert repo_yaml.read_text(encoding="utf-8") == original


def test_upgrade_repo_api_ignores_missing_file_and_missing_repo_section(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Ignore repositories without a repo.yaml or a repo metadata section."""
    upgrade = load_upgrade_script(monkeypatch)
    upgrade["upgrade_spack_repo_api"](make_repo(tmp_path))

    repo_yaml = tmp_path / "repo.yaml"
    repo_yaml.write_text("metadata:\n  owner: local\n", encoding="utf-8")
    upgrade["upgrade_spack_repo_api"](make_repo(tmp_path))
    assert yaml.safe_load(repo_yaml.read_text(encoding="utf-8")) == {
        "metadata": {"owner": "local"}
    }


def test_upgrade_repo_api_reports_invalid_yaml_with_path_context(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Wrap parser failures with the path of the repository metadata file."""
    upgrade = load_upgrade_script(monkeypatch)
    repo_yaml = tmp_path / "repo.yaml"
    repo_yaml.write_text("repo: [unterminated", encoding="utf-8")

    with pytest.raises(RuntimeError, match="Error updating") as error:
        upgrade["upgrade_spack_repo_api"](make_repo(tmp_path))

    assert error.value.__cause__ is not None


def test_clean_package_imports_rewrites_nested_recipes_only(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]
) -> None:
    """Rewrite legacy imports in nested recipes, not adjacent Python or docs."""
    upgrade = load_upgrade_script(monkeypatch)
    packages = tmp_path / "packages"
    recipe = packages / "group" / "package.py"
    recipe.parent.mkdir(parents=True)
    recipe.write_text(
        "import llnl.util.filesystem as filesystem\n"
        "from llnl.util.filesystem import working_dir\n"
        "from spack.llnl.util import filesystem\n"
        "from spack.llnl.util.filesystem import mkdirp\n"
        "import spack.llnl.util.filesystem as legacy\n",
        encoding="utf-8",
    )
    unrelated_python = packages / "group" / "helper.py"
    unrelated_python.write_text("import llnl.util.filesystem as filesystem\n", encoding="utf-8")
    documentation = packages / "group" / "README.md"
    documentation.write_text("import llnl.util.filesystem as filesystem\n", encoding="utf-8")

    upgrade["clean_package_imports"](make_repo(tmp_path, "custom"))

    assert recipe.read_text(encoding="utf-8") == (
        "import spack.util.filesystem as filesystem\n"
        "from spack.util.filesystem import working_dir\n"
        "import spack.util.filesystem as filesystem\n"
        "from spack.util.filesystem import mkdirp\n"
        "import spack.util.filesystem as legacy\n"
    )
    assert (
        unrelated_python.read_text(encoding="utf-8")
        == "import llnl.util.filesystem as filesystem\n"
    )
    assert (
        documentation.read_text(encoding="utf-8") == "import llnl.util.filesystem as filesystem\n"
    )
    output = capsys.readouterr().out
    assert "group/package.py" in output
    assert "Successfully patched 1 package recipes." in output


def test_clean_package_imports_does_not_rewrite_already_clean_recipe(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Leave an already-canonical recipe byte-for-byte unchanged."""
    upgrade = load_upgrade_script(monkeypatch)
    packages = tmp_path / "packages"
    packages.mkdir()
    recipe = packages / "package.py"
    original = "from spack.util.filesystem import mkdirp\n"
    recipe.write_text(original, encoding="utf-8")

    upgrade["clean_package_imports"](make_repo(tmp_path))

    assert recipe.read_text(encoding="utf-8") == original


def test_clean_package_imports_ignores_missing_package_directory(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    capsys: pytest.CaptureFixture[str],
) -> None:
    """Skip scanning when a repository has no package directory."""
    upgrade = load_upgrade_script(monkeypatch)
    upgrade["clean_package_imports"](make_repo(tmp_path))
    assert capsys.readouterr().out == ""


def test_main_skips_builtin_repo_and_upgrades_custom_repo(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Protect builtin Spack while migrating active third-party repositories."""
    upgrade = load_upgrade_script(monkeypatch)
    builtin = tmp_path / "builtin"
    custom = tmp_path / "custom"
    builtin.mkdir()
    custom.mkdir()
    builtin_yaml = builtin / "repo.yaml"
    builtin_yaml.write_text("repo:\n  api: v1.0\n", encoding="utf-8")
    custom_yaml = custom / "repo.yaml"
    custom_yaml.write_text("repo:\n  api: v1.0\n", encoding="utf-8")

    upgrade["spack"].repo.PATH.repos = [make_repo(builtin, "builtin"), make_repo(custom, "custom")]
    upgrade["main"]()

    assert yaml.safe_load(builtin_yaml.read_text(encoding="utf-8"))["repo"]["api"] == "v1.0"
    assert yaml.safe_load(custom_yaml.read_text(encoding="utf-8"))["repo"]["api"] == "v2.2"
