#!/bin/bash

# Ensure auxiliary repositories exist on the host as siblings of the phlex
# repository.  Called by devcontainer.json initializeCommand before the
# container is created, so that bind mounts always have a valid source path.
#
# If a repository is already present its existing content is left untouched,
# preserving local branches and uncommitted changes.  If a clone fails the
# directory is still created so the mount succeeds, and setup-repos.sh can
# retry the clone from inside the container.

# Do not use set -e: clone failures are handled explicitly and must not abort
# the whole script, as that would prevent the container from starting.
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PARENT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"

clone_if_absent() {
  local repo=$1
  local dest="${PARENT_DIR}/${repo}"

  if [ -e "${dest}/.git" ]; then
    echo "Repository already present: ${dest}"
    return 0
  fi

  if [ -d "${dest}" ] && [ -n "$(ls -A "${dest}" 2>/dev/null)" ]; then
    echo "WARNING: ${dest} exists and is non-empty but not a git repository; skipping" >&2
    return 0
  fi

  # Create the directory so the bind mount has a valid source even if the
  # clone fails.
  mkdir -p "${dest}"

  echo "Cloning Framework-R-D/${repo} into ${dest} ..."
  local max_tries=3 current_try=0
  while ! git clone --depth 1 "https://github.com/Framework-R-D/${repo}.git" "${dest}"; do
    (( ++current_try ))
    echo "Attempt ${current_try}/${max_tries} to clone ${repo} from GitHub FAILED" >&2
    if (( current_try >= max_tries )); then
      echo "WARNING: unable to clone ${repo} to ${dest}; an empty directory was created" >&2
      return 0
    fi
    # Remove any partial clone before retrying; recreate the empty directory
    # so the bind mount source remains valid.
    rm -rf "${dest}"
    mkdir -p "${dest}"
    sleep 5
  done
}

clone_if_absent phlex-design
clone_if_absent phlex-examples
clone_if_absent phlex-coding-guidelines
clone_if_absent phlex-spack-recipes

# Ensure the Podman runtime directory exists so the devcontainer bind mount
# has a valid source even when the Podman socket is not yet active.  The
# socket itself is created by Podman at runtime; we only need the parent
# directory to exist for the mount to succeed.
PODMAN_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}/podman"
if mkdir -p "${PODMAN_RUNTIME_DIR}" 2>/dev/null; then
  if [ ! -S "${PODMAN_RUNTIME_DIR}/podman.sock" ]; then
    echo "NOTE: Podman socket not found at ${PODMAN_RUNTIME_DIR}/podman.sock" >&2
    echo "  To use 'act' inside the devcontainer, enable the Podman socket:" >&2
    echo "    systemctl --user enable --now podman.socket" >&2
  fi
else
  echo "WARNING: unable to create ${PODMAN_RUNTIME_DIR}; 'act' will not work inside the container without the Podman socket" >&2
fi
