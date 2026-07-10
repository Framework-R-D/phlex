#!/bin/bash
# Run inside the dev container after creation.

set -euo pipefail

# Configure act to use the Podman socket and run privileged so that nested
# container operations (e.g. workflow_dispatch testing) work correctly.
cat > ~/.actrc <<'EOF'
--container-daemon-socket unix:///tmp/podman.sock
--container-options --privileged
--container-options --userns=keep-id
EOF

# Clear the VS Code extension-install marker so that VS Code always installs the
# extensions listed in devcontainer.json on a fresh container. Without this, the
# marker persists on the host via the Machine data bind-mount and VS Code skips
# installation on every rebuild.
rm -f /root/.vscode-server-insiders/data/Machine/.installExtensionsMarker

# Seed the Kilo Code auth token into the container-private data volume.
# The volume is not shared with the host to avoid SQLite conflicts between
# the Remote-SSH and devcontainer Kilo Code instances.  The API key is
# passed in via the KILO_API_KEY remoteEnv variable.
if [ -n "${KILO_API_KEY:-}" ]; then
  mkdir -p /root/.local/share/kilo
  touch /root/.local/share/kilo/auth.json
  chmod 0600 /root/.local/share/kilo/auth.json
  python3 - <<'PY'
import json
import os
from pathlib import Path

# The single fnal-litellm provider was split into fnal-azure (optimized) and
# fnal-ow (passthrough); both share the same upstream gateway key.  Seed both
# provider keys so Kilo resolves whichever provider a model is routed through.
key = os.environ["KILO_API_KEY"]
p = Path("/root/.local/share/kilo/auth.json")
p.write_text(
    json.dumps(
        {
            "fnal-azure": {"type": "api", "key": key},
            "fnal-ow": {"type": "api", "key": key},
        },
        indent=2,
    )
    + "\n",
    encoding="utf-8",
)
PY
fi

# Expose repo scripts that follow the git-<subcommand> naming convention as
# proper git subcommands by symlinking them into /usr/local/bin.  This makes
# `git ai-commit` work in any directory without requiring scripts/ on PATH.
ln -sf /workspaces/phlex/scripts/git-ai-commit /usr/local/bin/git-ai-commit

# Adjust MANPATH to enable `git help ai-commit` to find the man page.
# The scripts/man/man1 directory contains git-subcommand man pages.
# Prepend the repo man directory and ensure the resulting MANPATH always has
# an empty field (which tells man to search the system default paths).  When
# there is no pre-existing MANPATH, or the existing one lacks an empty field,
# a trailing colon is appended to provide one; if an empty field already
# exists it is preserved as-is.
cat >> /root/.bashrc <<'EOF'

prepend_to_manpath() {
  # Prepends one or more paths to MANPATH, preserving the position of any
  # existing empty field (which tells man(1) where to insert the system
  # default paths).  Handles unset, empty, or degenerate MANPATH safely.
  # If MANPATH has no empty field, a trailing : is added (system paths last).
  local prefix
  prefix=$(IFS=:; echo "$*")
  # Unset or degenerate (only colons): start fresh, system defaults last.
  if [[ -z ${MANPATH+set} || -z "${MANPATH//:}" ]]; then
    export MANPATH="${prefix}:"
    return
  fi
  case "$MANPATH" in
    *::*|*:|:*)
      # Already has an empty field somewhere; prepend and keep it in place.
      # :foo  -> newpath::foo  (leading : becomes embedded :: after prepend)
      # foo:  -> newpath:foo:  (trailing : stays trailing)
      # f::b  -> newpath:f::b  (embedded :: stays embedded)
      export MANPATH="${prefix}:${MANPATH}" ;;
    *)
      # No empty field: add trailing : so system paths are still searched.
      export MANPATH="${prefix}:${MANPATH}:" ;;
  esac
}

prepend_to_manpath "/workspaces/phlex/scripts/man"
EOF

# Install pre-commit hooks if available.
if command -v prek >/dev/null 2>&1; then
  prek install || true
elif command -v pre-commit >/dev/null 2>&1; then
  pre-commit install || true
fi

# Configure Kilo to use host.docker.internal for headroom proxy ports.
# The headroom SSH tunnels bind to 127.0.0.1 only, but rootless Podman's
# pasta networking only routes host.docker.internal to ports listening on
# 0.0.0.0.  We use socat relays (set up in ensure-repos.sh) to forward
# 127.0.0.1:9797 -> 0.0.0.0:19797 and 127.0.0.1:9798 -> 0.0.0.0:19798.
# This code rewrites Kilo's baseURL from 127.0.0.1:<port> to
# host.docker.internal:<port+10000> for the relays to work.
cat >> /root/.bashrc <<'EOF'

# Unset any previous KILO_CONFIG_CONTENT to ensure a clean slate.
unset KILO_CONFIG_CONTENT

# If the headroom proxy ports (9797, 9798) are active via socat relays,
# Kilo needs a modified config with baseURL pointing to host.docker.internal.
# Parse 'kilo debug config', rewrite baseURLs, and export as KILO_CONFIG_CONTENT.
if command -v kilo >/dev/null 2>&1 && command -v jq >/dev/null 2>&1; then
  # Only rewrite if we have a working Kilo config to work with.
  if KILO_CONFIG_CONTENT="$(perl -pe 's{("(?:[^"\\]|\\.)*"|'"'"'(?:[^'"'"'\\]|\\.)*'"'"')|//.*}{$1 // ""}ge' "${HOME}/.config/kilo/kilo.jsonc" | jq -c '.provider |= with_entries(
        .value.options.baseURL =
        (if .value.options.baseURL == null then null
         elif (.value.options.baseURL | test("^https?://127\\.0\\.0\\.1:"))
         then (.value.options.baseURL | capture("https?://127\\.0\\.0\\.1:(?<port>[0-9]+)(?<rest>.*)") | "http://host.docker.internal:" + ((.port | tonumber + 10000) | tostring) + .rest)
         else .value.options.baseURL
         end)
      )')"; then
    export KILO_CONFIG_CONTENT
  fi
fi
EOF
