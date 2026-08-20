#!/usr/bin/env bash
# Prepare KILO_CONFIG_CONTENT with rewritten headroom base URLs for container runtime.
# This script is sourced by /etc/profile.d/ so the variable is available to all processes.

# Unset any previous KILO_CONFIG_CONTENT to ensure a clean slate.
unset KILO_CONFIG_CONTENT

# If the headroom proxy ports (9797, 9798) are active via socat relays,
# Kilo needs a modified config with baseURL pointing to host.docker.internal.
# Parse the host config, rewrite baseURLs, and export as KILO_CONFIG_CONTENT.
if command -v kilo >/dev/null 2>&1 && command -v jq >/dev/null 2>&1; then
  # Only rewrite if we have a working Kilo config to work with.
  KILO_CONFIG_CONTENT="$(perl -pe 's{("(?:[^"\\]|\\.)*"|'"'"'(?:[^'"'"'\\]|\\.)*'"'"')|//.*}{$1 // ""}ge' "${HOME}/.config/kilo/kilo.jsonc" | jq -c '.provider |= with_entries(
        .value.options.baseURL =
        (if .value.options.baseURL == null then null
         elif (.value.options.baseURL | test("^https?://127\\.0\\.0\\.1:"))
         then (.value.options.baseURL | capture("https?://127\\.0\\.0\\.1:(?<port>[0-9]+)(?<rest>.*)") | "http://host.docker.internal:" + ((.port | tonumber + 10000) | tostring) + .rest)
         else .value.options.baseURL
         end)
      )')"
  export KILO_CONFIG_CONTENT
fi
