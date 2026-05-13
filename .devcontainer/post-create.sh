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

# Install pre-commit hooks if available.
if command -v prek >/dev/null 2>&1; then
  prek install || true
elif command -v pre-commit >/dev/null 2>&1; then
  pre-commit install || true
fi

# Install kiro-cli and set up shell integrations.
curl -fsSL https://cli.kiro.dev/install | bash
