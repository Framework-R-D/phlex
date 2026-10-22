#!/bin/bash

if [[ "${BASH_SOURCE[0]}" != "$0" ]]; then
  printf '%s\n' 'ERROR: ensure-repos.sh must be executed, not sourced' >&2
  return 1
fi

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
SELF_TEST_ROOT=""

set_mode() {
  local requested_mode="$1"

  if [ "$MODE" != "default" ] && [ "$MODE" != "$requested_mode" ]; then
    echo "ERROR: only one execution mode may be selected" >&2
    return 1
  fi

  MODE="$requested_mode"
}

parse_cli_args() {
  while [ "$#" -gt 0 ]; do
    case "$1" in
      --start-test-services)
        set_mode "start-test-services" || return 1
        shift
        ;;
      --start-test-services=*)
        echo "ERROR: --start-test-services does not accept a value" >&2
        return 1
        ;;
      --self-test)
        set_mode "self-test" || return 1
        shift
        if [ "$#" -gt 0 ] && [[ "$1" != --* ]]; then
          SELF_TEST_ROOT="$1"
          shift
        fi
        ;;
      --self-test=*)
        set_mode "self-test" || return 1
        SELF_TEST_ROOT="${1#*=}"
        shift
        ;;
      --cleanup-owned)
        set_mode "cleanup-owned" || return 1
        shift
        ;;
      *)
        echo "ERROR: unknown option: $1" >&2
        return 1
        ;;
    esac
  done
}

parse_cli_args "$@" || exit 1

# --- Relay configuration ---
DEFAULT_PHLEX_RELAY_STATE_DIR="${HOME}/.phlex-devcontainer-tmp/relays"

canonical_path() {
  python3 - "$1" <<'PY'
import os
import sys

print(os.path.realpath(sys.argv[1]))
PY
}

if [ "$MODE" = "self-test" ]; then
  if [ -n "${PHLEX_RELAY_STATE_DIR:-}" ]; then
    :
  elif [ -n "$SELF_TEST_ROOT" ]; then
    PHLEX_RELAY_STATE_DIR="${SELF_TEST_ROOT%/}/relay-state"
  else
    echo "ERROR: --self-test requires PHLEX_RELAY_STATE_DIR or a directory argument" >&2
    exit 1
  fi

  canonical_state_dir=$(canonical_path "$PHLEX_RELAY_STATE_DIR") || exit 1
  canonical_default_state_dir=$(canonical_path "$DEFAULT_PHLEX_RELAY_STATE_DIR") || exit 1
  if [ "$canonical_state_dir" = "$canonical_default_state_dir" ]; then
    echo "ERROR: --self-test must not use the production relay state directory" >&2
    exit 1
  fi
else
  PHLEX_RELAY_STATE_DIR="${PHLEX_RELAY_STATE_DIR:-$DEFAULT_PHLEX_RELAY_STATE_DIR}"
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

RELAY_ALLOWLIST="25114=25115,25300=25301"
PHLEX_RELAY_ALLOWED_RANGE="${PHLEX_RELAY_ALLOWED_RANGE:-10.88.0.0/16}"
READ_PID=""

declare -a valid_source_ports=()
declare -a valid_dest_ports=()
SELF_TEST_BLOCKER_PID=""
SELF_TEST_LISTENER_25114_PID=""
SELF_TEST_LISTENER_25300_PID=""
SELF_TEST_ROGUE_PID=""
SELF_TEST_SAVED_HOST_PORTS=""
SELF_TEST_SAVED_HOST_PORTS_SET=0
SELF_TEST_SAVED_TEST_PORTS=""
SELF_TEST_SAVED_TEST_PORTS_SET=0

trim_whitespace() {
  printf '%s' "$1" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//'
}

is_darwin() {
  [[ "$OSTYPE" == darwin* ]]
}

relay_pid_file() {
  printf '%s/%s\n' "$RELAY_PID_DIR" "$1"
}

allowed_destination_port() {
  case "$1" in
    25114)
      printf '25115\n'
      ;;
    25300)
      printf '25301\n'
      ;;
    *)
      return 1
      ;;
  esac
}

is_allowed_mapping() {
  case "$1=$2" in
    25114=25115|25300=25301)
      return 0
      ;;
    *)
      return 1
      ;;
  esac
}

numeric_listen_spec() {
  local dest_port="$1"

  if is_darwin; then
    printf '%s\n' "TCP-LISTEN:${dest_port},bind=127.0.0.1,fork,reuseaddr"
  else
    printf '%s\n' "TCP-LISTEN:${dest_port},fork,reuseaddr,range=${PHLEX_RELAY_ALLOWED_RANGE}"
  fi
}

numeric_connect_spec() {
  printf '%s\n' "TCP:127.0.0.1:$1"
}

loopback_listener_ready_darwin() {
  local port="$1"

  command -v lsof >/dev/null 2>&1 || return 1
  lsof -nP -iTCP:"$port" -sTCP:LISTEN -Fn 2>/dev/null | awk -v v4="n127.0.0.1:$port" -v v6="n[::1]:$port" '
    $0 == v4 || $0 == v6 { found = 1 }
    END { exit !found }
  '
}

relay_listener_ready_darwin() {
  local port="$1"

  command -v lsof >/dev/null 2>&1 || return 1
  lsof -nP -iTCP:"$port" -sTCP:LISTEN -Fn 2>/dev/null | awk -v v4="n127.0.0.1:$port" '
    $0 == v4 { found = 1 }
    END { exit !found }
  '
}

loopback_listener_ready_linux() {
  local port="$1"

  command -v ss >/dev/null 2>&1 || return 1
  ss -H -ltn 2>/dev/null | awk -v v4="127.0.0.1:$port" -v v6="[::1]:$port" '
    $4 == v4 || $4 == v6 { found = 1 }
    END { exit !found }
  '
}

relay_listener_ready_linux() {
  local pid="${2:-}"
  local port="$1"

  command -v ss >/dev/null 2>&1 || return 1
  ss -H -ltn 2>/dev/null | awk -v v4="0.0.0.0:$port" -v v6="[::]:$port" -v star="*:$port" '
    $4 == v4 || $4 == v6 || $4 == star { found = 1 }
    END { exit !found }
  ' || return 1

  if [ -n "$pid" ]; then
    linux_relay_process_has_allowed_range "$pid"
  fi
}

linux_relay_process_has_allowed_range() {
  local cmdline
  local pid="$1"

  cmdline=$(ps -p "$pid" -o command= 2>/dev/null) || return 1
  [[ "$cmdline" == *"range=${PHLEX_RELAY_ALLOWED_RANGE}"* ]]
}

detect_source_listener() {
  if is_darwin; then
    loopback_listener_ready_darwin "$1"
  else
    loopback_listener_ready_linux "$1"
  fi
}

detect_numeric_relay_listener() {
  if is_darwin; then
    relay_listener_ready_darwin "$1"
  else
    relay_listener_ready_linux "$1"
  fi
}

aux_tcp_listener_ready() {
  local port="$1"

  if is_darwin; then
    command -v lsof >/dev/null 2>&1 || return 1
    lsof -nP -iTCP:"$port" -sTCP:LISTEN >/dev/null 2>&1
  else
    command -v ss >/dev/null 2>&1 || return 1
    ss -H -ltn 2>/dev/null | awk -v v4="0.0.0.0:$port" -v v6="[::]:$port" -v star="*:$port" '
      $4 == v4 || $4 == v6 || $4 == star { found = 1 }
      END { exit !found }
    '
  fi
}

numeric_pid_has_listener_darwin() {
  local pid="$1"
  local dest_port="$2"

  command -v lsof >/dev/null 2>&1 || return 1
  lsof -a -p "$pid" -nP -iTCP:"$dest_port" -sTCP:LISTEN -Fn 2>/dev/null | awk -v v4="n127.0.0.1:$dest_port" '
    $0 == v4 { found = 1 }
    END { exit !found }
  '
}

numeric_pid_has_listener_linux() {
  local pid="$1"
  local dest_port="$2"

  command -v ss >/dev/null 2>&1 || return 1
  ss -H -ltnp 2>/dev/null | awk -v pid_fragment="pid=$pid," -v v4="0.0.0.0:$dest_port" -v v6="[::]:$dest_port" -v star="*:$dest_port" '
    ($4 == v4 || $4 == v6 || $4 == star) && index($0, pid_fragment) { found = 1 }
    END { exit !found }
  ' || return 1

  relay_listener_ready_linux "$dest_port" "$pid"
}

numeric_pid_has_listener() {
  if is_darwin; then
    numeric_pid_has_listener_darwin "$1" "$2"
  else
    numeric_pid_has_listener_linux "$1" "$2"
  fi
}

read_numeric_pidfile() {
  local pid_contents
  local pidfile="$1"

  READ_PID=""

  [ -f "$pidfile" ] || return 1
  pid_contents=$(cat "$pidfile" 2>/dev/null) || return 1
  pid_contents=$(trim_whitespace "$pid_contents")
  [[ "$pid_contents" =~ ^[0-9]+$ ]] || return 1

  READ_PID="$pid_contents"
  return 0
}

wait_for_pid_exit() {
  local attempts=0
  local pid="$1"

  while [ "$attempts" -lt 40 ]; do
    if ! kill -0 "$pid" 2>/dev/null; then
      return 0
    fi
    sleep 0.1
    attempts=$((attempts + 1))
  done

  return 1
}

terminate_pid_gracefully() {
  local pid="$1"

  kill "$pid" 2>/dev/null || return 1
  wait_for_pid_exit "$pid"
}

numeric_commandline_matches() {
  local cmdline
  local connect_spec
  local dest_port="$3"
  local listen_spec
  local pid="$1"
  local source_port="$2"

  cmdline=$(ps -p "$pid" -o command= 2>/dev/null) || return 1
  listen_spec=$(numeric_listen_spec "$dest_port")
  connect_spec=$(numeric_connect_spec "$source_port")

  case "$cmdline" in
    *socat*"$listen_spec"*"$connect_spec"*)
      return 0
      ;;
    *socat*"$connect_spec"*"$listen_spec"*)
      return 0
      ;;
    *)
      return 1
      ;;
  esac
}

numeric_relay_is_owned() {
  local dest_port="$3"
  local pid="$1"
  local source_port="$2"

  kill -0 "$pid" 2>/dev/null || return 1
  numeric_commandline_matches "$pid" "$source_port" "$dest_port" || return 1
  numeric_pid_has_listener "$pid" "$dest_port" || return 1

  return 0
}

resolve_existing_numeric_relay_state() {
  local dest_port="$2"
  local pid
  local pidfile
  local source_port="$1"

  pidfile=$(relay_pid_file "$source_port")

  if [ ! -f "$pidfile" ]; then
    return 0
  fi

  if ! read_numeric_pidfile "$pidfile"; then
    rm -f "$pidfile"
    return 0
  fi
  pid="$READ_PID"

  if ! kill -0 "$pid" 2>/dev/null; then
    rm -f "$pidfile"
    return 0
  fi

  if numeric_relay_is_owned "$pid" "$source_port" "$dest_port"; then
    return 10
  fi

  if numeric_commandline_matches "$pid" "$source_port" "$dest_port"; then
    if terminate_pid_gracefully "$pid"; then
      rm -f "$pidfile"
      return 0
    fi

    echo "WARNING: relay PID $pid for ${source_port} -> ${dest_port} did not stop cleanly" >&2
    return 11
  fi

  # PID reuse must not block startup or grant ownership of an unrelated process.
  echo "WARNING: relay PID $pid in $pidfile is not a matching relay; discarding stale PID file" >&2
  rm -f "$pidfile"
  return $?
}

wait_for_source_listener_ready() {
  local attempts=0
  local port="$1"

  while [ "$attempts" -lt 40 ]; do
    if detect_source_listener "$port"; then
      return 0
    fi
    sleep 0.1
    attempts=$((attempts + 1))
  done

  return 1
}

aux_unix_socket_ready() {
  local value="$1"

  [ -S "$value" ] || return 1
  python3 - "$value" <<'PY'
import socket
import sys

sock = socket.socket(socket.AF_UNIX)
sock.settimeout(0.5)
try:
    sock.connect(sys.argv[1])
finally:
    sock.close()
PY
}

aux_endpoint_ready_once() {
  local kind="$1"
  local value="$2"

  case "$kind" in
    unix-socket)
      aux_unix_socket_ready "$value"
      ;;
    tcp-port)
      aux_tcp_listener_ready "$value"
      ;;
    *)
      return 1
      ;;
  esac
}

wait_for_aux_endpoint_ready() {
  local attempts=0
  local kind="$1"
  local value="$2"

  while [ "$attempts" -lt 40 ]; do
    aux_endpoint_ready_once "$kind" "$value" && return 0
    sleep 0.1
    attempts=$((attempts + 1))
  done

  return 1
}

# start_socat_relay LABEL LISTEN_SPEC CONNECT_SPEC LOGFILE READY_KIND READY_VALUE
start_socat_relay() {
  local connect_spec="$3"
  local label="$1"
  local listen_spec="$2"
  local logfile="$4"
  local pid
  local ready_kind="$5"
  local ready_value="$6"

  if ! command -v socat >/dev/null 2>&1; then
    echo "WARNING: socat not found; cannot start ${label} relay" >&2
    return 1
  fi

  if aux_endpoint_ready_once "$ready_kind" "$ready_value"; then
    echo "${label} relay already ready"
    return 0
  fi

  echo "Starting ${label} relay: ${listen_spec} -> ${connect_spec} ..."
  nohup socat "$listen_spec" "$connect_spec" > "$logfile" 2>&1 &
  pid=$!

  if wait_for_aux_endpoint_ready "$ready_kind" "$ready_value"; then
    echo "${label} relay ready"
    return 0
  fi

  if kill -0 "$pid" 2>/dev/null; then
    terminate_pid_gracefully "$pid" >/dev/null 2>&1 || true
  fi

  echo "WARNING: ${label} relay did not become ready in time" >&2
  return 1
}

parse_and_validate_relays() {
  local dest_port
  local duplicate_found
  local invalid=0
  local pair
  local relay_input="${1:-}"
  local seen_source
  local source_port
  local -a relay_pairs=()
  local -a seen_sources=()

  valid_source_ports=()
  valid_dest_ports=()

  if [ -z "$relay_input" ]; then
    return 0
  fi

  IFS=',' read -r -a relay_pairs <<< "$relay_input"

  for pair in "${relay_pairs[@]}"; do
    pair=$(trim_whitespace "$pair")

    if [ -z "$pair" ] || [[ ! "$pair" =~ ^[^=]+=[^=]+$ ]]; then
      echo "ERROR: malformed relay mapping: '$pair' (expected SOURCE=DEST)" >&2
      invalid=1
      continue
    fi

    source_port=$(trim_whitespace "${pair%%=*}")
    dest_port=$(trim_whitespace "${pair#*=}")

    if [[ ! "$source_port" =~ ^[0-9]+$ ]] || [[ ! "$dest_port" =~ ^[0-9]+$ ]]; then
      echo "ERROR: relay mapping ports must be numeric: '$pair'" >&2
      invalid=1
      continue
    fi

    if (( source_port < 1024 || dest_port < 1024 || source_port > 65535 || dest_port > 65535 )); then
      echo "ERROR: relay ports must be between 1024 and 65535: '$pair'" >&2
      invalid=1
      continue
    fi

    if (( source_port == dest_port )); then
      echo "ERROR: source and destination ports must be different: '$pair'" >&2
      invalid=1
      continue
    fi

    duplicate_found=0
    if [ "${#seen_sources[@]}" -gt 0 ]; then
      for seen_source in "${seen_sources[@]}"; do
        if [ "$seen_source" = "$source_port" ]; then
          duplicate_found=1
          break
        fi
      done
    fi
    if [ "$duplicate_found" -eq 1 ]; then
      echo "ERROR: duplicate source port in relay mappings: '$source_port'" >&2
      invalid=1
      continue
    fi
    seen_sources+=("$source_port")

    if ! is_allowed_mapping "$source_port" "$dest_port"; then
      echo "ERROR: relay mapping is not in the allowlist: '$pair'" >&2
      invalid=1
      continue
    fi

    if ! detect_source_listener "$source_port"; then
      echo "ERROR: source port $source_port is not listening on loopback" >&2
      invalid=1
      continue
    fi

    valid_source_ports+=("$source_port")
    valid_dest_ports+=("$dest_port")
  done

  if [ "$invalid" -ne 0 ]; then
    valid_source_ports=()
    valid_dest_ports=()
    return 1
  fi

  return 0
}

write_relay_maps() {
  local i=0
  local pair_count=0
  local sorted_pairs=""

  if [ -n "${valid_source_ports[*]:-}" ]; then
    pair_count=${#valid_source_ports[@]}
  fi

  if [ "$pair_count" -eq 0 ]; then
    printf '{}\n' > "$RELAY_MAP_JSON"
    : > "$RELAY_MAP_ENV"
    return 0
  fi

  sorted_pairs=$(for ((i = 0; i < pair_count; i++)); do
    printf '%s %s\n' "${valid_source_ports[$i]}" "${valid_dest_ports[$i]}"
  done | LC_ALL=C sort -k1,1n) || return 1

  {
    local dest_port
    local first=1
    local source_port

    printf '{'
    while IFS=' ' read -r source_port dest_port; do
      [ -n "$source_port" ] || continue
      if [ "$first" -eq 1 ]; then
        first=0
      else
        printf ','
      fi
      printf '"%s":"%s"' "$source_port" "$dest_port"
    done <<EOF
$sorted_pairs
EOF
    printf '}\n'
  } > "$RELAY_MAP_JSON"

  {
    local dest_port
    local source_port

    while IFS=' ' read -r source_port dest_port; do
      [ -n "$source_port" ] || continue
      printf 'PHLEX_HOST_RELAY_%s=%s\n' "$source_port" "$dest_port"
    done <<EOF
$sorted_pairs
EOF
  } > "$RELAY_MAP_ENV"
}

start_numeric_relay() {
  local attempts=0
  local connect_spec
  local dest_port="$2"
  local listen_spec
  local logfile
  local pid
  local pidfile
  local source_port="$1"
  local state_status=0

  if ! command -v socat >/dev/null 2>&1; then
    echo "ERROR: socat not found; cannot start host relay ${source_port}=${dest_port}" >&2
    return 1
  fi

  pidfile=$(relay_pid_file "$source_port")
  resolve_existing_numeric_relay_state "$source_port" "$dest_port"
  state_status=$?
  case "$state_status" in
    0)
      ;;
    10)
      echo "Host relay $source_port -> $dest_port already ready"
      return 0
      ;;
    11)
      return 1
      ;;
    *)
      echo "WARNING: unexpected relay state status $state_status for ${source_port}=${dest_port}" >&2
      return 1
      ;;
  esac

  if detect_numeric_relay_listener "$dest_port"; then
    echo "WARNING: destination port $dest_port is already listening but not owned by this state" >&2
    return 1
  fi

  listen_spec=$(numeric_listen_spec "$dest_port")
  connect_spec=$(numeric_connect_spec "$source_port")
  logfile="${PHLEX_RELAY_STATE_DIR}/relay-${source_port}-${dest_port}.log"
  mkdir -p "$RELAY_PID_DIR"

  echo "Starting host relay: 127.0.0.1:${source_port} -> ${dest_port}"
  nohup socat "$listen_spec" "$connect_spec" > "$logfile" 2>&1 &
  pid=$!
  printf '%s\n' "$pid" > "$pidfile"

  while [ "$attempts" -lt 40 ]; do
    if numeric_relay_is_owned "$pid" "$source_port" "$dest_port"; then
      echo "Host relay $source_port -> $dest_port ready"
      return 0
    fi

    if ! kill -0 "$pid" 2>/dev/null; then
      break
    fi

    sleep 0.1
    attempts=$((attempts + 1))
  done

  echo "WARNING: host relay $source_port -> $dest_port did not become ready in time" >&2

  if kill -0 "$pid" 2>/dev/null && numeric_commandline_matches "$pid" "$source_port" "$dest_port"; then
    if ! terminate_pid_gracefully "$pid"; then
      echo "WARNING: failed relay PID $pid for ${source_port}=${dest_port} did not stop cleanly" >&2
      return 1
    fi
  fi

  rm -f "$pidfile"
  return 1
}

apply_requested_relays() {
  local i=0
  local overall_status=0
  local relay_input="$1"
  local requested_count=0
  local -a ready_dest_ports=()
  local -a ready_source_ports=()

  if ! parse_and_validate_relays "$relay_input"; then
    write_relay_maps
    return 1
  fi

  if [ -n "${valid_source_ports[*]:-}" ]; then
    requested_count=${#valid_source_ports[@]}
  fi

  for ((i = 0; i < requested_count; i++)); do
    if start_numeric_relay "${valid_source_ports[$i]}" "${valid_dest_ports[$i]}"; then
      ready_source_ports+=("${valid_source_ports[$i]}")
      ready_dest_ports+=("${valid_dest_ports[$i]}")
    else
      overall_status=1
    fi
  done

  if [ "${#ready_source_ports[@]}" -gt 0 ]; then
    valid_source_ports=("${ready_source_ports[@]}")
    valid_dest_ports=("${ready_dest_ports[@]}")
  else
    valid_source_ports=()
    valid_dest_ports=()
  fi

  write_relay_maps
  return "$overall_status"
}

mode_default() {
  echo "Running in default mode with PHLEX_HOST_RELAY_PORTS"

  if [ -n "${PHLEX_HOST_RELAY_PORTS:-}" ]; then
    apply_requested_relays "$PHLEX_HOST_RELAY_PORTS"
    return $?
  fi

  echo "No PHLEX_HOST_RELAY_PORTS configured; using defaults"
  valid_source_ports=()
  valid_dest_ports=()
  write_relay_maps
  return 0
}

mode_start_test_services() {
  local test_ports="${PHLEX_TEST_RELAY_PORTS:-$RELAY_ALLOWLIST}"

  echo "Starting test relay services"
  apply_requested_relays "$test_ports"
}

cleanup_owned_numeric_relay() {
  local dest_port
  local pid
  local pidfile
  local source_port="$1"

  pidfile=$(relay_pid_file "$source_port")
  [ -f "$pidfile" ] || return 0

  dest_port=$(allowed_destination_port "$source_port") || {
    echo "WARNING: relay PID file for unallowlisted source $source_port left untouched" >&2
    return 1
  }

  if ! read_numeric_pidfile "$pidfile"; then
    rm -f "$pidfile"
    return 0
  fi
  pid="$READ_PID"

  if ! kill -0 "$pid" 2>/dev/null; then
    rm -f "$pidfile"
    return 0
  fi

  if ! numeric_commandline_matches "$pid" "$source_port" "$dest_port"; then
    echo "WARNING: relay PID $pid in $pidfile is not a matching relay; discarding stale PID file" >&2
    rm -f "$pidfile"
    return $?
  fi

  if ! numeric_relay_is_owned "$pid" "$source_port" "$dest_port"; then
    echo "WARNING: relay PID $pid for ${source_port} -> ${dest_port} was not proven owned; leaving it untouched" >&2
    return 1
  fi

  if ! terminate_pid_gracefully "$pid"; then
    echo "WARNING: relay PID $pid for ${source_port} -> ${dest_port} did not stop cleanly" >&2
    return 1
  fi

  rm -f "$pidfile"
  return 0
}

mode_cleanup_owned() {
  local cleanup_failed=0
  local pidfile
  local source_port

  echo "Cleaning up owned relay processes"

  if [ -d "$RELAY_PID_DIR" ]; then
    for pidfile in "$RELAY_PID_DIR"/*; do
      [ -e "$pidfile" ] || continue
      source_port=$(basename "$pidfile")
      case "$source_port" in
        '' | *[!0-9]*)
          continue
          ;;
      esac
      if ! cleanup_owned_numeric_relay "$source_port"; then
        cleanup_failed=1
      fi
    done
  fi

  valid_source_ports=()
  valid_dest_ports=()
  write_relay_maps

  if [ "$cleanup_failed" -ne 0 ]; then
    echo "WARNING: one or more relay processes were not proven owned and were left untouched" >&2
    return 1
  fi

  echo "Cleanup completed"
  return 0
}

mode_self_test() {
  # Production startup and cleanup do not need fixture listeners or assertions.
  if ! source "${SCRIPT_DIR}/ensure-repos-self-test.sh"; then
    echo "ERROR: failed to source self-test helper" >&2
    return 1
  fi
  mode_self_test_loaded
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
# macOS sockets cannot cross the VM's shared filesystem. configure-host.py
# selects the VM-side socket and pasta reaches host loopback services directly.
if ! is_darwin; then
AUX_RELAY_LOG_DIR="${HOME}/.phlex-devcontainer-tmp/relay-logs"
USER_ID=$(id -u)
PODMAN_REAL_SOCKET="${XDG_RUNTIME_DIR:-/run/user/${USER_ID}}/podman/podman.sock"
PROXY_DIR="${HOME}/.podman-proxy"
PROXY_SOCKET="${PROXY_DIR}/podman.sock"

ensure_bind_dir -m 0700 "${AUX_RELAY_LOG_DIR}"
ensure_bind_dir -m 0700 "${PROXY_DIR}"

if [ -S "${PODMAN_REAL_SOCKET}" ]; then
  start_socat_relay \
    "Podman socket" \
    "UNIX-LISTEN:${PROXY_SOCKET},fork,reuseaddr,unlink-early" \
    "UNIX-CONNECT:${PODMAN_REAL_SOCKET}" \
    "${AUX_RELAY_LOG_DIR}/socat-podman.log" \
    "unix-socket" \
    "${PROXY_SOCKET}" || true
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

  if detect_source_listener "${proxy_port}"; then
    start_socat_relay \
      "Headroom ${role} proxy" \
      "TCP-LISTEN:${relay_port},fork,reuseaddr" \
      "TCP:${local_addr}" \
      "${AUX_RELAY_LOG_DIR}/socat-headroom-${role}.log" \
      "tcp-port" \
      "${relay_port}" || true
  else
    echo "WARNING: headroom ${role} proxy not detected at ${local_addr}; skipping relay" >&2
    echo "  Ensure the SSH tunnel is active (headroom-${role} running on your laptop)" >&2
  fi
}

relay_headroom_port azure "${HEADROOM_AZURE_PORT}"
relay_headroom_port ow "${HEADROOM_OW_PORT}"
fi

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

# The initializer is a child process: exporting these settings here cannot
# affect the later Compose process launched by VS Code.
python3 "${SCRIPT_DIR}/configure-host.py" || exit 1

echo "SUCCESS: .devcontainer/ensure-repos.sh completed successfully"
