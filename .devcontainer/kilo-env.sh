#!/usr/bin/env bash
# Prepare KILO_CONFIG_CONTENT with rewritten loopback URLs based on relay map.
# This script is sourced by /etc/profile.d/ so the variable is available to all processes.
# Conforms to kilo-plan/v5: write-free Kilo configuration rewriting.

KILO_ENV_PRODUCTION_ROOT="/run/phlex-host-relays"
KILO_ENV_FIXED_ROOT="$KILO_ENV_PRODUCTION_ROOT"
KILO_ENV_HOST_GATEWAY="host.docker.internal"
KILO_ENV_SELF_TEST_MODE=0

# Keep helper lookup independent of later changes to the caller's working directory.
KILO_ENV_SCRIPT_PATH="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)" || {
  printf 'kilo-env: unable to resolve production script path\n' >&2
  [ "${BASH_SOURCE[0]-}" = "$0" ] && exit 1 || return 1
}
KILO_ENV_SCRIPT_PATH="${KILO_ENV_SCRIPT_PATH}/${BASH_SOURCE[0]##*/}"

kilo_env_is_sourced() {
  [ "${BASH_SOURCE[0]-}" != "$0" ]
}

kilo_env_print_error() {
  printf 'kilo-env: %s\n' "$*" >&2
}

kilo_env_require_args() {
  local expected="${1-}"
  local function_name="${2-}"
  [ "$#" -ge 2 ] || return 2
  shift 2
  if [ "$#" -ne "$expected" ]; then
    kilo_env_print_error "${function_name} requires ${expected} arguments; got $#"
    return 2
  fi
}

kilo_env_unset_var_quietly() {
  kilo_env_require_args 1 "${FUNCNAME[0]}" "$@" || return 2
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
  kilo_env_require_args 1 "${FUNCNAME[0]}" "$@" || return 2
  local relay_root
  local candidate

  relay_root="$1"

  # Fixture roots contain their own config; production uses the read-only host mount.
  if [ "$relay_root" = "$KILO_ENV_PRODUCTION_ROOT" ]; then
    relay_root="/root/.config/kilo"
  fi

  for candidate in "${relay_root}/kilo.json" "${relay_root}/kilo.jsonc"; do
    if [ -f "$candidate" ]; then
      printf '%s\n' "$candidate"
      return 0
    fi
  done

  return 1
}

kilo_env_export_path_state() {
  kilo_env_require_args 1 "${FUNCNAME[0]}" "$@" || return 2
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
  kilo_env_require_args 3 "${FUNCNAME[0]}" "$@" || return 2
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
  kilo_env_require_args 3 "${FUNCNAME[0]}" "$@" || return 2
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
  kilo_env_require_args 1 "${FUNCNAME[0]}" "$@" || return 2
  local relay_root
  local config_path

  relay_root="$1"
  config_path=""

  kilo_env_export_path_state "$relay_root" || return 1
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

kilo_env_run_self_tests() {
  local helper_path

  case "$KILO_ENV_SCRIPT_PATH" in
    /etc/profile.d/kilo-env.sh)
      helper_path="/usr/local/lib/phlex/kilo-env-self-test.sh"
      ;;
    *)
      helper_path="${KILO_ENV_SCRIPT_PATH%/*}/kilo-env-self-test.sh"
      ;;
  esac

  # Keep test code outside login-shell startup and preserve the caller's options.
  if ! . "$helper_path"; then
    kilo_env_print_error "failed to load self-test helper: $helper_path"
    return 1
  fi

  kilo_env_run_self_tests_loaded
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
    kilo_env_run_self_tests || return $?
    return 0
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
