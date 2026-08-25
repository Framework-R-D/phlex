#!/usr/bin/env bash
# Prepare KILO_CONFIG_CONTENT with rewritten headroom base URLs for container runtime.
# This script is sourced by /etc/profile.d/ so the variable is available to all processes.

# Unset any previous KILO_CONFIG_CONTENT to ensure a clean slate.
unset KILO_CONFIG_CONTENT

# Discover Kilo configuration from mounted container path
# Kilo configuration is provided as a mounted file at /run/phlex-host-relays/
KILO_CONFIG_MOUNTED="/run/phlex-host-relays/kilo.json"
KILO_CONFIG_MOUNTED_C="/run/phlex-host-relays/kilo.jsonc"

# Determine which config file exists
if [ -f "$KILO_CONFIG_MOUNTED" ]; then
  KILO_CONFIG_PATH="$KILO_CONFIG_MOUNTED"
elif [ -f "$KILO_CONFIG_MOUNTED_C" ]; then
  KILO_CONFIG_PATH="$KILO_CONFIG_MOUNTED_C"
else
  # Fallback: no mounted config
  KILO_CONFIG_PATH=""
fi

# Read relay maps from mounted path or fallback to tmp paths
RELAY_MAP_JSON="/run/phlex-host-relays/relay-map.json"
RELAY_MAP_ENV="/run/phlex-host-relays/relay-map.env"

if [ ! -f "$RELAY_MAP_JSON" ]; then
  RELAY_MAP_JSON="$HOME/.phlex-devcontainer-tmp/relays/relay-map.json"
fi
if [ ! -f "$RELAY_MAP_ENV" ]; then
  RELAY_MAP_ENV="$HOME/.phlex-devcontainer-tmp/relays/relay-map.env"
fi

# Export relay environment paths (always set, even if empty)
export PHLEX_HOST_GATEWAY="host.docker.internal"
export PHLEX_HOST_RELAY_FILE="$RELAY_MAP_JSON"
export PHLEX_HOST_RELAYS_ENV="$RELAY_MAP_ENV"
export PHLEX_PODMAN_SOCKET_SOURCE="podman-machine"

# Parse relay environment file to set relay-specific env vars
if [ -f "$RELAY_MAP_ENV" ] && [ -s "$RELAY_MAP_ENV" ]; then
  while IFS='=' read -r key value; do
    # Skip empty lines and comments
    [[ -z "$key" || "$key" =~ ^# ]] && continue
    # Export the relay mapping as an environment variable
    export "$key"="$value"
  done < "$RELAY_MAP_ENV"
fi

# Only rewrite Kilo config if we have working tools and a config file
if command -v jq >/dev/null 2>&1 && [ -n "$KILO_CONFIG_PATH" ] && [ -f "$KILO_CONFIG_PATH" ]; then
  # Read the config file and strip comments (JSONC support)
  KILO_CONFIG_STR=$(perl -pe 's{([^"\\/]|\\.)*(?:"([^"\\]|\\.)*"|//.*$)}{$1 // ""}ge' "$KILO_CONFIG_PATH" 2>/dev/null | \
                   perl -pe 's{/\*.*?\*/}{}gs' 2>/dev/null | \
                   jq -c '.' 2>/dev/null)

  if [ -n "$KILO_CONFIG_STR" ]; then
    # Rewrite baseURLs in provider configurations
    # Only rewrite loopback URLs (127.0.0.1:*) to host.docker.internal
    KILO_CONFIG_CONTENT=$(echo "$KILO_CONFIG_STR" | jq -c '.provider |= with_entries(
      .value.options.baseURL =
      (if .value.options.baseURL == null then null
       elif (.value.options.baseURL | test("^https?://127\\.0\\.0\\.1:"))
       then (.value.options.baseURL | capture("https?://127\\.0\\.0\\.1:(?<port>[0-9]+)(?<rest>.*)") | "https://host.docker.internal:" + ((.port | tonumber + 10000) | tostring) + .rest)
       else .value.options.baseURL
       end)
    )')

    # Export only if rewriting succeeded
    if [ -n "$KILO_CONFIG_CONTENT" ]; then
      export KILO_CONFIG_CONTENT
    fi
  fi
fi
