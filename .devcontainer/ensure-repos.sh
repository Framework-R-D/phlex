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

# --- Headroom Proxy Relays for Devcontainer ---
#
# The headroom proxy was split into TWO SSH-tunnelled ports, each bound only to
# 127.0.0.1 on this host:
#   HEADROOM_AZURE_PORT (9797) : optimized proxy, Kilo provider fnal-azure
#   HEADROOM_OW_PORT    (9798) : passthrough proxy, Kilo provider fnal-ow
# Rootless Podman uses pasta for container networking, so containers reach the
# host via host.docker.internal (169.254.1.2) rather than via a bridge
# interface.  However, pasta maps host.docker.internal to the host's loopback
# only for ports that are actually listening on all interfaces -- headroom's
# ports are bound to 127.0.0.1 only and are therefore unreachable.
#
# We relay EACH headroom port onto a different port on 0.0.0.0 so that
# containers can reach them via host.docker.internal:<port+10000>.  A different
# port is required because 0.0.0.0:<port> would conflict with the existing
# 127.0.0.1:<port> listener.  KILO_CONFIG_CONTENT_DOCKER (built in the shell rc)
# rewrites both loopback ports to their host.docker.internal relay ports and is
# wired into /root/.bashrc by post-create.sh.

HEADROOM_AZURE_PORT="${HEADROOM_AZURE_PORT:-9797}"
HEADROOM_OW_PORT="${HEADROOM_OW_PORT:-9798}"

# relay_headroom_port ROLE PROXY_PORT
#   Relay 127.0.0.1:PROXY_PORT to 0.0.0.0:(PROXY_PORT+10000) if the proxy is
#   listening; otherwise warn and skip.  Each role gets an independent relay
#   and log file so the two never collide.
relay_headroom_port() {
  local role="$1"
  local proxy_port="$2"
  local relay_port=$(( proxy_port + 10000 ))
  local local_addr="127.0.0.1:${proxy_port}"

  if ss -tlnp 2>/dev/null | grep -q "127.0.0.1:${proxy_port}"; then
    start_socat_relay \
      "Headroom ${role} proxy" \
      "socat TCP-LISTEN:${relay_port}" \
      "TCP-LISTEN:${relay_port},fork,reuseaddr" \
      "TCP:${local_addr}" \
      "/tmp/socat-headroom-${role}.log" \
      "ss -tlnp 2>/dev/null | grep -q ':${relay_port} '" || true
  else
    echo "WARNING: headroom ${role} proxy not detected at ${local_addr}; skipping relay" >&2
    echo "  Ensure the SSH tunnel is active (headroom-${role} running on your laptop)" >&2
  fi
}

relay_headroom_port azure "${HEADROOM_AZURE_PORT}"
relay_headroom_port ow "${HEADROOM_OW_PORT}"

# Ensure remaining source bind mount points exist.
ensure_bind_dir "$HOME/.config/"{gh,kilo}
ensure_bind_dir -m 0700 "$HOME/.gnupg"
ensure_bind_dir -m 0700 "$HOME/.vscode-remote-user-data"
ensure_bind_dir -m 0700 "$HOME/.local/share/kilo"
ensure_bind_dir -m 0700 "$HOME/.phlex-devcontainer-tmp"

# --- Bounded Host Relay Management ---
#
# PHLEX_HOST_RELAY_PORTS specifies explicit port mappings to relay through
# the host loopback. Format: comma-separated SOURCE=DEST pairs where both
# SOURCE and DEST are numeric ports. No wildcards or arbitrary host scan.
#
# Examples:
#   PHLEX_HOST_RELAY_PORTS="11434=21434,3000=13000"
#   PHLEX_HOST_RELAY_PORTS="3000=13000"
#
# Validation rules:
#   - Each entry must be NUMERIC=NUMERIC (e.g., 11434=21434)
#   - Both ports must be non-privileged (>= 1024)
#   - No duplicates (same SOURCE in multiple entries)
#   - No source=relay mappings (SOURCE must be numeric)
#   - SOURCE port must be listening on loopback (127.0.0.1 or ::1)
#   - Darwin relays bind to 127.0.0.1 explicitly for --net=pasta compatibility
#
# Output files (always written, deterministic empty or populated):
#   ~/.phlex-devcontainer-tmp/relays/relay-map.json  - JSON map of SOURCE->DEST
#   ~/.phlex-devcontainer-tmp/relays/relay-map.env   - SHELL_ENV KEY=VALUE pairs
#   PHLEX_HOST_GATEWAY                               - host.docker.internal or host.containers.internal
#   PHLEX_HOST_RELAY_FILE                            - path to JSON map
#   PHLEX_HOST_RELAYS_ENV                            - path to env map
#   PHLEX_PODMAN_SOCKET_SOURCE                       - socket source identifier

RELAYS_DIR="$HOME/.phlex-devcontainer-tmp/relays"
mkdir -p "$RELAYS_DIR"

RELAY_MAP_JSON="${RELAYS_DIR}/relay-map.json"
RELAY_MAP_ENV="${RELAYS_DIR}/relay-map.env"
RELAY_PID_DIR="${RELAYS_DIR}/pids"
mkdir -p "$RELAY_PID_DIR"

# Initialize empty maps
echo '{}' > "$RELAY_MAP_JSON"
: > "$RELAY_MAP_ENV"

# Export defaults (will be updated if relays configured)
export PHLEX_HOST_GATEWAY="host.docker.internal"
export PHLEX_HOST_RELAY_FILE="$RELAY_MAP_JSON"
export PHLEX_HOST_RELAYS_ENV="$RELAY_MAP_ENV"
export PHLEX_PODMAN_SOCKET_SOURCE="podman-machine"

# Track PIDs for per-relay cleanup
declare -A relay_pids

# cleanup_relay SOURCE_PORT - kill only the process owned by this relay
cleanup_relay() {
  local source_port="$1"
  local pid="${relay_pids[$source_port]:-}"
  if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
    kill "$pid" 2>/dev/null || true
    rm -f "${RELAY_PID_DIR}/${source_port}"
    unset "relay_pids[$source_port]"
  fi
}

# Cleanup function for exit
cleanup_relays() {
  for source_port in "${!relay_pids[@]}"; do
    cleanup_relay "$source_port"
  done
}

# Register cleanup on exit
trap cleanup_relays EXIT

# Parse PHLEX_HOST_RELAY_PORTS if set
if [ -n "${PHLEX_HOST_RELAY_PORTS:-}" ]; then
  # Track source ports to detect duplicates
  declare -A seen_sources
  relay_json_entries=()
  relay_env_lines=()

  # Split by comma
  IFS=',' read -ra relay_pairs <<< "$PHLEX_HOST_RELAY_PORTS"

  for pair in "${relay_pairs[@]}"; do
    # Trim whitespace
    pair=$(echo "$pair" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')

    # Must contain exactly one equals sign
    if [[ ! "$pair" =~ ^[^=]+=[^=]+$ ]]; then
      echo "ERROR: malformed relay mapping: '$pair' (expected SOURCE=DEST)" >&2
      continue
    fi

    source_port="${pair%%=*}"
    dest_port="${pair#*=}"

    # Both must be numeric
    if [[ ! "$source_port" =~ ^[0-9]+$ ]] || [[ ! "$dest_port" =~ ^[0-9]+$ ]]; then
      echo "ERROR: relay mapping ports must be numeric: '$pair'" >&2
      continue
    fi

    # Reject source=relay or source=hostname entries - SOURCE must be numeric
    if [[ "$source_port" == "relay" ]] || [[ "$source_port" =~ ^[a-zA-Z] ]]; then
      echo "ERROR: relay source must be numeric port, not 'relay' or hostname: '$pair'" >&2
      continue
    fi

    # Check for non-privileged ports (both must be >= 1024)
    if (( source_port < 1024 || dest_port < 1024 )); then
      echo "ERROR: relay ports must be non-privileged (>= 1024): '$pair'" >&2
      continue
    fi

    # Check for duplicate source ports
    if [[ -n "${seen_sources[$source_port]+isset}" ]]; then
      echo "ERROR: duplicate source port in relay mappings: '$source_port'" >&2
      continue
    fi
    seen_sources[$source_port]=1

    # Check if source port is actually listening on loopback (127.0.0.1 or ::1)
    source_listening=false
    if ss -tlnp 2>/dev/null | grep -qE "(127\.0\.0\.1|::1):${source_port} "; then
      source_listening=true
    fi

    if [ "$source_listening" = false ]; then
      echo "WARNING: source port $source_port not listening on loopback; skipping relay" >&2
      continue
    fi

    # Darwin-specific: bind the relay listener to 127.0.0.1 explicitly
    if [[ "$OSTYPE" == "darwin"* ]]; then
      relay_bind_addr="127.0.0.1:${dest_port}"
      listen_spec="TCP-LISTEN:${relay_bind_addr},fork,reuseaddr"
    else
      # Linux: bind to 0.0.0.0 for container reachability via host.containers.internal
      relay_bind_addr="0.0.0.0:${dest_port}"
      listen_spec="TCP-LISTEN:${dest_port},fork,reuseaddr"
    fi

    # Start the relay - source is 127.0.0.1:source_port, destination is relay_bind_addr
    kill_pattern="socat.*TCP:127.0.0.1:${source_port}"
    logfile="${RELAYS_DIR}/relay-${source_port}-${dest_port}.log"

    if command -v socat >/dev/null 2>&1; then
      echo "Starting host relay: 127.0.0.1:${source_port} -> ${relay_bind_addr}"
      nohup setsid socat "TCP:127.0.0.1:${source_port}" "${listen_spec}" > "$logfile" 2>&1 &
      local pid=$!
      relay_pids[$source_port]=$pid

      # Store PID for cleanup
      echo "$pid" > "${RELAY_PID_DIR}/${source_port}"

      # Wait briefly for relay to start
      sleep 0.2

      # Verify relay is listening
      if ss -tlnp 2>/dev/null | grep -qE "${relay_bind_addr} "; then
        echo "Host relay $source_port -> $dest_port ready"
      else
        echo "WARNING: host relay $source_port -> $dest_port did not start in time" >&2
        cleanup_relay "$source_port"
      fi
    else
      echo "WARNING: socat not found; cannot start relay $source_port -> $dest_port" >&2
    fi

    # Record the relay mapping for JSON and env files
    relay_json_entries+=("\"${source_port}\": \"${dest_port}\"")
    relay_env_lines+=("PHLEX_HOST_RELAY_${source_port}=${dest_port}")
  done

  # Write JSON map (empty object if no valid entries)
  if [ ${#relay_json_entries[@]} -gt 0 ]; then
    json_content="{"
    first=true
    for entry in "${relay_json_entries[@]}"; do
      if [ "$first" = true ]; then
        first=false
      else
        json_content+=", "
      fi
      json_content+="$entry"
    done
    json_content+="}"
    echo "$json_content" > "$RELAY_MAP_JSON"
  fi

  # Write env map (empty file if no valid entries)
  if [ ${#relay_env_lines[@]} -gt 0 ]; then
    printf '%s\n' "${relay_env_lines[@]}" > "$RELAY_MAP_ENV"
  fi

  # Update exports based on relay configuration
  export PHLEX_HOST_RELAY_FILE="$RELAY_MAP_JSON"
  export PHLEX_HOST_RELAYS_ENV="$RELAY_MAP_ENV"
fi

# Darwin-specific relay binding with --net=pasta guard
if [[ "$OSTYPE" == "darwin"* ]]; then
  # Darwin guard: require a real VM-side socket when Podman is actually in use
  VM_SOCKET_PATH="${PODMAN_REAL_SOCKET:-${XDG_RUNTIME_DIR:-/run/user/$(id -u)}/podman/podman.sock}"
  PODMAN_IN_USE=false

  if command -v podman >/dev/null 2>&1; then
    # Check if podman machine is running
    if podman machine list 2>/dev/null | grep -qE 'Running|Started'; then
      # Podman machine is running - verify VM-side socket via ssh
      if podman machine ssh test -S "$VM_SOCKET_PATH" 2>/dev/null; then
        export PHLEX_PODMAN_SOCKET_SOURCE="podman-machine:ssh"
        export PHLEX_DEV_NETWORK_MODE="pasta"
        export PHLEX_HOST_GATEWAY="host.docker.internal"
        PODMAN_IN_USE=true
      else
        echo "WARNING: Darwin podman machine socket not accessible via 'podman machine ssh -S'" >&2
        echo "  Tested path: $VM_SOCKET_PATH" >&2
        echo "  Nested container support (act) may not work" >&2
        export PHLEX_PODMAN_SOCKET_SOURCE="podman-machine:missing"
        export PHLEX_DEV_NETWORK_MODE="pasta"
        export PHLEX_HOST_GATEWAY="host.docker.internal"
      fi
    else
      # Not in podman machine, check for native socket
      if [ -S "$VM_SOCKET_PATH" ]; then
        export PHLEX_PODMAN_SOCKET_SOURCE="host:direct"
        export PHLEX_DEV_NETWORK_MODE="pasta"
        export PHLEX_HOST_GATEWAY="host.docker.internal"
        PODMAN_IN_USE=true
      else
        echo "WARNING: Darwin socket not found at $VM_SOCKET_PATH" >&2
        echo "  Nested container support (act) may not work" >&2
        export PHLEX_PODMAN_SOCKET_SOURCE="host:missing"
        export PHLEX_DEV_NETWORK_MODE="pasta"
        export PHLEX_HOST_GATEWAY="host.docker.internal"
      fi
    fi
  else
    echo "WARNING: podman not found - assuming no Podman usage" >&2
    export PHLEX_PODMAN_SOCKET_SOURCE="none"
    export PHLEX_DEV_NETWORK_MODE="pasta"
    export PHLEX_HOST_GATEWAY="host.docker.internal"
  fi
else
  # Linux: preserve existing proxy behavior
  export PHLEX_PODMAN_SOCKET_SOURCE="host:direct"
  export PHLEX_DEV_NETWORK_MODE="bridge"
  export PHLEX_HOST_GATEWAY="host.containers.internal"
fi

echo "SUCCESS: .devcontainer/ensure-repos.sh completed successfully"
