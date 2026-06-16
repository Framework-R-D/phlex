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

ensure_bind_dir() {
  mkdir -p "$@"
}

# start_socat_relay LABEL KILL_PATTERN LISTEN_SPEC CONNECT_SPEC LOGFILE READY_TEST
#
# Start a background socat relay, waiting up to 2 s for it to become ready.
#
#   LABEL        - human-readable name for log messages
#   KILL_PATTERN - argument to pkill -f to stop any existing relay
#   LISTEN_SPEC  - socat first address (the listening side)
#   CONNECT_SPEC - socat second address (the connecting side)
#   LOGFILE      - path for socat stdout/stderr
#   READY_TEST   - shell command whose exit code indicates readiness
#
# Returns 0 if the relay is ready, 1 otherwise.
start_socat_relay() {
  local label="$1"
  local kill_pattern="$2"
  local listen_spec="$3"
  local connect_spec="$4"
  local logfile="$5"
  local ready_test="$6"

  if ! command -v socat >/dev/null 2>&1; then
    echo "WARNING: socat not found; cannot start ${label} relay" >&2
    return 1
  fi

  # Stop any pre-existing relay process.
  pkill -f "${kill_pattern}" 2>/dev/null || true
  sleep 0.2

  echo "Starting ${label} relay: ${listen_spec} -> ${connect_spec} ..."
  nohup setsid socat "${listen_spec}" "${connect_spec}" > "${logfile}" 2>&1 &

  # Wait up to 2 s for the relay to become ready.
  local i=0
  while [ "${i}" -lt 20 ]; do
    eval "${ready_test}" 2>/dev/null && break
    sleep 0.1
    i=$(( i + 1 ))
  done

  if eval "${ready_test}" 2>/dev/null; then
    echo "${label} relay ready"
    return 0
  else
    echo "WARNING: ${label} relay did not become ready in time" >&2
    return 1
  fi
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

ensure_bind_dir -m 0700 "${PROXY_DIR}"

if [ -S "${PODMAN_REAL_SOCKET}" ]; then
  start_socat_relay \
    "Podman socket" \
    "socat UNIX-LISTEN:${PROXY_SOCKET}" \
    "UNIX-LISTEN:${PROXY_SOCKET},fork,reuseaddr,unlink-early" \
    "UNIX-CONNECT:${PODMAN_REAL_SOCKET}" \
    "/tmp/socat-podman.log" \
    "[ -S '${PROXY_SOCKET}' ]" || true
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

# --- Headroom Proxy Relay for Devcontainer ---
#
# The headroom proxy is an SSH-tunnelled port bound only to 127.0.0.1 on this
# host.  Rootless Podman uses pasta for container networking, so containers
# reach the host via host.docker.internal (169.254.1.2) rather than via a
# bridge interface.  However, pasta maps host.docker.internal to the host's
# loopback only for ports that are actually listening on all interfaces --
# headroom's port is bound to 127.0.0.1 only and is therefore unreachable.
#
# We relay headroom onto a different port on 0.0.0.0 so that containers can
# reach it via host.docker.internal:$HEADROOM_RELAY_PORT.  A different port is
# required because 0.0.0.0:$HEADROOM_PORT would conflict with the existing
# 127.0.0.1:$HEADROOM_PORT listener.  KILO_CONFIG_CONTENT_DOCKER is passed into the
# devcontainer and post-create.sh wires it into /root/.bashrc to point at
# host.docker.internal:$HEADROOM_RELAY_PORT.

HEADROOM_PORT="${HEADROOM_PORT:-9797}"
HEADROOM_RELAY_PORT=$(( HEADROOM_PORT + 10000 ))
HEADROOM_LOCAL="127.0.0.1:${HEADROOM_PORT}"

if ss -tlnp 2>/dev/null | grep -q "127.0.0.1:${HEADROOM_PORT}"; then
  start_socat_relay \
    "Headroom proxy" \
    "socat TCP-LISTEN:${HEADROOM_RELAY_PORT}" \
    "TCP-LISTEN:${HEADROOM_RELAY_PORT},fork,reuseaddr" \
    "TCP:${HEADROOM_LOCAL}" \
    "/tmp/socat-headroom.log" \
    "ss -tlnp 2>/dev/null | grep -q ':${HEADROOM_RELAY_PORT} '" || true
else
  echo "WARNING: headroom proxy not detected at ${HEADROOM_LOCAL}; skipping relay" >&2
  echo "  Ensure the SSH tunnel is active (headroom running on your laptop)" >&2
fi

# Ensure remaining source bind mount points exist.
ensure_bind_dir "$HOME/.aws"
ensure_bind_dir "$HOME/.config/"{gh,kilo}
ensure_bind_dir "$HOME/.gnupg"
ensure_bind_dir "$HOME/.kiro"
ensure_bind_dir "$HOME/.vscode-remote-user-data"

echo "SUCCESS: .devcontainer/ensure-repos.sh completed successfully"
