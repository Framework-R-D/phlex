#!/usr/bin/env bash
# Prepare KILO_CONFIG_CONTENT with rewritten loopback URLs based on relay map.
# This script is sourced by /etc/profile.d/ so the variable is available to all processes.
# Conforms to kilo-plan/v5: write-free Kilo configuration rewriting.

KILO_ENV_FIXED_ROOT="/run/phlex-host-relays"
KILO_ENV_HOST_GATEWAY="host.docker.internal"
KILO_ENV_SELF_TEST_MODE=0

kilo_env_is_sourced() {
  [ "${BASH_SOURCE[0]-}" != "$0" ]
}

kilo_env_print_error() {
  printf 'kilo-env: %s\n' "$*" >&2
}

kilo_env_unset_var_quietly() {
  local variable_name
  variable_name="$1"
  unset "$variable_name" 2>/dev/null || true
}

kilo_env_clear_dynamic_exports() {
  local relay_variables
  local variable_name

  kilo_env_unset_var_quietly "KILO_CONFIG_CONTENT"

  relay_variables=$(compgen -A variable PHLEX_HOST_RELAY_ 2>/dev/null || true)
  if [ -z "$relay_variables" ]; then
    return 0
  fi

  while IFS= read -r variable_name; do
    if [[ "$variable_name" =~ ^PHLEX_HOST_RELAY_[0-9]+$ ]]; then
      kilo_env_unset_var_quietly "$variable_name"
    fi
  done <<EOF
$relay_variables
EOF

  return 0
}

kilo_env_clear_failed_exports() {
  kilo_env_clear_dynamic_exports
  kilo_env_unset_var_quietly "KILO_CONFIG_PATH"
}

kilo_env_find_config_path() {
  local relay_root
  local candidate

  relay_root="$1"

  for candidate in "${relay_root}/kilo.json" "${relay_root}/kilo.jsonc"; do
    if [ -f "$candidate" ]; then
      printf '%s\n' "$candidate"
      return 0
    fi
  done

  return 1
}

kilo_env_export_path_state() {
  local relay_root
  local config_path

  relay_root="$1"
  config_path=""

  RELAY_MAP_JSON="${relay_root}/relay-map.json"
  RELAY_MAP_ENV="${relay_root}/relay-map.env"

  export PHLEX_HOST_GATEWAY="$KILO_ENV_HOST_GATEWAY"
  export RELAY_MAP_JSON
  export PHLEX_HOST_RELAY_FILE="$RELAY_MAP_JSON"
  export PHLEX_HOST_RELAYS_ENV="$RELAY_MAP_ENV"

  if config_path=$(kilo_env_find_config_path "$relay_root"); then
    KILO_CONFIG_PATH="$config_path"
    export KILO_CONFIG_PATH
  else
    kilo_env_unset_var_quietly "KILO_CONFIG_PATH"
  fi
}

kilo_env_collect_exports() {
  local map_path
  local env_path
  local config_path

  map_path="$1"
  env_path="$2"
  config_path="$3"

  python3 - "$map_path" "$env_path" "$config_path" "$KILO_ENV_HOST_GATEWAY" <<'PY'
from __future__ import annotations

import json
import re
import sys
from pathlib import Path
from urllib.parse import urlsplit, urlunsplit

ALLOWED_RELAYS = {"25114": "25115", "25300": "25301"}
ENV_PATTERN = re.compile(r"^PHLEX_HOST_RELAY_(\d+)=(\d+)$")
LOOPBACK_HOSTS = {"127.0.0.1", "localhost", "::1"}


def fail(message: str) -> None:
    raise ValueError(message)


def read_text(path_str: str, label: str) -> str:
    path = Path(path_str)
    try:
        return path.read_text(encoding="utf-8")
    except FileNotFoundError as exc:
        raise ValueError(f"{label} missing: {path}") from exc
    except UnicodeDecodeError as exc:
        raise ValueError(f"{label} is not valid UTF-8: {path}") from exc
    except OSError as exc:
        raise ValueError(f"unable to read {label}: {path}: {exc}") from exc


def reject_duplicate_object(pairs: list[tuple[str, object]]) -> dict[str, object]:
    result: dict[str, object] = {}
    for key, value in pairs:
        if key in result:
            fail(f"duplicate JSON object key: {key}")
        result[key] = value
    return result


def parse_relay_map(text: str) -> dict[str, str]:
    try:
        parsed = json.loads(text, object_pairs_hook=reject_duplicate_object)
    except json.JSONDecodeError as exc:
        raise ValueError(f"invalid relay map JSON: {exc.msg}") from exc

    if not isinstance(parsed, dict):
        fail("relay map must be a JSON object")

    canonical = json.dumps(parsed, separators=(",", ":"), sort_keys=True)
    if text not in {canonical, canonical + "\n"}:
        fail("relay map must be a sorted canonical JSON object")

    relay_map: dict[str, str] = {}
    for source_port, relay_port in parsed.items():
        if not isinstance(source_port, str) or not isinstance(relay_port, str):
            fail("relay map entries must be string-to-string port mappings")
        if source_port not in ALLOWED_RELAYS or ALLOWED_RELAYS[source_port] != relay_port:
            fail(f"relay map entry {source_port}={relay_port} is not allowlisted")
        relay_map[source_port] = relay_port

    return relay_map


def parse_relay_env(text: str, relay_map: dict[str, str]) -> dict[str, str]:
    if not relay_map and text in {"", "\n"}:
        return {}

    parsed_env: dict[str, str] = {}
    for line in text.splitlines():
        match = ENV_PATTERN.fullmatch(line)
        if match is None:
            fail("relay env must contain only exact PHLEX_HOST_RELAY_<source>=<relay> lines")
        source_port, relay_port = match.groups()
        variable_name = f"PHLEX_HOST_RELAY_{source_port}"
        if variable_name in parsed_env:
            fail(f"duplicate relay env entry for source port {source_port}")
        parsed_env[variable_name] = relay_port

    expected_lines = [
        f"PHLEX_HOST_RELAY_{source_port}={relay_map[source_port]}"
        for source_port in sorted(relay_map)
    ]
    canonical_env = "\n".join(expected_lines)
    accepted = {canonical_env}
    if canonical_env:
        accepted.add(canonical_env + "\n")
    if text not in accepted:
        fail("relay env must be sorted canonical PHLEX_HOST_RELAY_<source>=<relay> lines")

    expected_names = {f"PHLEX_HOST_RELAY_{source_port}" for source_port in relay_map}
    if set(parsed_env) != expected_names:
        fail("relay env keys do not match relay map keys")

    for source_port, relay_port in relay_map.items():
        variable_name = f"PHLEX_HOST_RELAY_{source_port}"
        if parsed_env[variable_name] != relay_port:
            fail(f"relay env entry {variable_name} does not match relay map value")

    return parsed_env


def strip_jsonc(text: str) -> str:
    without_comments: list[str] = []
    in_string = False
    escape = False
    index = 0

    while index < len(text):
        char = text[index]

        if in_string:
            without_comments.append(char)
            if escape:
                escape = False
            elif char == "\\":
                escape = True
            elif char == '"':
                in_string = False
            index += 1
            continue

        if char == '"':
            in_string = True
            without_comments.append(char)
            index += 1
            continue

        if char == "/" and index + 1 < len(text):
            next_char = text[index + 1]
            if next_char == "/":
                index += 2
                while index < len(text) and text[index] != "\n":
                    index += 1
                continue
            if next_char == "*":
                index += 2
                while index + 1 < len(text) and not (text[index] == "*" and text[index + 1] == "/"):
                    index += 1
                if index + 1 >= len(text):
                    fail("unterminated block comment in Kilo config")
                index += 2
                continue

        without_comments.append(char)
        index += 1

    if in_string or escape:
        fail("unterminated string in Kilo config")

    stripped = "".join(without_comments)
    without_trailing_commas: list[str] = []
    in_string = False
    escape = False
    index = 0

    while index < len(stripped):
        char = stripped[index]

        if in_string:
            without_trailing_commas.append(char)
            if escape:
                escape = False
            elif char == "\\":
                escape = True
            elif char == '"':
                in_string = False
            index += 1
            continue

        if char == '"':
            in_string = True
            without_trailing_commas.append(char)
            index += 1
            continue

        if char == ",":
            look_ahead = index + 1
            while look_ahead < len(stripped) and stripped[look_ahead] in " \t\r\n":
                look_ahead += 1
            if look_ahead < len(stripped) and stripped[look_ahead] in "]}":
                index += 1
                continue

        without_trailing_commas.append(char)
        index += 1

    if in_string or escape:
        fail("unterminated string in Kilo config")

    return "".join(without_trailing_commas)


def parse_config(config_path: str) -> object | None:
    if not config_path:
        return None

    cleaned = strip_jsonc(read_text(config_path, "Kilo config"))
    try:
        return json.loads(cleaned, object_pairs_hook=reject_duplicate_object)
    except json.JSONDecodeError as exc:
        raise ValueError(f"invalid Kilo config JSON/JSONC: {exc.msg}") from exc


def rewrite_url(value: str, relay_map: dict[str, str], host_gateway: str) -> str:
    parsed = urlsplit(value)
    if parsed.scheme not in {"http", "https"}:
        return value

    hostname = parsed.hostname
    if hostname is None or hostname.lower() not in LOOPBACK_HOSTS:
        return value

    try:
        source_port = parsed.port
    except ValueError as exc:
        raise ValueError(f"invalid loopback URL port in Kilo config: {value}") from exc

    if source_port is None:
        return value

    relay_port = relay_map.get(str(source_port))
    if relay_port is None:
        return value

    userinfo = ""
    if parsed.username is not None:
        userinfo = parsed.username
        if parsed.password is not None:
            userinfo = f"{userinfo}:{parsed.password}"
        userinfo = f"{userinfo}@"

    netloc = f"{userinfo}{host_gateway}:{relay_port}"
    return urlunsplit((parsed.scheme, netloc, parsed.path, parsed.query, parsed.fragment))


def rewrite_value(value: object, relay_map: dict[str, str], host_gateway: str) -> object:
    if isinstance(value, dict):
        return {key: rewrite_value(item, relay_map, host_gateway) for key, item in value.items()}
    if isinstance(value, list):
        return [rewrite_value(item, relay_map, host_gateway) for item in value]
    if isinstance(value, str):
        return rewrite_url(value, relay_map, host_gateway)
    return value


def main() -> None:
    map_path, env_path, config_path, host_gateway = sys.argv[1:5]
    relay_map = parse_relay_map(read_text(map_path, "relay map"))
    relay_env = parse_relay_env(read_text(env_path, "relay env"), relay_map)
    config = parse_config(config_path)

    for source_port in sorted(relay_map):
        variable_name = f"PHLEX_HOST_RELAY_{source_port}"
        print(f"export\t{variable_name}\t{relay_env[variable_name]}")

    if config is not None:
        rewritten = rewrite_value(config, relay_map, host_gateway)
        config_content = json.dumps(rewritten, separators=(",", ":"), ensure_ascii=False)
        print(f"config\tKILO_CONFIG_CONTENT\t{config_content}")


if __name__ == "__main__":
    try:
        main()
    except ValueError as exc:
        print(f"kilo-env: {exc}", file=sys.stderr)
        raise SystemExit(1)
PY
}

kilo_env_apply_paths() {
  local map_path
  local env_path
  local config_path
  local output
  local entry_type
  local entry_name
  local entry_value

  map_path="$1"
  env_path="$2"
  config_path="$3"
  output=""

  kilo_env_clear_dynamic_exports

  if ! command -v python3 >/dev/null 2>&1; then
    kilo_env_print_error "python3 is required to validate relay data"
    kilo_env_clear_failed_exports
    return 1
  fi

  if ! output=$(kilo_env_collect_exports "$map_path" "$env_path" "$config_path"); then
    kilo_env_clear_failed_exports
    return 1
  fi

  while IFS=$'\t' read -r entry_type entry_name entry_value; do
    if [ -z "$entry_type" ]; then
      continue
    fi

    case "$entry_type" in
      export)
        if [[ ! "$entry_name" =~ ^PHLEX_HOST_RELAY_[0-9]+$ ]]; then
          kilo_env_print_error "internal export validation failed for ${entry_name}"
          kilo_env_clear_failed_exports
          return 1
        fi
        if ! export "${entry_name}=${entry_value}"; then
          kilo_env_print_error "failed to export ${entry_name}"
          kilo_env_clear_failed_exports
          return 1
        fi
        ;;
      config)
        if [ "$entry_name" != "KILO_CONFIG_CONTENT" ]; then
          kilo_env_print_error "internal config validation failed for ${entry_name}"
          kilo_env_clear_failed_exports
          return 1
        fi
        if ! export "KILO_CONFIG_CONTENT=${entry_value}"; then
          kilo_env_print_error "failed to export KILO_CONFIG_CONTENT"
          kilo_env_clear_failed_exports
          return 1
        fi
        ;;
      *)
        kilo_env_print_error "internal helper emitted unsupported record type ${entry_type}"
        kilo_env_clear_failed_exports
        return 1
        ;;
    esac
  done <<EOF
$output
EOF

  return 0
}

kilo_env_apply_root() {
  local relay_root
  local config_path

  relay_root="$1"
  config_path=""

  kilo_env_export_path_state "$relay_root"
  config_path="${KILO_CONFIG_PATH-}"

  if [ ! -f "$RELAY_MAP_JSON" ] && [ ! -f "$RELAY_MAP_ENV" ]; then
    kilo_env_clear_failed_exports
    return 0
  fi

  if [ ! -f "$RELAY_MAP_JSON" ] || [ ! -f "$RELAY_MAP_ENV" ]; then
    kilo_env_clear_failed_exports
    kilo_env_print_error "relay map and relay env must both exist under ${relay_root}"
    return 1
  fi

  if ! kilo_env_apply_paths "$RELAY_MAP_JSON" "$RELAY_MAP_ENV" "$config_path"; then
    kilo_env_clear_failed_exports
    return 1
  fi

  return 0
}

kilo_env_sha256() {
  local path
  local sum_line

  path="$1"
  sum_line=""

  if ! sum_line=$(shasum -a 256 "$path" 2>/dev/null); then
    return 1
  fi

  printf '%s\n' "${sum_line%% *}"
}

kilo_env_assert_equal() {
  local actual
  local expected
  local label

  actual="$1"
  expected="$2"
  label="$3"

  if [ "$actual" = "$expected" ]; then
    printf 'PASS: %s\n' "$label"
    return 0
  fi

  printf 'FAIL: %s\n' "$label" >&2
  printf '  expected: %s\n' "$expected" >&2
  printf '  actual:   %s\n' "$actual" >&2
  return 1
}

kilo_env_assert_file_contains() {
  local path
  local needle
  local label

  path="$1"
  needle="$2"
  label="$3"

  if grep -F "$needle" "$path" >/dev/null 2>&1; then
    printf 'PASS: %s\n' "$label"
    return 0
  fi

  printf 'FAIL: %s\n' "$label" >&2
  if [ -f "$path" ]; then
    printf '  file: %s\n' "$path" >&2
  fi
  return 1
}

kilo_env_assert_file_empty() {
  local path
  local label

  path="$1"
  label="$2"

  if [ ! -s "$path" ]; then
    printf 'PASS: %s\n' "$label"
    return 0
  fi

  printf 'FAIL: %s\n' "$label" >&2
  return 1
}

kilo_env_assert_export_value() {
  local variable_name
  local expected
  local label
  local actual

  variable_name="$1"
  expected="$2"
  label="$3"
  actual=$(printenv "$variable_name" 2>/dev/null || true)

  kilo_env_assert_equal "$actual" "$expected" "$label"
}

kilo_env_assert_export_absent() {
  local variable_name
  local label

  variable_name="$1"
  label="$2"

  if printenv "$variable_name" >/dev/null 2>&1; then
    printf 'FAIL: %s\n' "$label" >&2
    return 1
  fi

  printf 'PASS: %s\n' "$label"
  return 0
}

kilo_env_assert_hash_unchanged() {
  local path
  local before_hash
  local label
  local after_hash

  path="$1"
  before_hash="$2"
  label="$3"
  after_hash=""

  if ! after_hash=$(kilo_env_sha256 "$path"); then
    printf 'FAIL: %s\n' "$label" >&2
    return 1
  fi

  kilo_env_assert_equal "$after_hash" "$before_hash" "$label"
}

kilo_env_extract_output_value() {
  local content
  local key

  content="$1"
  key="$2"

  printf '%s\n' "$content" | awk -F= -v key="$key" '$1 == key {print substr($0, index($0, "=") + 1); exit}'
}

kilo_env_create_self_test_root() {
  local nonce
  local scratch_root

  nonce=""
  scratch_root=""

  if ! nonce=$(python3 - <<'PY'
import os
import time

print(f"{os.getpid()}-{time.time_ns()}")
PY
  ); then
    return 1
  fi

  if ! scratch_root=$(mktemp -d "${TMPDIR:-/tmp}/phlex-kilo-self-test.${nonce}.XXXXXX"); then
    return 1
  fi

  printf '%s\n' "$scratch_root"
}

kilo_env_cleanup_self_test_root() {
  local scratch_root

  scratch_root="$1"
  case "$scratch_root" in
    "${TMPDIR:-/tmp}"/phlex-kilo-self-test.*)
      rm -rf "$scratch_root"
      ;;
  esac
}

kilo_env_seed_stale_exports() {
  export PHLEX_HOST_RELAY_25114="stale"
  export PHLEX_HOST_RELAY_25300="stale"
  export KILO_CONFIG_CONTENT="stale"
}

kilo_env_run_self_tests_impl() {
  local fixture_root
  local scratch_root
  local fixture_map
  local fixture_env
  local fixture_artifact
  local fixture_map_before
  local fixture_env_before
  local fixture_artifact_before
  local recorded_map_hash
  local recorded_env_hash
  local expected_json_round_trip
  local expected_jsonc_round_trip
  local exact_config
  local exact_config_hash
  local json_root
  local json_config
  local json_config_hash
  local json_decoy_hash
  local jsonc_root
  local jsonc_config
  local jsonc_config_hash
  local malformed_json_root
  local malformed_json_hash
  local malformed_json_stderr
  local malformed_jsonc_root
  local malformed_jsonc_hash
  local malformed_jsonc_stderr
  local mismatched_root
  local mismatched_stderr
  local invalid_allowlist_root
  local invalid_allowlist_stderr
  local duplicate_env_root
  local duplicate_env_stderr
  local missing_root
  local missing_root_stderr
  local script_path
  local child_output
  local tests_failed

  fixture_root="$1"
  scratch_root="$2"
  fixture_map="${fixture_root}/relay-map.json"
  fixture_env="${fixture_root}/relay-map.env"
  fixture_artifact="${fixture_root}/artifact-sha256.txt"
  tests_failed=0

  printf '=== Kilo config rewrite self-test ===\n'
  printf 'INFO: native mount/runtime integration unavailable; fixture-only consumer logic validated.\n'

  if [ ! -f "$fixture_map" ] || [ ! -f "$fixture_env" ] || [ ! -f "$fixture_artifact" ]; then
    kilo_env_print_error "PHLEX_RELAY_FIXTURE_DIR must contain relay-map.json, relay-map.env, and artifact-sha256.txt"
    return 1
  fi

  if ! fixture_map_before=$(kilo_env_sha256 "$fixture_map"); then
    kilo_env_print_error "unable to hash relay fixture map"
    return 1
  fi
  if ! fixture_env_before=$(kilo_env_sha256 "$fixture_env"); then
    kilo_env_print_error "unable to hash relay fixture env"
    return 1
  fi
  if ! fixture_artifact_before=$(kilo_env_sha256 "$fixture_artifact"); then
    kilo_env_print_error "unable to hash relay fixture artifact manifest"
    return 1
  fi

  recorded_map_hash=$(awk '/relay-map\.json$/ {print $1; exit}' "$fixture_artifact")
  recorded_env_hash=$(awk '/relay-map\.env$/ {print $1; exit}' "$fixture_artifact")

  if ! kilo_env_assert_equal "$fixture_map_before" "$recorded_map_hash" \
    "carrier relay-map.json SHA-256 matches artifact receipt"; then
    tests_failed=1
  fi
  if ! kilo_env_assert_equal "$fixture_env_before" "$recorded_env_hash" \
    "carrier relay-map.env SHA-256 matches artifact receipt"; then
    tests_failed=1
  fi

  printf 'INFO: carrier relay-map.json sha256=%s\n' "$fixture_map_before"
  printf 'INFO: carrier relay-map.env sha256=%s\n' "$fixture_env_before"

  expected_json_round_trip=$(cat <<'EOF'
{"provider":{"anthropic":{"options":{"baseURL":"https://host.docker.internal:25115/v1?source=a#frag"}},"openai":{"options":{"baseURL":"http://host.docker.internal:25301/api/v1"}},"ipv6":{"options":{"baseURL":"https://host.docker.internal:25115/chat?q=1#anchor"}},"nonloopback":{"options":{"baseURL":"https://api.example.com:25114/v1"}},"unmapped":{"options":{"baseURL":"http://127.0.0.1:29999/v1"}},"stdio":{"transport":{"type":"stdio","command":"kilo"}},"unix":{"options":{"baseURL":"unix:///var/run/kilo.sock"}},"nested":{"array":["https://host.docker.internal:25115/inside","http://localhost:9999/stay"]}},"other":"keep"}
EOF
)

  expected_jsonc_round_trip=$(cat <<'EOF'
{"provider":{"commented":{"options":{"baseURL":"https://host.docker.internal:25115/v2"}},"literal":{"url":"http://host.docker.internal:25301/path//segment?x=1#frag","text":"/* not a comment */ // still text"}},"array":["https://host.docker.internal:25115/array"]}
EOF
)

  kilo_env_seed_stale_exports
  if ! kilo_env_apply_root "$fixture_root"; then
    printf 'FAIL: exact carrier root validation\n' >&2
    tests_failed=1
  else
    printf 'PASS: exact carrier root validation\n'
  fi
  if ! kilo_env_assert_export_value "PHLEX_HOST_RELAY_25114" "25115" \
    "exact carrier exports PHLEX_HOST_RELAY_25114"; then
    tests_failed=1
  fi
  if ! kilo_env_assert_export_value "PHLEX_HOST_RELAY_25300" "25301" \
    "exact carrier exports PHLEX_HOST_RELAY_25300"; then
    tests_failed=1
  fi
  if ! kilo_env_assert_export_absent "KILO_CONFIG_CONTENT" \
    "exact carrier root without config leaves KILO_CONFIG_CONTENT absent"; then
    tests_failed=1
  fi
  if ! kilo_env_assert_export_absent "KILO_CONFIG_PATH" \
    "exact carrier root without config leaves KILO_CONFIG_PATH absent"; then
    tests_failed=1
  fi
  if ! kilo_env_assert_export_value "PHLEX_HOST_RELAY_FILE" "$fixture_map" \
    "exact carrier root updates PHLEX_HOST_RELAY_FILE"; then
    tests_failed=1
  fi
  if ! kilo_env_assert_export_value "PHLEX_HOST_RELAYS_ENV" "$fixture_env" \
    "exact carrier root updates PHLEX_HOST_RELAYS_ENV"; then
    tests_failed=1
  fi

  exact_config="${scratch_root}/exact-carrier-config.json"
  cat > "$exact_config" <<'EOF'
{
  "provider": {
    "anthropic": {
      "options": {
        "baseURL": "https://127.0.0.1:25114/v1?source=a#frag"
      }
    },
    "openai": {
      "options": {
        "baseURL": "http://localhost:25300/api/v1"
      }
    },
    "ipv6": {
      "options": {
        "baseURL": "https://[::1]:25114/chat?q=1#anchor"
      }
    },
    "nonloopback": {
      "options": {
        "baseURL": "https://api.example.com:25114/v1"
      }
    },
    "unmapped": {
      "options": {
        "baseURL": "http://127.0.0.1:29999/v1"
      }
    },
    "stdio": {
      "transport": {
        "type": "stdio",
        "command": "kilo"
      }
    },
    "unix": {
      "options": {
        "baseURL": "unix:///var/run/kilo.sock"
      }
    },
    "nested": {
      "array": [
        "https://127.0.0.1:25114/inside",
        "http://localhost:9999/stay"
      ]
    }
  },
  "other": "keep"
}
EOF
  exact_config_hash=$(kilo_env_sha256 "$exact_config")

  kilo_env_seed_stale_exports
  kilo_env_unset_var_quietly "KILO_CONFIG_PATH"
  if ! kilo_env_apply_paths "$fixture_map" "$fixture_env" "$exact_config"; then
    printf 'FAIL: exact carrier round-trip rewrite\n' >&2
    tests_failed=1
  else
    printf 'PASS: exact carrier round-trip rewrite\n'
  fi
  if ! kilo_env_assert_export_value "PHLEX_HOST_RELAY_25114" "25115" \
    "exact carrier round-trip keeps PHLEX_HOST_RELAY_25114"; then
    tests_failed=1
  fi
  if ! kilo_env_assert_export_value "PHLEX_HOST_RELAY_25300" "25301" \
    "exact carrier round-trip keeps PHLEX_HOST_RELAY_25300"; then
    tests_failed=1
  fi
  if ! kilo_env_assert_export_value "KILO_CONFIG_CONTENT" "$expected_json_round_trip" \
    "exact carrier round-trip rewrites 127.0.0.1 localhost and [::1] URLs"; then
    tests_failed=1
  fi
  if ! kilo_env_assert_hash_unchanged "$exact_config" "$exact_config_hash" \
    "exact carrier config hash unchanged"; then
    tests_failed=1
  fi

  json_root="${scratch_root}/json-root"
  mkdir -p "$json_root"
  cat > "${json_root}/relay-map.json" <<'EOF'
{"25114":"25115","25300":"25301"}
EOF
  cat > "${json_root}/relay-map.env" <<'EOF'
PHLEX_HOST_RELAY_25114=25115
PHLEX_HOST_RELAY_25300=25301
EOF
  json_config="${json_root}/kilo.json"
  cat > "$json_config" <<'EOF'
{
  "provider": {
    "anthropic": {
      "options": {
        "baseURL": "https://127.0.0.1:25114/v1?source=a#frag"
      }
    },
    "openai": {
      "options": {
        "baseURL": "http://localhost:25300/api/v1"
      }
    },
    "ipv6": {
      "options": {
        "baseURL": "https://[::1]:25114/chat?q=1#anchor"
      }
    },
    "nonloopback": {
      "options": {
        "baseURL": "https://api.example.com:25114/v1"
      }
    },
    "unmapped": {
      "options": {
        "baseURL": "http://127.0.0.1:29999/v1"
      }
    },
    "stdio": {
      "transport": {
        "type": "stdio",
        "command": "kilo"
      }
    },
    "unix": {
      "options": {
        "baseURL": "unix:///var/run/kilo.sock"
      }
    },
    "nested": {
      "array": [
        "https://127.0.0.1:25114/inside",
        "http://localhost:9999/stay"
      ]
    }
  },
  "other": "keep"
}
EOF
  cat > "${json_root}/kilo.jsonc" <<'EOF'
{
  "decoy": {
    "baseURL": "http://localhost:1/should-not-be-selected"
  }
}
EOF
  json_config_hash=$(kilo_env_sha256 "$json_config")
  json_decoy_hash=$(kilo_env_sha256 "${json_root}/kilo.jsonc")

  kilo_env_seed_stale_exports
  if ! kilo_env_apply_root "$json_root"; then
    printf 'FAIL: JSON root rewrite\n' >&2
    tests_failed=1
  else
    printf 'PASS: JSON root rewrite\n'
  fi
  if ! kilo_env_assert_export_value "KILO_CONFIG_PATH" "${json_root}/kilo.json" \
    "JSON root prefers kilo.json over kilo.jsonc"; then
    tests_failed=1
  fi
  if ! kilo_env_assert_export_value "KILO_CONFIG_CONTENT" "$expected_json_round_trip" \
    "JSON root exports rewritten KILO_CONFIG_CONTENT"; then
    tests_failed=1
  fi
  if ! kilo_env_assert_hash_unchanged "$json_config" "$json_config_hash" \
    "JSON root kilo.json hash unchanged"; then
    tests_failed=1
  fi
  if ! kilo_env_assert_hash_unchanged "${json_root}/kilo.jsonc" "$json_decoy_hash" \
    "JSON root decoy kilo.jsonc hash unchanged"; then
    tests_failed=1
  fi

  jsonc_root="${scratch_root}/jsonc-root"
  mkdir -p "$jsonc_root"
  cat > "${jsonc_root}/relay-map.json" <<'EOF'
{"25114":"25115","25300":"25301"}
EOF
  cat > "${jsonc_root}/relay-map.env" <<'EOF'
PHLEX_HOST_RELAY_25114=25115
PHLEX_HOST_RELAY_25300=25301
EOF
  jsonc_config="${jsonc_root}/kilo.jsonc"
  cat > "$jsonc_config" <<'EOF'
{
  // provider comment
  "provider": {
    /* block comment */
    "commented": {
      "options": {
        "baseURL": "https://localhost:25114/v2", // trailing comment
      },
    },
    "literal": {
      "url": "http://localhost:25300/path//segment?x=1#frag",
      "text": "/* not a comment */ // still text",
    },
  },
  "array": [
    "https://[::1]:25114/array",
  ],
}
EOF
  jsonc_config_hash=$(kilo_env_sha256 "$jsonc_config")

  kilo_env_seed_stale_exports
  if ! kilo_env_apply_root "$jsonc_root"; then
    printf 'FAIL: JSONC root rewrite\n' >&2
    tests_failed=1
  else
    printf 'PASS: JSONC root rewrite\n'
  fi
  if ! kilo_env_assert_export_value "KILO_CONFIG_PATH" "${jsonc_root}/kilo.jsonc" \
    "JSONC root falls back to kilo.jsonc"; then
    tests_failed=1
  fi
  if ! kilo_env_assert_export_value "KILO_CONFIG_CONTENT" "$expected_jsonc_round_trip" \
    "JSONC root strips comments/trailing commas and rewrites URLs"; then
    tests_failed=1
  fi
  if ! kilo_env_assert_hash_unchanged "$jsonc_config" "$jsonc_config_hash" \
    "JSONC root kilo.jsonc hash unchanged"; then
    tests_failed=1
  fi

  malformed_json_root="${scratch_root}/malformed-json-root"
  mkdir -p "$malformed_json_root"
  cat > "${malformed_json_root}/relay-map.json" <<'EOF'
{"25114":"25115","25300":"25301"}
EOF
  cat > "${malformed_json_root}/relay-map.env" <<'EOF'
PHLEX_HOST_RELAY_25114=25115
PHLEX_HOST_RELAY_25300=25301
EOF
  cat > "${malformed_json_root}/kilo.json" <<'EOF'
{
  "provider": {
    "broken": {
      "options": {
        "baseURL": "https://127.0.0.1:25114/v1"
      }
    }
EOF
  malformed_json_hash=$(kilo_env_sha256 "${malformed_json_root}/kilo.json")
  malformed_json_stderr="${malformed_json_root}/stderr.txt"

  kilo_env_seed_stale_exports
  if kilo_env_apply_root "$malformed_json_root" 2>"$malformed_json_stderr"; then
    printf 'FAIL: malformed JSON config must fail closed\n' >&2
    tests_failed=1
  else
    printf 'PASS: malformed JSON config fails closed\n'
  fi
  if ! kilo_env_assert_file_contains "$malformed_json_stderr" \
    "invalid Kilo config JSON/JSONC" "malformed JSON reports a clear stderr error"; then
    tests_failed=1
  fi
  if ! kilo_env_assert_export_absent "PHLEX_HOST_RELAY_25114" \
    "malformed JSON clears PHLEX_HOST_RELAY_25114"; then
    tests_failed=1
  fi
  if ! kilo_env_assert_export_absent "PHLEX_HOST_RELAY_25300" \
    "malformed JSON clears PHLEX_HOST_RELAY_25300"; then
    tests_failed=1
  fi
  if ! kilo_env_assert_export_absent "KILO_CONFIG_CONTENT" \
    "malformed JSON clears KILO_CONFIG_CONTENT"; then
    tests_failed=1
  fi
  if ! kilo_env_assert_export_absent "KILO_CONFIG_PATH" \
    "malformed JSON clears KILO_CONFIG_PATH"; then
    tests_failed=1
  fi
  if ! kilo_env_assert_hash_unchanged "${malformed_json_root}/kilo.json" "$malformed_json_hash" \
    "malformed JSON input hash unchanged"; then
    tests_failed=1
  fi

  malformed_jsonc_root="${scratch_root}/malformed-jsonc-root"
  mkdir -p "$malformed_jsonc_root"
  cat > "${malformed_jsonc_root}/relay-map.json" <<'EOF'
{"25114":"25115","25300":"25301"}
EOF
  cat > "${malformed_jsonc_root}/relay-map.env" <<'EOF'
PHLEX_HOST_RELAY_25114=25115
PHLEX_HOST_RELAY_25300=25301
EOF
  cat > "${malformed_jsonc_root}/kilo.jsonc" <<'EOF'
{
  "provider": {
    "broken": {
      "options": {
        "baseURL": "https://localhost:25114/v1"
      }
    } /* unterminated
EOF
  malformed_jsonc_hash=$(kilo_env_sha256 "${malformed_jsonc_root}/kilo.jsonc")
  malformed_jsonc_stderr="${malformed_jsonc_root}/stderr.txt"

  kilo_env_seed_stale_exports
  if kilo_env_apply_root "$malformed_jsonc_root" 2>"$malformed_jsonc_stderr"; then
    printf 'FAIL: malformed JSONC config must fail closed\n' >&2
    tests_failed=1
  else
    printf 'PASS: malformed JSONC config fails closed\n'
  fi
  if ! kilo_env_assert_file_contains "$malformed_jsonc_stderr" \
    "unterminated block comment in Kilo config" \
    "malformed JSONC reports unterminated comment"; then
    tests_failed=1
  fi
  if ! kilo_env_assert_export_absent "PHLEX_HOST_RELAY_25114" \
    "malformed JSONC clears PHLEX_HOST_RELAY_25114"; then
    tests_failed=1
  fi
  if ! kilo_env_assert_export_absent "PHLEX_HOST_RELAY_25300" \
    "malformed JSONC clears PHLEX_HOST_RELAY_25300"; then
    tests_failed=1
  fi
  if ! kilo_env_assert_export_absent "KILO_CONFIG_CONTENT" \
    "malformed JSONC clears KILO_CONFIG_CONTENT"; then
    tests_failed=1
  fi
  if ! kilo_env_assert_export_absent "KILO_CONFIG_PATH" \
    "malformed JSONC clears KILO_CONFIG_PATH"; then
    tests_failed=1
  fi
  if ! kilo_env_assert_hash_unchanged "${malformed_jsonc_root}/kilo.jsonc" "$malformed_jsonc_hash" \
    "malformed JSONC input hash unchanged"; then
    tests_failed=1
  fi

  mismatched_root="${scratch_root}/mismatched-root"
  mkdir -p "$mismatched_root"
  cat > "${mismatched_root}/relay-map.json" <<'EOF'
{"25114":"25115","25300":"25301"}
EOF
  cat > "${mismatched_root}/relay-map.env" <<'EOF'
PHLEX_HOST_RELAY_25114=25115
EOF
  mismatched_stderr="${mismatched_root}/stderr.txt"

  kilo_env_seed_stale_exports
  if kilo_env_apply_root "$mismatched_root" 2>"$mismatched_stderr"; then
    printf 'FAIL: mismatched relay map/env must fail closed\n' >&2
    tests_failed=1
  else
    printf 'PASS: mismatched relay map/env fails closed\n'
  fi
  if ! kilo_env_assert_file_contains "$mismatched_stderr" \
    "relay env must be sorted canonical" "mismatched relay map/env reports stderr error"; then
    tests_failed=1
  fi
  if ! kilo_env_assert_export_absent "PHLEX_HOST_RELAY_25114" \
    "mismatched relay map/env clears PHLEX_HOST_RELAY_25114"; then
    tests_failed=1
  fi
  if ! kilo_env_assert_export_absent "PHLEX_HOST_RELAY_25300" \
    "mismatched relay map/env clears PHLEX_HOST_RELAY_25300"; then
    tests_failed=1
  fi
  if ! kilo_env_assert_export_absent "KILO_CONFIG_CONTENT" \
    "mismatched relay map/env clears KILO_CONFIG_CONTENT"; then
    tests_failed=1
  fi
  if ! kilo_env_assert_export_absent "KILO_CONFIG_PATH" \
    "mismatched relay map/env clears KILO_CONFIG_PATH"; then
    tests_failed=1
  fi

  invalid_allowlist_root="${scratch_root}/invalid-allowlist-root"
  mkdir -p "$invalid_allowlist_root"
  cat > "${invalid_allowlist_root}/relay-map.json" <<'EOF'
{"9999":"10000"}
EOF
  cat > "${invalid_allowlist_root}/relay-map.env" <<'EOF'
PHLEX_HOST_RELAY_9999=10000
EOF
  invalid_allowlist_stderr="${invalid_allowlist_root}/stderr.txt"

  kilo_env_seed_stale_exports
  if kilo_env_apply_root "$invalid_allowlist_root" 2>"$invalid_allowlist_stderr"; then
    printf 'FAIL: invalid allowlist relay map must fail closed\n' >&2
    tests_failed=1
  else
    printf 'PASS: invalid allowlist relay map fails closed\n'
  fi
  if ! kilo_env_assert_file_contains "$invalid_allowlist_stderr" \
    "is not allowlisted" "invalid allowlist relay map reports stderr error"; then
    tests_failed=1
  fi
  if ! kilo_env_assert_export_absent "PHLEX_HOST_RELAY_25114" \
    "invalid allowlist relay map leaves PHLEX_HOST_RELAY_25114 absent"; then
    tests_failed=1
  fi
  if ! kilo_env_assert_export_absent "KILO_CONFIG_CONTENT" \
    "invalid allowlist relay map leaves KILO_CONFIG_CONTENT absent"; then
    tests_failed=1
  fi

  duplicate_env_root="${scratch_root}/duplicate-env-root"
  mkdir -p "$duplicate_env_root"
  cat > "${duplicate_env_root}/relay-map.json" <<'EOF'
{"25114":"25115"}
EOF
  cat > "${duplicate_env_root}/relay-map.env" <<'EOF'
PHLEX_HOST_RELAY_25114=25115
PHLEX_HOST_RELAY_25114=25115
EOF
  duplicate_env_stderr="${duplicate_env_root}/stderr.txt"

  kilo_env_seed_stale_exports
  if kilo_env_apply_root "$duplicate_env_root" 2>"$duplicate_env_stderr"; then
    printf 'FAIL: duplicate relay env must fail closed\n' >&2
    tests_failed=1
  else
    printf 'PASS: duplicate relay env fails closed\n'
  fi
  if ! kilo_env_assert_file_contains "$duplicate_env_stderr" \
    "duplicate relay env entry" "duplicate relay env reports stderr error"; then
    tests_failed=1
  fi
  if ! kilo_env_assert_export_absent "PHLEX_HOST_RELAY_25114" \
    "duplicate relay env clears PHLEX_HOST_RELAY_25114"; then
    tests_failed=1
  fi
  if ! kilo_env_assert_export_absent "KILO_CONFIG_CONTENT" \
    "duplicate relay env clears KILO_CONFIG_CONTENT"; then
    tests_failed=1
  fi

  missing_root="${scratch_root}/missing-root"
  missing_root_stderr="${scratch_root}/missing-root.stderr"

  kilo_env_seed_stale_exports
  if ! kilo_env_apply_root "$missing_root" 2>"$missing_root_stderr"; then
    printf 'FAIL: absent relay root must fail closed without returning nonzero\n' >&2
    tests_failed=1
  else
    printf 'PASS: absent relay root fails closed without returning nonzero\n'
  fi
  if ! kilo_env_assert_file_empty "$missing_root_stderr" \
    "absent relay root is quiet"; then
    tests_failed=1
  fi
  if ! kilo_env_assert_export_absent "PHLEX_HOST_RELAY_25114" \
    "absent relay root clears PHLEX_HOST_RELAY_25114"; then
    tests_failed=1
  fi
  if ! kilo_env_assert_export_absent "PHLEX_HOST_RELAY_25300" \
    "absent relay root clears PHLEX_HOST_RELAY_25300"; then
    tests_failed=1
  fi
  if ! kilo_env_assert_export_absent "KILO_CONFIG_CONTENT" \
    "absent relay root clears KILO_CONFIG_CONTENT"; then
    tests_failed=1
  fi

  if ! script_path=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd); then
    kilo_env_print_error "unable to resolve script directory for source-safety test"
    return 1
  fi
  script_path="${script_path}/$(basename "${BASH_SOURCE[0]}")"

  if ! child_output=$(
    PHLEX_RELAY_FIXTURE_DIR="$fixture_root" \
      bash --noprofile --norc -c '
script_path=$1
set -euf -o pipefail
before_flags=$-
before_opts=$(set +o)
export PHLEX_HOST_RELAY_25114=stale
export PHLEX_HOST_RELAY_25300=stale
export KILO_CONFIG_CONTENT=stale
export KILO_CONFIG_PATH=stale
. "$script_path"
status=$?
after_flags=$-
after_opts=$(set +o)
before_opts_sha=$(printf %s "$before_opts" | shasum -a 256)
before_opts_sha=${before_opts_sha%% *}
after_opts_sha=$(printf %s "$after_opts" | shasum -a 256)
after_opts_sha=${after_opts_sha%% *}
printf "%s\n" \
  "status=$status" \
  "before_flags=$before_flags" \
  "after_flags=$after_flags" \
  "before_opts_sha=$before_opts_sha" \
  "after_opts_sha=$after_opts_sha" \
  "relay25114=${PHLEX_HOST_RELAY_25114-unset}" \
  "relay25300=${PHLEX_HOST_RELAY_25300-unset}" \
  "config=${KILO_CONFIG_CONTENT-unset}" \
  "config_path=${KILO_CONFIG_PATH-unset}" \
  "relay_file=${PHLEX_HOST_RELAY_FILE-unset}" \
  "relays_env=${PHLEX_HOST_RELAYS_ENV-unset}" \
  "gateway=${PHLEX_HOST_GATEWAY-unset}"
' bash "$script_path"
  ); then
    printf 'FAIL: production source-safety child shell exited unexpectedly\n' >&2
    tests_failed=1
  else
    printf 'PASS: production source-safety child shell stayed alive\n'
  fi

  if ! kilo_env_assert_equal "$(kilo_env_extract_output_value "$child_output" "status")" "0" \
    "production sourcing returns success"; then
    tests_failed=1
  fi
  if ! kilo_env_assert_equal "$(kilo_env_extract_output_value "$child_output" "before_flags")" \
    "$(kilo_env_extract_output_value "$child_output" "after_flags")" \
    "production sourcing preserves shell flags"; then
    tests_failed=1
  fi
  if ! kilo_env_assert_equal "$(kilo_env_extract_output_value "$child_output" "before_opts_sha")" \
    "$(kilo_env_extract_output_value "$child_output" "after_opts_sha")" \
    "production sourcing preserves shell options"; then
    tests_failed=1
  fi
  if ! kilo_env_assert_equal "$(kilo_env_extract_output_value "$child_output" "relay25114")" "unset" \
    "production sourcing clears stale PHLEX_HOST_RELAY_25114"; then
    tests_failed=1
  fi
  if ! kilo_env_assert_equal "$(kilo_env_extract_output_value "$child_output" "relay25300")" "unset" \
    "production sourcing clears stale PHLEX_HOST_RELAY_25300"; then
    tests_failed=1
  fi
  if ! kilo_env_assert_equal "$(kilo_env_extract_output_value "$child_output" "config")" "unset" \
    "production sourcing clears stale KILO_CONFIG_CONTENT"; then
    tests_failed=1
  fi
  if ! kilo_env_assert_equal "$(kilo_env_extract_output_value "$child_output" "config_path")" "unset" \
    "production sourcing clears stale KILO_CONFIG_PATH"; then
    tests_failed=1
  fi
  if ! kilo_env_assert_equal "$(kilo_env_extract_output_value "$child_output" "relay_file")" \
    "${KILO_ENV_FIXED_ROOT}/relay-map.json" \
    "production sourcing keeps the fixed relay-map.json path"; then
    tests_failed=1
  fi
  if ! kilo_env_assert_equal "$(kilo_env_extract_output_value "$child_output" "relays_env")" \
    "${KILO_ENV_FIXED_ROOT}/relay-map.env" \
    "production sourcing keeps the fixed relay-map.env path"; then
    tests_failed=1
  fi
  if ! kilo_env_assert_equal "$(kilo_env_extract_output_value "$child_output" "gateway")" \
    "$KILO_ENV_HOST_GATEWAY" \
    "production sourcing keeps PHLEX_HOST_GATEWAY"; then
    tests_failed=1
  fi

  if ! kilo_env_assert_hash_unchanged "$fixture_map" "$fixture_map_before" \
    "carrier relay-map.json hash unchanged after self-test"; then
    tests_failed=1
  fi
  if ! kilo_env_assert_hash_unchanged "$fixture_env" "$fixture_env_before" \
    "carrier relay-map.env hash unchanged after self-test"; then
    tests_failed=1
  fi
  if ! kilo_env_assert_hash_unchanged "$fixture_artifact" "$fixture_artifact_before" \
    "carrier artifact-sha256.txt hash unchanged after self-test"; then
    tests_failed=1
  fi

  if [ "$tests_failed" -ne 0 ]; then
    printf '=== Self-test failed ===\n' >&2
    return 1
  fi

  printf '=== Self-test passed ===\n'
  return 0
}

kilo_env_run_self_tests() {
  local fixture_root
  local scratch_root
  local status

  fixture_root="${PHLEX_RELAY_FIXTURE_DIR-}"
  scratch_root=""
  status=0

  if [ -z "$fixture_root" ]; then
    kilo_env_print_error "PHLEX_RELAY_FIXTURE_DIR must be set when --self-test is used"
    return 1
  fi

  if ! scratch_root=$(kilo_env_create_self_test_root); then
    kilo_env_print_error "unable to create self-test scratch root"
    return 1
  fi

  if kilo_env_run_self_tests_impl "$fixture_root" "$scratch_root"; then
    status=0
  else
    status=$?
  fi

  kilo_env_cleanup_self_test_root "$scratch_root"
  return "$status"
}

kilo_env_parse_args() {
  while [ "$#" -gt 0 ]; do
    case "$1" in
      --self-test)
        KILO_ENV_SELF_TEST_MODE=1
        ;;
      *)
        kilo_env_print_error "unknown option: $1"
        return 1
        ;;
    esac
    shift
  done

  return 0
}

kilo_env_main() {
  if kilo_env_is_sourced; then
    if [ "${1-}" = "--self-test" ]; then
      if ! kilo_env_parse_args "$@"; then
        return 1
      fi
    else
      KILO_ENV_SELF_TEST_MODE=0
    fi
  else
    if ! kilo_env_parse_args "$@"; then
      return 1
    fi
  fi

  if [ "$KILO_ENV_SELF_TEST_MODE" -eq 1 ]; then
    kilo_env_run_self_tests
    return $?
  fi

  if ! kilo_env_apply_root "$KILO_ENV_FIXED_ROOT"; then
    :
  fi

  return 0
}

if kilo_env_main "$@"; then
  kilo_env_status=0
else
  kilo_env_status=$?
fi

if kilo_env_is_sourced; then
  if [ "$KILO_ENV_SELF_TEST_MODE" -eq 1 ]; then
    return "$kilo_env_status"
  fi
  return 0
fi

exit "$kilo_env_status"
