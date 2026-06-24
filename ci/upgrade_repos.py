#!/usr/bin/env spack python
"""Utility to upgrade Spack repository API versions and fix legacy package imports."""

from __future__ import annotations

import os
import re
from pathlib import Path

# 1. Leverage Spack internal API to dynamically find all active repositories
import spack.repo
from spack.repo import Repo
from spack.vendor.ruamel.yaml import YAML

# Mapping old broken imports to modern Spack equivalents
# Mapping old variants to modern canonical Spack equivalents
REPLACEMENTS: dict[str, str] = {
    # Fixes the 'llnl.util.filesystem' mistake from the previous run
    r"import\s+llnl\.util\.filesystem\s+as\s+filesystem": (
        "import spack.util.filesystem as filesystem"
    ),
    r"from\s+llnl\.util\.filesystem\s+import": "from spack.util.filesystem import",
    # Standard catch-alls for any recipes that haven't been touched yet
    r"from\s+spack\.llnl\.util\s+import\s+filesystem": (
        "import spack.util.filesystem as filesystem"
    ),
    r"from\s+spack\.llnl\.util\.filesystem\s+import": "from spack.util.filesystem import",
    r"import\s+spack\.llnl\.util\.filesystem": "import spack.util.filesystem as filesystem",
}


def upgrade_spack_repo_api(repo_obj: Repo) -> None:
    """Upgrades the repo.yaml api setting using Spack's Repo object."""
    # repo_obj.root gives the base path of the specific repository
    repo_path = Path(repo_obj.root)
    yaml_file = repo_path / "repo.yaml"

    if not yaml_file.exists():
        return

    yaml = YAML()
    yaml.preserve_quotes = True

    try:
        data = yaml.load(yaml_file)
        if "repo" in data:
            current_api = data["repo"].get("api", "v2.0")
            # Only upgrade if it's explicitly older than v2.2
            if current_api != "v2.2":
                print(f"Upgrading {repo_obj.namespace} from API {current_api} to v2.2...")
                data["repo"]["api"] = "v2.2"
                with open(yaml_file, "w") as f:
                    yaml.dump(data, f)
                print(f"  [FIXED] {yaml_file}")
    except Exception as e:
        print(f"Error updating {yaml_file}: {e}")


def clean_package_imports(repo_obj: Repo) -> None:
    """Scans and fixes package.py files inside the discovered repository."""
    # repo_obj.root handles varied directory structures seamlessly
    packages_path = Path(repo_obj.root) / "packages"
    if not packages_path.exists():
        return

    print(f"Scanning recipes in {repo_obj.namespace}: {packages_path}")
    count = 0

    for root, _, files in os.walk(packages_path):
        for file in files:
            if file == "package.py":
                file_path = Path(root) / file
                with open(file_path, "r", encoding="utf-8") as f:
                    content = f.read()

                modified_content = content
                for pattern, replacement in REPLACEMENTS.items():
                    modified_content = re.sub(pattern, replacement, modified_content)

                if modified_content != content:
                    with open(file_path, "w", encoding="utf-8") as f:
                        f.write(modified_content)
                    print(f"  [FIXED] {file_path.relative_to(packages_path)}")
                    count += 1

    if count > 0:
        print(f"Successfully patched {count} package recipes.\n")


# --- Execute Refactoring via Spack Context ---
def main() -> None:
    """Main entry point to upgrade Spack repositories and clean package imports."""
    # Spack's repo.path contains a list of all active Repo instances
    active_repos = spack.repo.PATH.repos

    for repo in active_repos:
        # Skip the core builtin Spack repo to avoid touching core code
        if repo.namespace == "builtin":
            continue

        upgrade_spack_repo_api(repo)
        clean_package_imports(repo)


if __name__ == "__main__":
    main()
