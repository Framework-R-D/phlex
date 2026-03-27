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

# --- Podman Socket Proxy for Nested Containers (act) ---
#
# Rootless Podman nested volume mounts (like act's Docker SDK mounting the
# Podman socket) require that the source and target paths match across the
# host/container boundary. We use 'socat' on the host to provide a proxy socket
# at a stable, matching path in the user's home directory.

USER_ID=$(id -u)
PODMAN_REAL_SOCKET="${XDG_RUNTIME_DIR:-/run/user/${USER_ID}}/podman/podman.sock"
PROXY_DIR="${HOME}/.podman-proxy"
PROXY_SOCKET="${PROXY_DIR}/podman.sock"

# Kill any existing socat proxy for this socket
pkill -f "socat UNIX-LISTEN:${PROXY_SOCKET}" || true
# Wait for old process to die and socket to be removed
sleep 0.2
mkdir -p "${PROXY_DIR}"
chmod 700 "${PROXY_DIR}"

mkdir -p "${PROXY_DIR}"
chmod 700 "${PROXY_DIR}"

if [ -S "${PODMAN_REAL_SOCKET}" ]; then
  if command -v socat >/dev/null 2>&1; then
    echo "Proxying Podman socket ${PODMAN_REAL_SOCKET} -> ${PROXY_SOCKET} ..."
    # Use a background socat to proxy the real socket to the proxy socket.
    # This socket will be bind-mounted at the SAME PATH in the container.
    nohup setsid socat "UNIX-LISTEN:${PROXY_SOCKET},fork,reuseaddr,unlink-early" "UNIX-CONNECT:${PODMAN_REAL_SOCKET}" > /tmp/socat-podman.log 2>&1 &
    # Wait for socket to be created
    i=0
    while [ $i -lt 20 ] && [ ! -S "${PROXY_SOCKET}" ]; do
      sleep 0.1
      i=$((i + 1))
    done
    if [ ! -S "${PROXY_SOCKET}" ]; then
      echo "WARNING: Socket creation timed out; creating placeholder" >&2
      socat "UNIX-LISTEN:${PROXY_SOCKET},fork,reuseaddr" "UNIX-CONNECT:${PODMAN_REAL_SOCKET}" &
    fi
  else
    echo "WARNING: socat not found on host; act will not work inside the container" >&2
  fi
else
  echo "WARNING: Podman socket not found at ${PODMAN_REAL_SOCKET}" >&2
  echo "  To use 'act' inside the devcontainer, enable the Podman socket:" >&2
  echo "    systemctl --user enable --now podman.socket" >&2
fi

# Ensure the bind-mount source always exists so 'podman run' never fails with
# "no such file or directory", even when socat is unavailable or the real
# Podman socket is absent.
if [ ! -S "${PROXY_SOCKET}" ]; then
  # Create a dummy socket so the mount source is valid.  The container will
  # start successfully; nested container support (act) simply won't work.
  python3 -c "
import socket, os
s = socket.socket(socket.AF_UNIX)
s.bind('${PROXY_SOCKET}')
" 2>/dev/null || touch "${PROXY_SOCKET}"
fi

echo "SUCCESS: .devcontainer/ensure-repos.sh completed successfully"
