#!/bin/bash

# Helper functions for self-test mode - sourced, not executed
# This file contains test-only utilities and the mode_self_test implementation
# It must be sourced by ensure-repos.sh, not executed directly

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
  printf '%s\n' 'ERROR: ensure-repos-self-test.sh must be sourced, not executed' >&2
  exit 1
fi

file_sha256() {
  local path="$1"

  if command -v shasum >/dev/null 2>&1; then
    shasum -a 256 "$path" | awk '{print $1}'
  elif command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$path" | awk '{print $1}'
  else
    return 1
  fi
}

start_python_loopback_listener() {
  local label="$2"
  local port="$1"

  python3 - "$port" "$label" <<'PY' >/dev/null 2>&1 &
import signal
import socket
import sys

port = int(sys.argv[1])
label = sys.argv[2].encode("utf-8") + b"\n"
stop = False

def handle_signal(signum, frame):
    del signum, frame
    global stop
    stop = True

signal.signal(signal.SIGTERM, handle_signal)
signal.signal(signal.SIGINT, handle_signal)

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
sock.bind(("127.0.0.1", port))
sock.listen(16)
sock.settimeout(0.2)

while not stop:
    try:
        conn, _ = sock.accept()
    except socket.timeout:
        continue
    except OSError:
        break

    with conn:
        try:
            conn.sendall(label)
        except OSError:
            pass

sock.close()
PY

  printf '%s\n' "$!"
}

probe_python_tcp_message() {
  local expected_label="$2"
  local port="$1"

  python3 - "$port" "$expected_label" <<'PY'
import socket
import sys

port = int(sys.argv[1])
expected = (sys.argv[2] + "\n").encode("utf-8")

sock = socket.create_connection(("127.0.0.1", port), timeout=2.0)
try:
    data = sock.recv(128)
finally:
    sock.close()

if data != expected:
    raise SystemExit(1)
PY
}

# mode_self_test_loaded implements the actual self-test logic
# This function must be called from mode_self_test() in ensure-repos.sh
mode_self_test_loaded() {
  local PHLEX_RELAY_ALLOWED_RANGE="127.0.0.0/8"
  local default_env="${DEFAULT_PHLEX_RELAY_STATE_DIR}/relay-map.env"
  local default_env_hash_before=""
  local default_env_missing_before=0
  local default_json="${DEFAULT_PHLEX_RELAY_STATE_DIR}/relay-map.json"
  local default_json_hash_before=""
  local default_json_missing_before=0
  local test_failed=0

  SELF_TEST_BLOCKER_PID=""
  SELF_TEST_LISTENER_25114_PID=""
  SELF_TEST_LISTENER_25300_PID=""
  SELF_TEST_ROGUE_PID=""
  SELF_TEST_SAVED_HOST_PORTS=""
  SELF_TEST_SAVED_HOST_PORTS_SET=0
  SELF_TEST_SAVED_TEST_PORTS=""
  SELF_TEST_SAVED_TEST_PORTS_SET=0
  if [ "${PHLEX_HOST_RELAY_PORTS+x}" = x ]; then
    SELF_TEST_SAVED_HOST_PORTS="$PHLEX_HOST_RELAY_PORTS"
    SELF_TEST_SAVED_HOST_PORTS_SET=1
  fi
  if [ "${PHLEX_TEST_RELAY_PORTS+x}" = x ]; then
    SELF_TEST_SAVED_TEST_PORTS="$PHLEX_TEST_RELAY_PORTS"
    SELF_TEST_SAVED_TEST_PORTS_SET=1
  fi

  assert_file_content() {
    local actual=""
    local expected="$1"
    local label="$3"
    local path="$2"

    if [ -f "$path" ]; then
      actual=$(cat "$path")
    fi

    if [ "$actual" != "$expected" ]; then
      echo "ERROR: ${label} mismatch" >&2
      echo "  expected: $expected" >&2
      echo "  actual:   $actual" >&2
      return 1
    fi

    return 0
  }

  assert_empty_state() {
    local pidfile
    local pidfile_count=0

    assert_file_content '{}' "$RELAY_MAP_JSON" 'empty relay-map.json' || return 1
    [ ! -s "$RELAY_MAP_ENV" ] || {
      echo "ERROR: expected empty relay-map.env" >&2
      return 1
    }

    for pidfile in "$RELAY_PID_DIR"/*; do
      [ -e "$pidfile" ] || continue
      case "$(basename "$pidfile")" in
        '' | *[!0-9]*)
          continue
          ;;
        *)
          pidfile_count=$((pidfile_count + 1))
          ;;
      esac
    done
    if [ "$pidfile_count" -ne 0 ]; then
      echo "ERROR: expected no numeric relay PID files, found $pidfile_count" >&2
      return 1
    fi

    if detect_numeric_relay_listener 25115 || detect_numeric_relay_listener 25301; then
      echo "ERROR: expected no ready numeric relay listeners" >&2
      return 1
    fi

    return 0
  }

  restore_saved_environment() {
    if [ "$SELF_TEST_SAVED_HOST_PORTS_SET" -eq 0 ]; then
      unset PHLEX_HOST_RELAY_PORTS
    else
      PHLEX_HOST_RELAY_PORTS="$SELF_TEST_SAVED_HOST_PORTS"
    fi

    if [ "$SELF_TEST_SAVED_TEST_PORTS_SET" -eq 0 ]; then
      unset PHLEX_TEST_RELAY_PORTS
    else
      PHLEX_TEST_RELAY_PORTS="$SELF_TEST_SAVED_TEST_PORTS"
    fi
  }

  self_test_cleanup() {
    mode_cleanup_owned >/dev/null 2>&1 || true

    if [ -n "$SELF_TEST_BLOCKER_PID" ] && kill -0 "$SELF_TEST_BLOCKER_PID" 2>/dev/null; then
      terminate_pid_gracefully "$SELF_TEST_BLOCKER_PID" >/dev/null 2>&1 || true
    fi
    SELF_TEST_BLOCKER_PID=""

    if [ -n "$SELF_TEST_ROGUE_PID" ] && kill -0 "$SELF_TEST_ROGUE_PID" 2>/dev/null; then
      terminate_pid_gracefully "$SELF_TEST_ROGUE_PID" >/dev/null 2>&1 || true
    fi
    SELF_TEST_ROGUE_PID=""

    if [ -n "$SELF_TEST_LISTENER_25114_PID" ] && kill -0 "$SELF_TEST_LISTENER_25114_PID" 2>/dev/null; then
      terminate_pid_gracefully "$SELF_TEST_LISTENER_25114_PID" >/dev/null 2>&1 || true
    fi
    SELF_TEST_LISTENER_25114_PID=""

    if [ -n "$SELF_TEST_LISTENER_25300_PID" ] && kill -0 "$SELF_TEST_LISTENER_25300_PID" 2>/dev/null; then
      terminate_pid_gracefully "$SELF_TEST_LISTENER_25300_PID" >/dev/null 2>&1 || true
    fi
    SELF_TEST_LISTENER_25300_PID=""

    restore_saved_environment
  }

  trap 'self_test_cleanup' EXIT
  trap 'trap - EXIT; self_test_cleanup; exit 129' HUP
  trap 'trap - EXIT; self_test_cleanup; exit 130' INT
  trap 'trap - EXIT; self_test_cleanup; exit 143' TERM

  mkdir -p "$PHLEX_RELAY_STATE_DIR" "$RELAY_PID_DIR"
  write_relay_maps

  if [ -f "$default_json" ]; then
    default_json_hash_before=$(file_sha256 "$default_json") || return 1
  else
    default_json_missing_before=1
  fi

  if [ -f "$default_env" ]; then
    default_env_hash_before=$(file_sha256 "$default_env") || return 1
  else
    default_env_missing_before=1
  fi

  SELF_TEST_LISTENER_25114_PID=$(start_python_loopback_listener 25114 25114)
  SELF_TEST_LISTENER_25300_PID=$(start_python_loopback_listener 25300 25300)
  wait_for_source_listener_ready 25114 || {
    echo "ERROR: source listener 25114 did not become ready" >&2
    return 1
  }
  wait_for_source_listener_ready 25300 || {
    echo "ERROR: source listener 25300 did not become ready" >&2
    return 1
  }

  echo "Self-test: ready named services"
  PHLEX_TEST_RELAY_PORTS="$RELAY_ALLOWLIST"
  if ! mode_start_test_services; then
    echo "ERROR: --start-test-services positive path failed" >&2
    test_failed=1
  fi
  assert_file_content '{"25114":"25115","25300":"25301"}' "$RELAY_MAP_JSON" 'ready relay-map.json' || test_failed=1
  assert_file_content $'PHLEX_HOST_RELAY_25114=25115\nPHLEX_HOST_RELAY_25300=25301' "$RELAY_MAP_ENV" 'ready relay-map.env' || test_failed=1
  probe_python_tcp_message 25115 25114 || {
    echo "ERROR: relay 25115 did not reach source 25114" >&2
    test_failed=1
  }
  probe_python_tcp_message 25301 25300 || {
    echo "ERROR: relay 25301 did not reach source 25300" >&2
    test_failed=1
  }

  local first_pid_25114=""
  local first_pid_25300=""
  first_pid_25114=$(cat "$(relay_pid_file 25114)" 2>/dev/null || true)
  first_pid_25300=$(cat "$(relay_pid_file 25300)" 2>/dev/null || true)
  [[ "$first_pid_25114" =~ ^[0-9]+$ ]] || {
    echo "ERROR: missing numeric PID file for 25114" >&2
    test_failed=1
  }
  [[ "$first_pid_25300" =~ ^[0-9]+$ ]] || {
    echo "ERROR: missing numeric PID file for 25300" >&2
    test_failed=1
  }

  echo "Self-test: repeat convergence"
  if ! mode_start_test_services; then
    echo "ERROR: repeat --start-test-services failed" >&2
    test_failed=1
  fi
  if [ "$first_pid_25114" != "$(cat "$(relay_pid_file 25114)" 2>/dev/null || true)" ]; then
    echo "ERROR: relay 25114 did not converge on the same owned PID" >&2
    test_failed=1
  fi
  if [ "$first_pid_25300" != "$(cat "$(relay_pid_file 25300)" 2>/dev/null || true)" ]; then
    echo "ERROR: relay 25300 did not converge on the same owned PID" >&2
    test_failed=1
  fi

  echo "Self-test: deterministic ordering"
  PHLEX_HOST_RELAY_PORTS="25300=25301,25114=25115"
  if ! mode_default; then
    echo "ERROR: reverse-order relay admission failed" >&2
    test_failed=1
  fi
  assert_file_content '{"25114":"25115","25300":"25301"}' "$RELAY_MAP_JSON" 'sorted relay-map.json' || test_failed=1
  assert_file_content $'PHLEX_HOST_RELAY_25114=25115\nPHLEX_HOST_RELAY_25300=25301' "$RELAY_MAP_ENV" 'sorted relay-map.env' || test_failed=1

  echo "Self-test: positive cleanup"
  if ! mode_cleanup_owned; then
    echo "ERROR: cleanup of owned relays failed" >&2
    test_failed=1
  fi
  assert_empty_state || test_failed=1

  echo "Self-test: listener-start failure"
  SELF_TEST_BLOCKER_PID=$(start_python_loopback_listener 25115 blocker)
  wait_for_source_listener_ready 25115 || {
    echo "ERROR: blocking listener on 25115 did not become ready" >&2
    return 1
  }
  PHLEX_HOST_RELAY_PORTS="$RELAY_ALLOWLIST"
  if mode_default; then
    echo "ERROR: expected listener-start failure when 25115 is already listening" >&2
    test_failed=1
  fi
  assert_file_content '{"25300":"25301"}' "$RELAY_MAP_JSON" 'partial relay-map.json after listener-start failure' || test_failed=1
  assert_file_content 'PHLEX_HOST_RELAY_25300=25301' "$RELAY_MAP_ENV" 'partial relay-map.env after listener-start failure' || test_failed=1
  [ ! -f "$(relay_pid_file 25114)" ] || {
    echo "ERROR: failed relay 25114 left a PID file behind" >&2
    test_failed=1
  }
  probe_python_tcp_message 25301 25300 || {
    echo "ERROR: surviving relay 25301 did not reach source 25300" >&2
    test_failed=1
  }
  terminate_pid_gracefully "$SELF_TEST_BLOCKER_PID" >/dev/null 2>&1 || {
    echo "ERROR: failed to stop blocking listener on 25115" >&2
    test_failed=1
  }
  SELF_TEST_BLOCKER_PID=""
  if ! mode_cleanup_owned; then
    echo "ERROR: cleanup after listener-start failure failed" >&2
    test_failed=1
  fi
  assert_empty_state || test_failed=1

  echo "Self-test: malformed input"
  PHLEX_HOST_RELAY_PORTS="25114=25115,garbage"
  if mode_default; then
    echo "ERROR: malformed relay input unexpectedly succeeded" >&2
    test_failed=1
  fi
  assert_empty_state || test_failed=1

  echo "Self-test: duplicate input"
  PHLEX_HOST_RELAY_PORTS="25114=25115,25114=25115"
  if mode_default; then
    echo "ERROR: duplicate relay input unexpectedly succeeded" >&2
    test_failed=1
  fi
  assert_empty_state || test_failed=1

  echo "Self-test: privileged input"
  PHLEX_HOST_RELAY_PORTS="25114=25115,80=8080"
  if mode_default; then
    echo "ERROR: privileged relay input unexpectedly succeeded" >&2
    test_failed=1
  fi
  assert_empty_state || test_failed=1

  echo "Self-test: equal-port input"
  PHLEX_HOST_RELAY_PORTS="25114=25115,25300=25300"
  if mode_default; then
    echo "ERROR: equal-port relay input unexpectedly succeeded" >&2
    test_failed=1
  fi
  assert_empty_state || test_failed=1

  echo "Self-test: unlisted input"
  PHLEX_HOST_RELAY_PORTS="25114=25115,25300=25302"
  if mode_default; then
    echo "ERROR: unlisted relay input unexpectedly succeeded" >&2
    test_failed=1
  fi
  assert_empty_state || test_failed=1

  echo "Self-test: unavailable source input"
  if ! terminate_pid_gracefully "$SELF_TEST_LISTENER_25300_PID" >/dev/null 2>&1; then
    echo "ERROR: failed to stop source listener 25300 for unavailable-source test" >&2
    test_failed=1
  fi
  SELF_TEST_LISTENER_25300_PID=""
  PHLEX_HOST_RELAY_PORTS="$RELAY_ALLOWLIST"
  if mode_default; then
    echo "ERROR: unavailable source relay input unexpectedly succeeded" >&2
    test_failed=1
  fi
  assert_empty_state || test_failed=1
  SELF_TEST_LISTENER_25300_PID=$(start_python_loopback_listener 25300 25300)
  wait_for_source_listener_ready 25300 || {
    echo "ERROR: source listener 25300 did not restart" >&2
    return 1
  }

  echo "Self-test: cleanup ownership boundary"
  SELF_TEST_ROGUE_PID=$(start_python_loopback_listener 26001 rogue)
  wait_for_source_listener_ready 26001 || {
    echo "ERROR: rogue process listener 26001 did not become ready" >&2
    return 1
  }
  printf '%s\n' "$SELF_TEST_ROGUE_PID" > "$(relay_pid_file 25114)"
  if ! mode_cleanup_owned; then
    echo "ERROR: cleanup-owned failed to discard a stale PID file" >&2
    test_failed=1
  fi
  if ! kill -0 "$SELF_TEST_ROGUE_PID" 2>/dev/null; then
    echo "ERROR: cleanup-owned terminated an unrelated process" >&2
    test_failed=1
  fi
  [ ! -f "$(relay_pid_file 25114)" ] || {
    echo "ERROR: cleanup-owned retained a stale PID file" >&2
    test_failed=1
  }
  assert_empty_state || test_failed=1

  echo "Self-test: startup after PID reuse"
  printf '%s\n' "$SELF_TEST_ROGUE_PID" > "$(relay_pid_file 25114)"
  PHLEX_HOST_RELAY_PORTS="$RELAY_ALLOWLIST"
  if ! mode_default; then
    echo "ERROR: startup failed to recover from a stale PID file" >&2
    test_failed=1
  fi
  probe_python_tcp_message 25115 25114 || test_failed=1
  probe_python_tcp_message 25301 25300 || test_failed=1
  if ! kill -0 "$SELF_TEST_ROGUE_PID" 2>/dev/null; then
    echo "ERROR: startup terminated an unrelated process" >&2
    test_failed=1
  fi
  mode_cleanup_owned || test_failed=1
  terminate_pid_gracefully "$SELF_TEST_ROGUE_PID" >/dev/null 2>&1 || {
    echo "ERROR: failed to stop rogue process listener 26001" >&2
    test_failed=1
  }
  SELF_TEST_ROGUE_PID=""
  assert_empty_state || test_failed=1

  if [ "$default_json_missing_before" -eq 1 ]; then
    [ ! -e "$default_json" ] || {
      echo "ERROR: self-test created production relay-map.json" >&2
      test_failed=1
    }
  else
    [ "$(file_sha256 "$default_json")" = "$default_json_hash_before" ] || {
      echo "ERROR: self-test modified production relay-map.json" >&2
      test_failed=1
    }
  fi

  if [ "$default_env_missing_before" -eq 1 ]; then
    [ ! -e "$default_env" ] || {
      echo "ERROR: self-test created production relay-map.env" >&2
      test_failed=1
    }
  else
    [ "$(file_sha256 "$default_env")" = "$default_env_hash_before" ] || {
      echo "ERROR: self-test modified production relay-map.env" >&2
      test_failed=1
    }
  fi

  restore_saved_environment
  trap - EXIT HUP INT TERM
  self_test_cleanup

  if [ "$test_failed" -ne 0 ]; then
    echo "Self-test FAILED" >&2
    return 1
  fi

  echo "Self-test passed"
  return 0
}
