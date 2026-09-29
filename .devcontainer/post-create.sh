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

# Seed host auth into the container-private data volume without modifying the
# read-only host file or replacing credentials already saved in this container.
mkdir -p /root/.local/share/kilo
if [ -f /run/phlex-host-auth.json ] && [ ! -e /root/.local/share/kilo/auth.json ]; then
  install -m 0600 /run/phlex-host-auth.json /root/.local/share/kilo/auth.json
fi

# Seed the Kilo Code auth token into the container-private data volume.
# The volume is not shared with the host to avoid SQLite conflicts between
# the Remote-SSH and devcontainer Kilo Code instances.  The API key is
# passed in via the KILO_API_KEY remoteEnv variable.
if [ -n "${KILO_API_KEY:-}" ]; then
  python3 - <<'PY'
import json
import os
from pathlib import Path

# The single fnal-litellm provider was split into fnal-azure (optimized) and
# fnal-ow (passthrough); both share the same upstream gateway key.  Seed both
# provider keys so Kilo resolves whichever provider a model is routed through.
key = os.environ["KILO_API_KEY"]
p = Path("/root/.local/share/kilo/auth.json")
auth = json.loads(p.read_text(encoding="utf-8")) if p.exists() else {}
auth.update(
    {
        "fnal-azure": {"type": "api", "key": key},
        "fnal-ow": {"type": "api", "key": key},
    }
)
p.touch(mode=0o600, exist_ok=True)
p.chmod(0o600)
p.write_text(
    json.dumps(auth, indent=2) + "\n",
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

# KILO configuration now provided by /etc/profile.d/kilo-env.sh
