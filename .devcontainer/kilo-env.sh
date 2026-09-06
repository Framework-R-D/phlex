#!/usr/bin/env bash
# Prepare KILO_CONFIG_CONTENT with rewritten loopback URLs based on relay map.
# This script is sourced by /etc/profile.d/ so the variable is available to all processes.
# Conforms to kilo-plan/v4 step 3b: write-free Kilo configuration rewriting.

set -uo pipefail

# Unset any previous KILO_CONFIG_CONTENT to ensure a clean slate.
unset KILO_CONFIG_CONTENT

# Global variables for self-test
SELF_TEST_MODE=false
SELF_TEST_DIR=""

# Parse command line arguments
parse_args() {
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --self-test)
                SELF_TEST_MODE=true
                SELF_TEST_DIR="$2"
                shift 2
                ;;
            *)
                # For sourced usage, pass unknown args through
                if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
                    echo "Unknown option: $1" >&2
                    exit 1
                fi
                return 0
                ;;
        esac
    done
}

# Parse relay maps from mounted path
RELAY_MAP_JSON="/run/phlex-host-relays/relay-map.json"
RELAY_MAP_ENV="/run/phlex-host-relays/relay-map.env"

# Discover Kilo configuration from mounted container path (ordered candidates)
KILO_CONFIG_CANDIDATES=(
    "/run/phlex-host-relays/kilo.json"
    "/run/phlex-host-relays/kilo.jsonc"
)

# Discover the first existing Kilo config file
KILO_CONFIG_PATH=""
for candidate in "${KILO_CONFIG_CANDIDATES[@]}"; do
    if [[ -f "$candidate" ]]; then
        KILO_CONFIG_PATH="$candidate"
        break
    fi
done
export KILO_CONFIG_PATH
export RELAY_MAP_JSON

# Export relay environment paths (always set, even if empty)
export PHLEX_HOST_GATEWAY="host.docker.internal"
export PHLEX_HOST_RELAY_FILE="$RELAY_MAP_JSON"
export PHLEX_HOST_RELAYS_ENV="$RELAY_MAP_ENV"
export PHLEX_PODMAN_SOCKET_SOURCE="podman-machine"

# Export relay-specific environment variables from relay-map.env
if [[ -f "$RELAY_MAP_ENV" ]] && [[ -s "$RELAY_MAP_ENV" ]]; then
    while IFS='=' read -r key value; do
        # Skip empty lines and comments
        [[ -z "$key" || "$key" =~ ^# ]] && continue
        # Export the relay mapping as an environment variable
        export "$key"="$value"
    done < "$RELAY_MAP_ENV"
fi

# Main processing function
process_config() {
    # Only process if we have required tools
    if ! command -v python3 >/dev/null 2>&1; then
        return 0
    fi

    if [[ -z "$KILO_CONFIG_PATH" ]] || [[ ! -f "$KILO_CONFIG_PATH" ]]; then
        return 0
    fi

    # Use Python for the complete workflow
    KILO_CONFIG_CONTENT=$(python3 << 'PYEOF'
import json
import os
import re
import sys

def parse_jsonc(content):
    """Parse JSONC (JSON with comments) into clean JSON."""
    result = []
    i = 0
    in_string = False
    escape_next = False
    in_block_comment = False

    while i < len(content):
        ch = content[i]

        if in_block_comment:
            if ch == '*' and i + 1 < len(content) and content[i + 1] == '/':
                in_block_comment = False
                i += 2
            else:
                i += 1
            continue

        if escape_next:
            result.append(ch)
            escape_next = False
            i += 1
            continue

        if in_string:
            if ch == '\\':
                escape_next = True
                result.append(ch)
                i += 1
            elif ch == '"':
                in_string = False
                result.append(ch)
                i += 1
            else:
                result.append(ch)
                i += 1
            continue

        if ch == '"':
            in_string = True
            result.append(ch)
            i += 1
            continue
        elif ch == '/' and i + 1 < len(content):
            if content[i + 1] == '/':
                while i < len(content) and content[i] != '\n':
                    i += 1
                continue
            elif content[i + 1] == '*':
                in_block_comment = True
                i += 2
                continue

        if ch == ',' and i + 1 < len(content):
            j = i + 1
            while j < len(content) and content[j] in ' \t\n\r':
                j += 1
            if j < len(content) and content[j] in ']}':
                i += 1
                continue

        result.append(ch)
        i += 1

    clean_json = ''.join(result)
    json.loads(clean_json)
    return clean_json

def rewrite_urls(json_content, relay_map):
    """Rewrite HTTP(S) loopback URLs based on relay map."""
    try:
        data = json.loads(json_content)

        if 'provider' in data and isinstance(data['provider'], dict):
            for provider_name, provider_config in data['provider'].items():
                if isinstance(provider_config, dict) and 'options' in provider_config:
                    options = provider_config['options']
                    if isinstance(options, dict) and 'baseURL' in options:
                        base_url = options['baseURL']
                        if isinstance(base_url, str):
                            match = re.match(r'^(https?)://127\.0\.0\.1:(\d+)(.*)$', base_url)
                            if match:
                                scheme = match.group(1)
                                port = match.group(2)
                                rest = match.group(3)
                                if port in relay_map:
                                    relay_port = relay_map[port]
                                    new_url = f'{scheme}://host.docker.internal:{relay_port}{rest}'
                                    options['baseURL'] = new_url

        return json.dumps(data, separators=(',', ':'))
    except Exception:
        return None

def main():
    # Consume the producer's exact JSON map; env lines are exports only.
    map_path = os.environ.get('RELAY_MAP_JSON', '')
    if not map_path:
        return 1
    try:
        with open(map_path, 'r') as map_file:
            relay_map = json.load(map_file)
        if not isinstance(relay_map, dict):
            return 1
        for source_port, relay_port in relay_map.items():
            if (not re.fullmatch(r'[0-9]+', str(source_port)) or
                    not re.fullmatch(r'[0-9]+', str(relay_port))):
                return 1
        relay_map = {str(source): str(dest) for source, dest in relay_map.items()}
    except Exception:
        return 1

    # Read config file
    config_path = os.environ.get('KILO_CONFIG_PATH', '')
    if not config_path:
        return 1

    try:
        with open(config_path, 'r') as f:
            original_content = f.read()
    except Exception:
        return 1

    # Parse JSONC
    try:
        clean_json = parse_jsonc(original_content)
    except Exception:
        return 1

    # Rewrite URLs
    rewritten = rewrite_urls(clean_json, relay_map)
    if rewritten is None:
        return 1

    # Output result
    print(rewritten)
    return 0

if __name__ == '__main__':
    sys.exit(main())
PYEOF
)

    if [[ -n "$KILO_CONFIG_CONTENT" ]]; then
        export KILO_CONFIG_CONTENT
    fi
}

# Self-test helper functions
run_self_test() {
    local test_dir="$1"
    local test_name="$2"
    local expected_result="$3"

    echo "Test: $test_name"

    # Clean environment
    unset KILO_CONFIG_CONTENT

    # Set up test environment
    export KILO_CONFIG_PATH="$test_dir/config.json"
    export RELAY_MAP_JSON="$test_dir/relay-map.json"

    # Unset PHLEX_HOST_RELAY_* variables by reading them first
    local relay_var_list
    relay_var_list=$(compgen -v "PHLEX_HOST_RELAY_" 2>/dev/null || true)
    if [[ -n "$relay_var_list" ]]; then
        for var in $relay_var_list; do
            unset "$var" || true
        done
    fi

    if [[ -f "$test_dir/relay.env" ]]; then
        printf '%s\n' '{}' > "$RELAY_MAP_JSON"
        python3 - "$test_dir/relay.env" "$RELAY_MAP_JSON" << 'PYEOF'
import json
import sys

env_path, map_path = sys.argv[1:]
mapping = {}
with open(env_path, encoding='utf-8') as env_file:
    for line in env_file:
        key, value = line.rstrip('\n').split('=', 1)
        if key.startswith('PHLEX_HOST_RELAY_'):
            mapping[key.removeprefix('PHLEX_HOST_RELAY_')] = value
with open(map_path, 'w', encoding='utf-8') as map_file:
    json.dump(mapping, map_file, sort_keys=True)
PYEOF
        while IFS='=' read -r key value; do
            [[ -z "$key" || "$key" =~ ^# ]] && continue
            export "$key"="$value"
        done < "$test_dir/relay.env"
    fi

    # Run processing
    if process_config; then
        if [[ "$expected_result" == "success" ]]; then
            if [[ -n "${KILO_CONFIG_CONTENT:-}" ]]; then
                echo "  PASS: KILO_CONFIG_CONTENT exported"
                echo "$KILO_CONFIG_CONTENT"
                if [[ "$test_name" == "loopback-only" ]] &&
                    [[ "$KILO_CONFIG_CONTENT" == *'"nonloopback":{"options":{"baseURL":"https://host.docker.internal:25115/v1"}}'* ]]; then
                    echo "  PASS: Non-loopback URL unchanged"
                elif [[ "$test_name" == "loopback-only" ]]; then
                    echo "  FAIL: Non-loopback URL changed"
                    return 1
                fi
            else
                echo "  FAIL: Expected KILO_CONFIG_CONTENT but got none"
                return 1
            fi
        else
            if [[ -z "${KILO_CONFIG_CONTENT:-}" ]]; then
                echo "  PASS: No KILO_CONFIG_CONTENT (as expected)"
            else
                echo "  FAIL: Did not expect KILO_CONFIG_CONTENT"
                return 1
            fi
        fi
    else
        echo "  FAIL: process_config returned non-zero"
        return 1
    fi

    return 0
}

# Self-test entry point
run_self_tests() {
    local test_dir="$1"

    echo "=== Kilo Config Rewrite Self-Test ==="
    echo "Test directory: $test_dir"

    local all_passed=true

    # Test 1: JSON rewriting with map
    mkdir -p "$test_dir/json-test"
    cat > "$test_dir/json-test/config.json" << 'JSONEOF'
{
  "provider": {
    "anthropic": {
      "options": {
        "baseURL": "https://127.0.0.1:25114/v1"
      }
    },
    "openai": {
      "options": {
        "baseURL": "https://127.0.0.1:25300/v1"
      }
    },
    "unmapped": {
      "options": {
        "baseURL": "https://127.0.0.1:9999/v1"
      }
    },
    "nonloopback": {
      "options": {
        "baseURL": "https://api.example.com/v1"
      }
    }
  }
}
JSONEOF
    cat > "$test_dir/json-test/relay.env" << 'ENVEOF'
PHLEX_HOST_RELAY_25114=25115
PHLEX_HOST_RELAY_25300=25301
ENVEOF

    if ! run_self_test "$test_dir/json-test" "json-rewrite" "success"; then
        all_passed=false
    fi
    echo ""

    # Test 2: JSONC with comments
    mkdir -p "$test_dir/jsonc-test"
    cat > "$test_dir/jsonc-test/config.json" << 'JSONEOF'
{
  // This is a comment
  "provider": {
    /* Block comment */
    "anthropic": {
      "options": {
        "baseURL": "https://127.0.0.1:25114/v1"
      }
    }
  }
}
JSONEOF
    cat > "$test_dir/jsonc-test/relay.env" << 'ENVEOF'
PHLEX_HOST_RELAY_25114=25115
ENVEOF

    if ! run_self_test "$test_dir/jsonc-test" "jsonc-comments" "success"; then
        all_passed=false
    fi
    echo ""

    # Test 3: Malformed JSON (should fail closed)
    mkdir -p "$test_dir/malformed-test"
    cat > "$test_dir/malformed-test/config.json" << 'JSONEOF'
{
  "provider": {
    "anthropic": {
      "options": {
        "baseURL": "https://127.0.0.1:25114/v1"
      }
    }
  }
JSONEOF

    echo "Test: malformed-json (fails closed)"
    unset KILO_CONFIG_CONTENT
    export KILO_CONFIG_PATH="$test_dir/malformed-test/config.json"
    export RELAY_MAP_JSON="$test_dir/malformed-test/relay-map.json"
    printf '%s\n' '{}' > "$RELAY_MAP_JSON"

    local relay_var_list
    relay_var_list=$(compgen -v "PHLEX_HOST_RELAY_" 2>/dev/null || true)
    if [[ -n "$relay_var_list" ]]; then
        for var in $relay_var_list; do
            unset "$var" || true
        done
    fi

    if process_config 2>/dev/null; then
        if [[ -z "${KILO_CONFIG_CONTENT:-}" ]]; then
            echo "  PASS: Malformed input failed closed"
        else
            echo "  FAIL: Malformed input should have failed"
            all_passed=false
        fi
    else
        echo "  PASS: Malformed input failed with error code"
    fi
    echo ""

    # Test 4: Loopback-only URLs (should skip non-loopback)
    mkdir -p "$test_dir/loopback-test"
    cat > "$test_dir/loopback-test/config.json" << 'JSONEOF'
{
  "provider": {
    "loopback": {
      "options": {
        "baseURL": "https://127.0.0.1:25114/v1"
      }
    },
    "nonloopback": {
      "options": {
        "baseURL": "https://host.docker.internal:25115/v1"
      }
    },
    "unmapped": {
      "options": {
        "baseURL": "http://127.0.0.1:9999/v1"
      }
    }
  }
}
JSONEOF
    cat > "$test_dir/loopback-test/relay.env" << 'ENVEOF'
PHLEX_HOST_RELAY_25114=25115
ENVEOF

    if ! run_self_test "$test_dir/loopback-test" "loopback-only" "success"; then
        all_passed=false
    fi
    echo ""

    # Test 5: No write assertions (verify no files written)
    mkdir -p "$test_dir/no-write-test"
    cat > "$test_dir/no-write-test/config.json" << 'JSONEOF'
{
  "provider": {
    "test": {
      "options": {
        "baseURL": "https://127.0.0.1:25114/v1"
      }
    }
  }
}
JSONEOF
    cat > "$test_dir/no-write-test/relay.env" << 'ENVEOF'
PHLEX_HOST_RELAY_25114=25115
ENVEOF

    local config_before
    config_before=$(md5sum "$test_dir/no-write-test/config.json" 2>/dev/null || echo "notfound")

    echo "Test: no-write (config not modified)"
    unset KILO_CONFIG_CONTENT
    export KILO_CONFIG_PATH="$test_dir/no-write-test/config.json"

    relay_var_list=$(compgen -v "PHLEX_HOST_RELAY_" 2>/dev/null || true)
    if [[ -n "$relay_var_list" ]]; then
        for var in $relay_var_list; do
            unset "$var" || true
        done
    fi

    if process_config; then
        local config_after
        config_after=$(md5sum "$test_dir/no-write-test/config.json" 2>/dev/null || echo "notfound")
        if [[ "$config_before" == "$config_after" ]]; then
            echo "  PASS: Config file not modified"
        else
            echo "  FAIL: Config file was modified"
            all_passed=false
        fi
    else
        echo "  FAIL: process_config failed"
        all_passed=false
    fi
    echo ""

    if $all_passed; then
        echo "=== All tests passed ==="
        return 0
    else
        echo "=== Some tests failed ==="
        return 1
    fi
}

# Main entry point
parse_args "$@"

if $SELF_TEST_MODE; then
    run_self_tests "$SELF_TEST_DIR"
else
    process_config || unset KILO_CONFIG_CONTENT
fi
