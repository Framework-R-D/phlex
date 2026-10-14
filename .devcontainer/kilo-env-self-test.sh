#!/usr/bin/env bash
# Test-only helper for kilo-env.sh - contains assertion and test functions
# This file is sourced only when kilo_env_run_self_tests is invoked
# Do not execute directly; returns error if executed directly (not sourced)

# If executed directly (BASH_SOURCE[0] == $0), return error
if [ "${BASH_SOURCE[0]}" = "$0" ]; then
  printf 'kilo-env-self-test: this file is a test helper and must not be executed directly (only sourced)\n' >&2
  exit 1
fi

# === Test-only assertion functions ===

kilo_env_sha256() {
  kilo_env_require_args 1 "${FUNCNAME[0]}" "$@" || return 2
  local path
  local sum_line

  path="$1"
  sum_line=""

  if command -v shasum >/dev/null 2>&1 && \
    sum_line=$(shasum -a 256 "$path" 2>/dev/null); then
    printf '%s\n' "${sum_line%% *}"
    return 0
  fi

  if command -v sha256sum >/dev/null 2>&1 && \
    sum_line=$(sha256sum "$path" 2>/dev/null); then
    printf '%s\n' "${sum_line%% *}"
    return 0
  fi

  return 1
}

kilo_env_assert_equal() {
  kilo_env_require_args 3 "${FUNCNAME[0]}" "$@" || return 2
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
  kilo_env_require_args 3 "${FUNCNAME[0]}" "$@" || return 2
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
  kilo_env_require_args 2 "${FUNCNAME[0]}" "$@" || return 2
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
  kilo_env_require_args 3 "${FUNCNAME[0]}" "$@" || return 2
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
  kilo_env_require_args 2 "${FUNCNAME[0]}" "$@" || return 2
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
  kilo_env_require_args 3 "${FUNCNAME[0]}" "$@" || return 2
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
  kilo_env_require_args 2 "${FUNCNAME[0]}" "$@" || return 2
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
  kilo_env_require_args 1 "${FUNCNAME[0]}" "$@" || return 2
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
  kilo_env_require_args 2 "${FUNCNAME[0]}" "$@" || return 2
  local fixture_root="$1"
  local scratch_root="$2"
  local fixture_map="${fixture_root}/relay-map.json"
  local fixture_env="${fixture_root}/relay-map.env"
  local fixture_artifact="${fixture_root}/artifact-sha256.txt"
  local tests_failed=0

  printf '=== Kilo config rewrite self-test ===\n'
  printf 'INFO: native mount/runtime integration unavailable; fixture-only consumer logic validated.\n'

  if [ ! -f "$fixture_map" ] || [ ! -f "$fixture_env" ] || [ ! -f "$fixture_artifact" ]; then
    kilo_env_print_error "PHLEX_RELAY_FIXTURE_DIR must contain relay-map.json, relay-map.env, and artifact-sha256.txt"
    return 1
  fi

  local fixture_map_before fixture_env_before fixture_artifact_before
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

  local recorded_map_hash recorded_env_hash
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

  local expected_json_round_trip expected_jsonc_round_trip
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

  local exact_config="${scratch_root}/exact-carrier-config.json"
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
  local exact_config_hash
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

  local json_root="${scratch_root}/json-root"
  mkdir -p "$json_root"
  cat > "${json_root}/relay-map.json" <<'EOF'
{"25114":"25115","25300":"25301"}
EOF
  cat > "${json_root}/relay-map.env" <<'EOF'
PHLEX_HOST_RELAY_25114=25115
PHLEX_HOST_RELAY_25300=25301
EOF
  local json_config="${json_root}/kilo.json"
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
  local json_config_hash
  json_config_hash=$(kilo_env_sha256 "$json_config")
  local json_decoy_hash
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

  local jsonc_root="${scratch_root}/jsonc-root"
  mkdir -p "$jsonc_root"
  cat > "${jsonc_root}/relay-map.json" <<'EOF'
{"25114":"25115","25300":"25301"}
EOF
  cat > "${jsonc_root}/relay-map.env" <<'EOF'
PHLEX_HOST_RELAY_25114=25115
PHLEX_HOST_RELAY_25300=25301
EOF
  local jsonc_config="${jsonc_root}/kilo.jsonc"
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
  local jsonc_config_hash
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

  local malformed_json_root="${scratch_root}/malformed-json-root"
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
  local malformed_json_hash
  malformed_json_hash=$(kilo_env_sha256 "${malformed_json_root}/kilo.json")
  local malformed_json_stderr="${malformed_json_root}/stderr.txt"

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

  local malformed_jsonc_root="${scratch_root}/malformed-jsonc-root"
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
  local malformed_jsonc_hash
  malformed_jsonc_hash=$(kilo_env_sha256 "${malformed_jsonc_root}/kilo.jsonc")
  local malformed_jsonc_stderr="${malformed_jsonc_root}/stderr.txt"

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

  local mismatched_root="${scratch_root}/mismatched-root"
  mkdir -p "$mismatched_root"
  cat > "${mismatched_root}/relay-map.json" <<'EOF'
{"25114":"25115","25300":"25301"}
EOF
  cat > "${mismatched_root}/relay-map.env" <<'EOF'
PHLEX_HOST_RELAY_25114=25115
EOF
  local mismatched_stderr="${mismatched_root}/stderr.txt"

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

  local invalid_allowlist_root="${scratch_root}/invalid-allowlist-root"
  mkdir -p "$invalid_allowlist_root"
  cat > "${invalid_allowlist_root}/relay-map.json" <<'EOF'
{"9999":"10000"}
EOF
  cat > "${invalid_allowlist_root}/relay-map.env" <<'EOF'
PHLEX_HOST_RELAY_9999=10000
EOF
  local invalid_allowlist_stderr="${invalid_allowlist_root}/stderr.txt"

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

  local duplicate_env_root="${scratch_root}/duplicate-env-root"
  mkdir -p "$duplicate_env_root"
  cat > "${duplicate_env_root}/relay-map.json" <<'EOF'
{"25114":"25115"}
EOF
  cat > "${duplicate_env_root}/relay-map.env" <<'EOF'
PHLEX_HOST_RELAY_25114=25115
PHLEX_HOST_RELAY_25114=25115
EOF
  local duplicate_env_stderr="${duplicate_env_root}/stderr.txt"

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

  local missing_root="${scratch_root}/missing-root"
  local missing_root_stderr="${scratch_root}/missing-root.stderr"

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

  local script_path child_output source_safety_output_ready=0
  local isolated_root="${scratch_root}/isolated-fixed-root"
  local fixed_root_replacements=0
  local line
  script_path="${scratch_root}/kilo-env.sh"

  if ! mkdir -p "$isolated_root"; then
    kilo_env_print_error "unable to create isolated source-safety relay root"
    tests_failed=1
  elif ! {
    while IFS= read -r line || [ -n "$line" ]; do
      if [[ "$line" == KILO_ENV_FIXED_ROOT=* ]]; then
        printf 'KILO_ENV_FIXED_ROOT=%q\n' "$isolated_root"
        fixed_root_replacements=$((fixed_root_replacements + 1))
      else
        printf '%s\n' "$line"
      fi
    done < "$KILO_ENV_SCRIPT_PATH"
  } > "$script_path"; then
    kilo_env_print_error \
      "unable to prepare isolated source-safety script"
    tests_failed=1
  elif [ "$fixed_root_replacements" -ne 1 ]; then
    kilo_env_print_error "source-safety script did not have exactly one fixed-root assignment"
    tests_failed=1
  elif [ -e "${isolated_root}/relay-map.json" ] || [ -e "${isolated_root}/relay-map.env" ]; then
    kilo_env_print_error \
      "source-safety test requires ${isolated_root} to contain neither relay-map.json nor relay-map.env"
    tests_failed=1
  elif ! child_output=$(
      PHLEX_RELAY_FIXTURE_DIR="$fixture_root" \
        bash --noprofile --norc -c '
script_path=$1
isolated_root=$2
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
sha256_text() {
  local input="$1"
  local sum_line
  if command -v shasum >/dev/null 2>&1 && \
    sum_line=$(printf %s "$input" | shasum -a 256 2>/dev/null); then
    printf "%s\n" "$sum_line"
    return 0
  fi
  if command -v sha256sum >/dev/null 2>&1 && \
    sum_line=$(printf %s "$input" | sha256sum 2>/dev/null); then
    printf "%s\n" "$sum_line"
    return 0
  fi
  return 1
}
before_opts_sha=$(sha256_text "$before_opts") || exit 1
before_opts_sha=${before_opts_sha%% *}
after_opts_sha=$(sha256_text "$after_opts") || exit 1
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
' bash "$script_path" "$isolated_root"
  ); then
    printf 'FAIL: production source-safety child shell exited unexpectedly\n' >&2
    tests_failed=1
  else
    printf 'PASS: production source-safety child shell stayed alive\n'
    source_safety_output_ready=1
  fi

  if [ "$source_safety_output_ready" -eq 1 ]; then
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
      "${isolated_root}/relay-map.json" \
      "source-safety copy uses the isolated relay-map.json path"; then
      tests_failed=1
    fi
    if ! kilo_env_assert_equal "$(kilo_env_extract_output_value "$child_output" "relays_env")" \
      "${isolated_root}/relay-map.env" \
      "source-safety copy uses the isolated relay-map.env path"; then
      tests_failed=1
    fi
    if ! kilo_env_assert_equal "$(kilo_env_extract_output_value "$child_output" "gateway")" \
      "$KILO_ENV_HOST_GATEWAY" \
      "production sourcing keeps PHLEX_HOST_GATEWAY"; then
      tests_failed=1
    fi
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

# kilo_env_run_self_tests_loaded - runner invoked after helper loaded
# Uses PHLEX_RELAY_FIXTURE_DIR (from environment) instead of argument
kilo_env_run_self_tests_loaded() {
  local fixture_root="${PHLEX_RELAY_FIXTURE_DIR-}"
  local scratch_root=""
  local status=0

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

  kilo_env_cleanup_self_test_root "$scratch_root" || return 1
  return "$status"
}
