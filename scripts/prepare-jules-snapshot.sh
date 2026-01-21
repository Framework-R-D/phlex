#!/bin/bash
set -euo pipefail

# This script prepares a development environment snapshot.
# It uses sudo for privileged operations.

echo "Starting environment snapshot preparation..."

echo "--> Installing and configuring system-level dependencies..."

# Use sudo for all system-level package management
sudo apt-get update
sudo apt-get install -y --no-install-recommends \
    ca-certificates \
    curl \
    git \
    gnupg \
    locales-all \
    software-properties-common \
    sudo

echo "--> System-level dependencies installed successfully."

echo "--> Installing and configuring Spack..."

# Clone Spack and set up directories with sudo
sudo git clone --depth=2 https://github.com/spack/spack.git /spack
sudo groupadd --gid 2000 spack || true # Use || true in case group exists
sudo mkdir -p /opt/spack-stage /opt/spack-cache/downloads /opt/spack-cache/misc /opt/spack-environments
sudo chgrp -R spack /spack /opt/spack-stage /opt/spack-cache /opt/spack-environments
sudo chmod -R g+rwX /spack /opt/spack-stage /opt/spack-cache /opt/spack-environments
sudo find /spack /opt/spack-stage /opt/spack-cache /opt/spack-environments -type d -exec chmod g+s {} +

# Create the spack.yaml file in /tmp as the current user
cat <<'EOF' > /tmp/snapshot-spack.yaml
spack:
  specs:
  - phlex
  - gcc@15.2.0
  - catch2
  - cmake
  - lcov
  - ninja
  - python
  - py-gcovr
  - py-numpy
  - py-pip # Needed temporarily for ruff installation
  - |
      llvm@21.1.4: +zstd +llvm_dylib +link_llvm_dylib targets=x86

  view: true
  concretizer:
    unify: true

  packages:
    all:
      target:
      - x86_64_v3

    cmake:
      require:
      - "~qtgui"
      - "@4:"

    phlex:
      require:
      - "+form"
      - "cxxstd=20"
      - "%gcc@15.2.0"


    # GCC 15 uses C23 as the default C language, and some packages
    # (e.g. libunwind, unuran) do not yet support it. We enforce C17
    # for compatibility.
    libunwind:
      require:
      - "cflags='-std=c17'"

    unuran:
      require:
      - "cflags='-std=c17'"

    # ROOT should be built with the same version of the C++ standard
    # as will be used by Phlex (C++20 for now).
    root:
      require:
      - "~x"  # No graphics libraries required
      - "cxxstd=20"
EOF

# Run all Spack operations within a single sudo shell session to ensure a consistent environment
sudo bash -c '
set -euo pipefail

echo "--> Configuring Spack repositories and settings..."

# Set environment variables for Spack
export SPACK_USER_CONFIG_PATH=/dev/null
export SPACK_DISABLE_LOCAL_CONFIG=true
. /spack/share/spack/setup-env.sh

# Configure Spack repos
SPACK_REPO_ROOT=/opt/spack-repos
mkdir -p "$SPACK_REPO_ROOT"
git clone --depth=1 https://github.com/FNALssi/fnal_art.git "$SPACK_REPO_ROOT/fnal_art"
git clone --depth=1 https://github.com/Framework-R-D/phlex-spack-recipes.git "$SPACK_REPO_ROOT/phlex-spack-recipes"
chgrp -R spack "$SPACK_REPO_ROOT"
chmod -R g+rwX "$SPACK_REPO_ROOT"
find "$SPACK_REPO_ROOT" -type d -exec chmod g+s {} +

spack repo add --scope site "$SPACK_REPO_ROOT/phlex-spack-recipes/spack_repo/phlex"
spack repo add --scope site "$SPACK_REPO_ROOT/fnal_art/spack_repo/fnal_art"
spack repo set --scope site --destination "$SPACK_REPO_ROOT" builtin
spack compiler find
spack external find --exclude python

spack config --scope defaults add config:build_stage:/opt/spack-stage
spack config --scope defaults add config:source_cache:/opt/spack-cache/downloads
spack config --scope defaults add config:misc_cache:/opt/spack-cache/misc

echo "--> Installing all Spack packages..."

# Create, activate, and concretize the Spack environment
PHLEX_SPACK_ENV=/opt/spack-environments/phlex-ci
spack env create -d "$PHLEX_SPACK_ENV" /tmp/snapshot-spack.yaml
spack env activate -d "$PHLEX_SPACK_ENV"
spack concretize

# Install all packages
spack install --fail-fast -j "$(nproc)"

echo "--> Spack packages installed. Cleaning up..."

# Install ruff and gersemi via pip
echo "--> Installing developer tools (ruff, gersemi)..."
PYTHONDONTWRITEBYTECODE=1 \
  pip --isolated --no-input --disable-pip-version-check --no-cache-dir install \
  --prefix /usr/local ruff gersemi

# Clean all Spack caches
echo "--> Cleaning Spack caches..."
spack clean --all
'

echo "--> Spack and Python tools setup complete."

echo "--> Installing additional developer tools (act, gh)..."

# Install GitHub'"'"'s act CLI
download_url=$(curl -s https://api.github.com/repos/nektos/act/releases/latest | grep -o '"browser_download_url": "[^"]*act_Linux_x86_64[^"]*"' | cut -d '"' -f 4)
if [ -z "$download_url" ]; then
  echo "Failed to determine act download URL" >&2
  exit 1
fi
curl -sfSL "$download_url" | sudo tar -C /usr/local/bin -zx act
sudo chmod +x /usr/local/bin/act

# Install GitHub CLI (gh)
curl -fsSL https://cli.github.com/packages/githubcli-archive-keyring.gpg | sudo dd of=/usr/share/keyrings/githubcli-archive-keyring.gpg
sudo chmod go+r /usr/share/keyrings/githubcli-archive-keyring.gpg
echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/githubcli-archive-keyring.gpg] https://cli.github.com/packages stable main" | sudo tee /etc/apt/sources.list.d/github-cli.list > /dev/null
sudo apt-get update
sudo apt-get install -y --no-install-recommends gh

echo "--> Additional developer tools installed successfully."

echo "--> Performing final cleanup..."

# Clean apt caches
sudo apt-get clean
sudo rm -rf /var/lib/apt/lists/*

# Remove the temporary spack environment file
rm -f /tmp/snapshot-spack.yaml

echo "--> Cleanup complete."
echo "Environment snapshot preparation is finished."
