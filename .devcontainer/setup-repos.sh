#!/bin/bash

# Clone companion repositories into the codespace so they are available
# for reference by developers and AI agents.
#
# Intended to be called from devcontainer.json onCreateCommand.

set -euo pipefail

WORKSPACE_ROOT="${1:-/workspaces}"

clone_if_absent() {
  local repo=$1
  local dest="${WORKSPACE_ROOT}/${repo}"
  if [ -d "$dest" ]; then
    echo "Repository already present: $dest"
    return
  fi
  echo "Cloning Framework-R-D/${repo} into ${dest} ..."
  git clone --depth 1 "https://github.com/Framework-R-D/${repo}.git" "$dest"
}

clone_if_absent phlex-design
clone_if_absent phlex-examples
clone_if_absent phlex-coding-guidelines
clone_if_absent phlex-spack-recipes
