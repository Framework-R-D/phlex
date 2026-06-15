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

# Set KILO_CONFIG_CONTENT for interactive shells.  When KILO_CONFIG_CONTENT_DOCKER
# is non-empty (headroom local proxy via socat relay), use it so that the baseURL
# points at host.docker.internal rather than 127.0.0.1.  Otherwise leave
# KILO_CONFIG_CONTENT unset so Kilo falls back to ~/.config/kilo/kilo.jsonc,
# which is bind-mounted from the host and may point at an external provider.
if [ -n "${KILO_CONFIG_CONTENT_DOCKER:-}" ]; then
  printf 'export KILO_CONFIG_CONTENT=%q\n' "${KILO_CONFIG_CONTENT_DOCKER}" >> /root/.bashrc
fi

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

p = Path("/root/.local/share/kilo/auth.json")
p.write_text(
    json.dumps(
        {"fnal-litellm": {"type": "api", "key": os.environ["KILO_API_KEY"]}},
        indent=2,
    )
    + "\n",
    encoding="utf-8",
)
PY
fi

# Install pre-commit hooks if available.
if command -v prek >/dev/null 2>&1; then
  prek install || true
elif command -v pre-commit >/dev/null 2>&1; then
  pre-commit install || true
fi

# Install kiro-cli and set up shell integrations.
curl -fsSL https://cli.kiro.dev/install | bash
