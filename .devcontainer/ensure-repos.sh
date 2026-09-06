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

# --- CLI options ---
MODE="default"
START_TEST_SERVICES=""
SELF_TEST_DIR=""
CLEANUP_OWNED=""

for arg in "$@"; do
  case "$arg" in
    --start-test-services=*)
      START_TEST_SERVICES="${arg#*=}"
      MODE="start-test-services"
      ;;
    --self-test=*)
      SELF_TEST_DIR="${arg#*=}"
      MODE="self-test"
      ;;
    --cleanup-owned)
      CLEANUP_OWNED="1"
      MODE="cleanup-owned"
      ;;
    *)
      echo "ERROR: unknown option: $arg" >&2
      exit 1
      ;;
  esac
done

# --- Relay configuration ---
PHLEX_RELAY_STATE_DIR="${PHLEX_RELAY_STATE_DIR:-$HOME/.phlex-devcontainer-tmp/relays}"
if [[ "$MODE" == "self-test" ]]; then
  PHLEX_RELAY_STATE_DIR="${SELF_TEST_DIR}/relay-state"
fi
mkdir -p "$PHLEX_RELAY_STATE_DIR"

RELAY_MAP_JSON="${PHLEX_RELAY_STATE_DIR}/relay-map.json"
RELAY_MAP_ENV="${PHLEX_RELAY_STATE_DIR}/relay-map.env"
RELAY_PID_DIR="${PHLEX_RELAY_STATE_DIR}/pids"
mkdir -p "$RELAY_PID_DIR"

# Default exports (updated if relays are configured)
export PHLEX_HOST_GATEWAY="host.docker.internal"
export PHLEX_HOST_RELAY_FILE="$RELAY_MAP_JSON"
export PHLEX_HOST_RELAYS_ENV="$RELAY_MAP_ENV"
export PHLEX_PODMAN_SOCKET_SOURCE="podman-machine"
export PHLEX_RELAY_STATE_DIR

# --- Helper functions ---

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

  pkill -f "${kill_pattern}" 2>/dev/null || true
  sleep 0.2

  echo "Starting ${label} relay: ${listen_spec} -> ${connect_spec} ..."
  if command -v setsid >/dev/null 2>&1; then
    nohup setsid socat "${listen_spec}" "${connect_spec}" > "${logfile}" 2>&1 &
  else
    nohup socat "${listen_spec}" "${connect_spec}" > "${logfile}" 2>&1 &
  fi

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

# detect_listener SOURCE_PORT - check if port is listening on loopback
# Returns 0 if listening, 1 otherwise
detect_listener() {
  local source_port="$1"

  if [[ "$OSTYPE" == "darwin"* ]]; then
    # Darwin: use lsof for precise listener detection
    if command -v lsof >/dev/null 2>&1; then
      lsof -i :${source_port} -P -n 2>/dev/null | grep -qE "127\.0\.0\.1:${source_port}|::1:${source_port}" && return 0
    fi
  else
    # Linux: use ss for listener detection
    if command -v ss >/dev/null 2>&1; then
      ss -tlnp 2>/dev/null | grep -qE "(127\.0\.0\.1|::1):${source_port} " && return 0
    fi
  fi

  return 1
}

# is_pid_owned_by_us PIDFILE SOURCE_PORT DEST_PORT - verify PID file ownership
# Returns 0 if PID file is owned by this relay script, 1 otherwise
is_pid_owned_by_us() {
  local pidfile="$1"
  local source_port="$2"
  local dest_port="$3"

  if [ ! -f "$pidfile" ]; then
    return 1
  fi

  local stored_pid
  stored_pid=$(cat "$pidfile" 2>/dev/null) || return 1

  if [ -z "$stored_pid" ]; then
    return 1
  fi

  if ! kill -0 "$stored_pid" 2>/dev/null; then
    return 1
  fi

  # Check command identity via /proc/pid/cmdline
  if [[ -f "/proc/${stored_pid}/cmdline" ]]; then
    local cmdline
    cmdline=$(tr '\0' ' ' < "/proc/${stored_pid}/cmdline" 2>/dev/null)
    if [[ "$cmdline" =~ socat.*TCP.*${source_port} ]] || [[ "$cmdline" =~ socat.*${source_port} ]]; then
      return 0
    fi
  fi

  # Darwin fallback: try lsof to verify process is socat relaying this port
  if [[ "$OSTYPE" == "darwin"* ]] && command -v lsof >/dev/null 2>&1; then
    if lsof -p "$stored_pid" -P -n 2>/dev/null | grep -q socat; then
      return 0
    fi
  fi

  return 1
}

# Cleanup function for exit
cleanup_relays() {
  for pidfile in "${PHLEX_RELAY_STATE_DIR}/pids"/*; do
    if [ -f "$pidfile" ]; then
      local source_port
      source_port=$(basename "$pidfile")
      cleanup_owned_process "$pidfile" "$source_port"
    fi
  done
}

# cleanup_owned_process PIDFILE SOURCE_PORT - clean only owned processes
cleanup_owned_process() {
  local pidfile="$1"
  local source_port="$2"

  if [ ! -f "$pidfile" ]; then
    return 0
  fi

  local stored_pid
  stored_pid=$(cat "$pidfile" 2>/dev/null) || return 0

  if [ -z "$stored_pid" ]; then
    rm -f "$pidfile"
    return 0
  fi

  # Verify PID file is owned by us before killing
  if is_pid_owned_by_us "$pidfile" "$source_port" ""; then
    if kill -0 "$stored_pid" 2>/dev/null; then
      kill "$stored_pid" 2>/dev/null || true
      sleep 0.1
      if kill -0 "$stored_pid" 2>/dev/null; then
        kill -9 "$stored_pid" 2>/dev/null || true
      fi
    fi
  fi

  rm -f "$pidfile"
}

# --- Relay mapping operations ---

# parse_and_validate_relays - validate and collect valid relay mappings
# Sets global arrays: valid_source_ports, valid_dest_ports
declare -a valid_source_ports=()
declare -a valid_dest_ports=()

parse_and_validate_relays() {
  local relay_input="${1:-}"
  local invalid=0
  local -a seen_sources=()

  valid_source_ports=()
  valid_dest_ports=()

  if [ -z "$relay_input" ]; then
    return 0
  fi

  IFS=',' read -ra relay_pairs <<< "$relay_input"

  for pair in "${relay_pairs[@]}"; do
    pair=$(echo "$pair" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')

    # Must contain exactly one equals sign
    if [[ ! "$pair" =~ ^[^=]+=[^=]+$ ]]; then
      echo "ERROR: malformed relay mapping: '$pair' (expected SOURCE=DEST)" >&2
      invalid=1
      continue
    fi

    source_port="${pair%%=*}"
    dest_port="${pair#*=}"

    # Both must be numeric
    if [[ ! "$source_port" =~ ^[0-9]+$ ]] || [[ ! "$dest_port" =~ ^[0-9]+$ ]]; then
      echo "ERROR: relay mapping ports must be numeric: '$pair'" >&2
      invalid=1
      continue
    fi

    # Check for privileged ports (both must be >= 1024)
    if (( source_port < 1024 || dest_port < 1024 )); then
      echo "ERROR: relay ports must be non-privileged (>= 1024): '$pair'" >&2
      invalid=1
      continue
    fi

    # Check for equal source and destination ports
    if (( source_port == dest_port )); then
      echo "ERROR: source and destination ports must be different: '$pair'" >&2
      invalid=1
      continue
    fi

    # Check for duplicate source ports
    for seen_source in ${seen_sources[*]-}; do
      if [ "$seen_source" = "$source_port" ]; then
        echo "ERROR: duplicate source port in relay mappings: '$source_port'" >&2
        invalid=1
        continue 2
      fi
    done
    seen_sources+=("$source_port")

    case "${source_port}=${dest_port}" in
      25114=25115|25300=25301) ;;
      *)
        echo "ERROR: relay mapping is not in the allowlist: '$pair'" >&2
        invalid=1
        continue
        ;;
    esac

    # Check if source port is actually listening on loopback
    if ! detect_listener "$source_port"; then
      echo "WARNING: source port $source_port not listening on loopback; skipping relay" >&2
      invalid=1
      continue
    fi

    # Valid mapping
    valid_source_ports+=("$source_port")
    valid_dest_ports+=("$dest_port")
  done

  if (( invalid != 0 )); then
    valid_source_ports=()
    valid_dest_ports=()
    return 1
  fi
}

# start_relay SOURCE_PORT DEST_PORT - start a single relay with proper binding
start_relay() {
  local source_port="$1"
  local dest_port="$2"

  local relay_bind_addr
  local listen_spec
  local connect_spec

  if [[ "$OSTYPE" == "darwin"* ]]; then
    # Darwin: bind to 127.0.0.1 explicitly for --net=pasta compatibility
    relay_bind_addr="127.0.0.1:${dest_port}"
    listen_spec="TCP-LISTEN:${relay_bind_addr},fork,reuseaddr"
  else
    # Linux: bind to 0.0.0.0 for container reachability via host.containers.internal
    relay_bind_addr="0.0.0.0:${dest_port}"
    listen_spec="TCP-LISTEN:${dest_port},fork,reuseaddr"
  fi

  connect_spec="TCP:127.0.0.1:${source_port}"
  local kill_pattern="socat.*${source_port}"
  local logfile="${PHLEX_RELAY_STATE_DIR}/relay-${source_port}-${dest_port}.log"

  echo "Starting host relay: 127.0.0.1:${source_port} -> ${relay_bind_addr}"
  if command -v setsid >/dev/null 2>&1; then
    nohup setsid socat "$listen_spec" "$connect_spec" > "$logfile" 2>&1 &
  else
    nohup socat "$listen_spec" "$connect_spec" > "$logfile" 2>&1 &
  fi
  local pid=$!

  # Store PID with command identity (for cleanup verification)
  echo "$pid" > "${RELAY_PID_DIR}/${source_port}"

  # Verify relay is listening
  sleep 0.2

  local ready=false
  if [[ "$OSTYPE" == "darwin"* ]]; then
    if command -v lsof >/dev/null 2>&1; then
      lsof -i :${dest_port} -P -n 2>/dev/null | grep -qE "127\.0\.0\.1:${dest_port}" && ready=true
    fi
  else
    if command -v ss >/dev/null 2>&1; then
      ss -tlnp 2>/dev/null | grep -qE ":${dest_port} " && ready=true
    fi
  fi

  if $ready; then
    echo "Host relay $source_port -> $dest_port ready"
    return 0
  else
    echo "WARNING: host relay $source_port -> $dest_port did not start in time" >&2
    cleanup_owned_process "${RELAY_PID_DIR}/${source_port}" "$source_port"
    return 1
  fi
}

# write_relay_maps - write deterministic sorted JSON and env maps
write_relay_maps() {
  # Sort by source port for deterministic output
  local -a sorted_source=()
  local -a sorted_dest=()

  if [ -n "${valid_source_ports[*]-}" ]; then
    # Create pairs for sorting
    local -a pairs=()
    local source_count=0
    if [ -n "${valid_source_ports[*]-}" ]; then
      source_count=${#valid_source_ports[@]}
    fi
    for ((i = 0; i < source_count; i++)); do
      pairs+=("${valid_source_ports[$i]}:${valid_dest_ports[$i]}")
    done

    # Sort pairs
    local -a sorted_pairs=()
    while IFS= read -r pair; do
      sorted_pairs+=("$pair")
    done < <(printf '%s\n' "${pairs[@]}" | sort -t: -k1 -n)

    for pair in "${sorted_pairs[@]}"; do
      sorted_source+=("${pair%%:*}")
      sorted_dest+=("${pair#*:}")
    done
  fi

  # Write JSON map
  if [ -n "${sorted_source[*]-}" ]; then
    local json_content="{"
    local first=true
    for source in "${sorted_source[@]}"; do
      local idx=0
      for s in "${sorted_source[@]}"; do
        if [ "$s" = "$source" ]; then
          break
        fi
        ((idx++))
      done
      local dest="${sorted_dest[$idx]}"

      if $first; then
        first=false
      else
        json_content+=", "
      fi
      json_content+="\"${source}\": \"${dest}\""
    done
    json_content+="}"
    echo "$json_content" > "$RELAY_MAP_JSON"
  else
    echo '{}' > "$RELAY_MAP_JSON"
  fi

  # Write env map
  if [ -n "${sorted_source[*]-}" ]; then
    local env_content=""
    local sorted_count=0
    if [ -n "${sorted_source[*]-}" ]; then
      sorted_count=${#sorted_source[@]}
    fi
    for ((i = 0; i < sorted_count; i++)); do
      env_content+="PHLEX_HOST_RELAY_${sorted_source[$i]}=${sorted_dest[$i]}"$'\n'
    done
    printf '%s' "$env_content" > "$RELAY_MAP_ENV"
  else
    : > "$RELAY_MAP_ENV"
  fi
}

# --- Mode-specific implementations ---

# mode_default - standard execution with PHLEX_HOST_RELAY_PORTS
mode_default() {
  echo "Running in default mode with PHLEX_HOST_RELAY_PORTS"

  if [ -n "${PHLEX_HOST_RELAY_PORTS:-}" ]; then
    if ! parse_and_validate_relays "$PHLEX_HOST_RELAY_PORTS"; then
      write_relay_maps
      return 1
    fi

    local -a ready_source_ports=()
    local -a ready_dest_ports=()
    local source_count=0
    if [ -n "${valid_source_ports[*]-}" ]; then
      source_count=${#valid_source_ports[@]}
    fi
    for ((i = 0; i < source_count; i++)); do
      if start_relay "${valid_source_ports[$i]}" "${valid_dest_ports[$i]}"; then
        ready_source_ports+=("${valid_source_ports[$i]}")
        ready_dest_ports+=("${valid_dest_ports[$i]}")
      fi
    done
    valid_source_ports=("${ready_source_ports[@]-}")
    valid_dest_ports=("${ready_dest_ports[@]-}")

    write_relay_maps

    export PHLEX_HOST_RELAY_FILE="$RELAY_MAP_JSON"
    export PHLEX_HOST_RELAYS_ENV="$RELAY_MAP_ENV"
  else
    echo "No PHLEX_HOST_RELAY_PORTS configured; using defaults"
    valid_source_ports=()
    valid_dest_ports=()
    write_relay_maps
  fi
}

# mode_start_test_services - start test relay services on 25114/25300
mode_start_test_services() {
  echo "Starting test relay services"

  # Use the exact mappings from relay-interface record
  local test_ports="25114=25115,25300=25301"

  if ! parse_and_validate_relays "$test_ports"; then
    write_relay_maps
    return 1
  fi

  local -a ready_source_ports=()
  local -a ready_dest_ports=()
  local source_count=0
  if [ -n "${valid_source_ports[*]-}" ]; then
    source_count=${#valid_source_ports[@]}
  fi
  for ((i = 0; i < source_count; i++)); do
    if start_relay "${valid_source_ports[$i]}" "${valid_dest_ports[$i]}"; then
      ready_source_ports+=("${valid_source_ports[$i]}")
      ready_dest_ports+=("${valid_dest_ports[$i]}")
    fi
  done
  valid_source_ports=("${ready_source_ports[@]-}")
  valid_dest_ports=("${ready_dest_ports[@]-}")

  write_relay_maps

  export PHLEX_HOST_RELAY_FILE="$RELAY_MAP_JSON"
  export PHLEX_HOST_RELAYS_ENV="$RELAY_MAP_ENV"
}

# mode_self_test - run self-test in external directory
mode_self_test() {
  local test_dir="$SELF_TEST_DIR"
  local failed=0

  assert_valid_count() {
    local expected="$1"
    local actual=0
    if [ -n "${valid_source_ports[*]-}" ]; then
      actual=${#valid_source_ports[@]}
    fi
    if [ "$actual" -ne "$expected" ]; then
      echo "ERROR: expected $expected valid mappings, got $actual" >&2
      return 1
    fi
  }

  if [ ! -d "$test_dir" ]; then
    echo "ERROR: self-test directory does not exist: $test_dir" >&2
    exit 1
  fi

  echo "Running self-test in: $test_dir"

  # Create test relay state directory
  local test_state_dir="${test_dir}/relay-state"
  mkdir -p "$test_state_dir/pids"

  # Override globals for test mode
  RELAY_MAP_JSON="${test_state_dir}/relay-map.json"
  RELAY_MAP_ENV="${test_state_dir}/relay-map.env"
  RELAY_PID_DIR="${test_state_dir}/pids"

  # Start disposable listeners on the two allowlisted source ports.
  echo "Setting up test listeners on 25114 and 25300..."
  local fakelistener
  fakelistener() {
    local port=$1
    python3 -c "
import socket, time
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(('127.0.0.1', $port))
s.listen(1)
time.sleep(5)
" 2>/dev/null &
  }
  fakelistener 25114
  fakelistener 25300
  sleep 0.5

  # Test 1: Positive case - valid mappings
  echo "Test 1: Valid relay mappings (25114=25115,25300=25301)"
  parse_and_validate_relays "25114=25115,25300=25301"
  local valid_count=${#valid_source_ports[@]}
  echo "  Valid pairs found: $valid_count (expected 2)"
  assert_valid_count 2 || failed=1

  # Test 2: Malformed input
  echo "Test 2: Malformed input (non-numeric and ports < 1024)"
  valid_source_ports=()
  valid_dest_ports=()
  parse_and_validate_relays "abc=def,123=456"
  echo "  Valid pairs found: ${#valid_source_ports[@]} (expected 0)"
  assert_valid_count 0 || failed=1

  # Test 3: Duplicate source
  echo "Test 3: Duplicate source port (25114=25115,25114=25116)"
  valid_source_ports=()
  valid_dest_ports=()
  parse_and_validate_relays "25114=25115,25114=25116"
  echo "  Valid pairs found: ${#valid_source_ports[@]} (expected 0)"
  assert_valid_count 0 || failed=1

  # Test 4: Privileged port (< 1024)
  echo "Test 4: Privileged port (80=8080)"
  valid_source_ports=()
  valid_dest_ports=()
  parse_and_validate_relays "80=8080,1024=11024"
  echo "  Valid pairs found: ${#valid_source_ports[@]} (expected 0 - 80 is privileged and 1024 not listening)"
  assert_valid_count 0 || failed=1

  # Test 5: Equal source and dest port
  echo "Test 5: Equal source and dest port (25114=25114)"
  valid_source_ports=()
  valid_dest_ports=()
  parse_and_validate_relays "25114=25114,25300=25301"
  echo "  Valid pairs found: ${#valid_source_ports[@]} (expected 0)"
  assert_valid_count 0 || failed=1

  # Test 6: Unavailable source (port not listening)
  echo "Test 6: Unavailable source port (9999=19999)"
  valid_source_ports=()
  valid_dest_ports=()
  parse_and_validate_relays "9999=19999"
  echo "  Valid pairs found: ${#valid_source_ports[@]} (expected 0)"
  assert_valid_count 0 || failed=1

  # Test 7: Deterministic map ordering (reverse order should produce same result)
  echo "Test 7: Deterministic map ordering (reverse order should produce same result)"
  valid_source_ports=()
  valid_dest_ports=()
  parse_and_validate_relays "25300=25301,25114=25115"
  write_relay_maps

  if [ -f "$RELAY_MAP_JSON" ]; then
    local json_content
    json_content=$(cat "$RELAY_MAP_JSON")
    echo "  JSON content: $json_content"
  fi

  if [ -f "$RELAY_MAP_ENV" ]; then
    local env_content
    env_content=$(cat "$RELAY_MAP_ENV")
    echo "  ENV lines: $(echo "$env_content" | grep -c . 2>/dev/null || echo 0)"
    echo "  ENV content:"
    cat "$RELAY_MAP_ENV" | sed 's/^/    /'
  fi

  # Test 8: Cleanup function
  echo "Test 8: Cleanup function"
  echo "12345" > "${test_state_dir}/pids/25114"
  if is_pid_owned_by_us "${test_state_dir}/pids/25114" "25114" "25115"; then
    echo "  PID ownership: CORRECTLY REJECTED (12345 is not a socat process)"
  else
    echo "  PID ownership: CORRECTLY FAILED (fake PID not a real process)"
  fi

  echo "Self-test completed"
  return "$failed"
}

# mode_cleanup_owned - cleanup only processes owned by this relay system
mode_cleanup_owned() {
  echo "Cleaning up owned relay processes"

  if [ ! -d "$RELAY_PID_DIR" ]; then
    echo "No PID directory found: $RELAY_PID_DIR"
    exit 0
  fi

  for pidfile in "$RELAY_PID_DIR"/*; do
    if [ -f "$pidfile" ]; then
      local source_port
      source_port=$(basename "$pidfile")
      cleanup_owned_process "$pidfile" "$source_port"
    fi
  done

  # Clean up empty PID directory
  rmdir "$RELAY_PID_DIR" 2>/dev/null || true

  echo "Cleanup completed"
}

# --- Main execution ---

# Fixture and cleanup modes must not clone repositories, create bind mounts,
# inspect Podman, or create production relay state.
case "$MODE" in
  self-test)
    mode_self_test
    exit $?
    ;;
  cleanup-owned)
    mode_cleanup_owned
    exit $?
    ;;
  start-test-services)
    mode_start_test_services
    exit $?
    ;;
esac

clone_if_absent phlex-design
clone_if_absent phlex-examples
clone_if_absent phlex-coding-guidelines
clone_if_absent phlex-spack-recipes

# --- Podman Socket Proxy for Nested Containers (act) ---
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

if [ ! -S "${PROXY_SOCKET}" ]; then
  python3 -c "
import socket, os
s = socket.socket(socket.AF_UNIX)
s.bind('${PROXY_SOCKET}')
" 2>/dev/null || touch "${PROXY_SOCKET}"
fi

# --- Headroom Proxy Relays for Devcontainer ---
HEADROOM_AZURE_PORT="${HEADROOM_AZURE_PORT:-9797}"
HEADROOM_OW_PORT="${HEADROOM_OW_PORT:-9798}"

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

# Execute the normal host setup after the legacy repository and Podman setup.
if ! mode_default; then
  echo "ERROR: relay configuration rejected; no relay map was admitted" >&2
  exit 1
fi

# Darwin-specific relay binding with --net=pasta guard
if [[ "$OSTYPE" == "darwin"* ]]; then
  VM_SOCKET_PATH="${PODMAN_REAL_SOCKET:-${XDG_RUNTIME_DIR:-/run/user/$(id -u)}/podman/podman.sock}"
  PODMAN_IN_USE=false

  if command -v podman >/dev/null 2>&1; then
    if podman machine list 2>/dev/null | grep -qE 'Running|Started'; then
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
  export PHLEX_PODMAN_SOCKET_SOURCE="host:direct"
  export PHLEX_DEV_NETWORK_MODE="bridge"
  export PHLEX_HOST_GATEWAY="host.containers.internal"
fi

echo "SUCCESS: .devcontainer/ensure-repos.sh completed successfully"
