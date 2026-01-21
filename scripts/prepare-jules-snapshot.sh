#!/bin/bash
set -euo pipefail

# This script prepares a development environment snapshot.
# It is intended to be run with root privileges.

if [[ "$EUID" -ne 0 ]]; then
  echo "Please run as root"
  exit 1
fi

echo "Starting environment snapshot preparation..."

echo "--> Installing and configuring system-level dependencies..."

export DEBIAN_FRONTEND=noninteractive
export LANG=en_US.UTF-8
export LC_ALL=en_US.UTF-8

apt-get update
apt-get install -y --no-install-recommends \
    ca-certificates \
    curl \
    git \
    gnupg \
    locales-all \
    software-properties-common \
    sudo

# Add repository for GCC 15 and install it
add-apt-repository -y ppa:ubuntu-toolchain-r/test
apt-get update
apt-get install -y --no-install-recommends gcc-15 g++-15

# Set GCC 15 as the default
update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-15 100
update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-15 100

echo "--> System-level dependencies installed successfully."

echo "--> Installing and configuring Spack..."

# Clone Spack
git clone --depth=2 https://github.com/spack/spack.git /spack

# Prepare shared group and directories for Spack
groupadd --gid 2000 spack
mkdir -p /opt/spack-stage /opt/spack-cache/downloads /opt/spack-cache/misc /opt/spack-environments
chgrp -R spack /spack /opt/spack-stage /opt/spack-cache /opt/spack-environments
chmod -R g+rwX /spack /opt/spack-stage /opt/spack-cache /opt/spack-environments
find /spack /opt/spack-stage /opt/spack-cache /opt/spack-environments -type d -exec chmod g+s {} +

# Set environment variables for Spack
export SPACK_USER_CONFIG_PATH=/dev/null
export SPACK_DISABLE_LOCAL_CONFIG=true
. /spack/share/spack/setup-env.sh

# Configure Spack
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

echo "--> Spack installed and configured successfully."

echo "--> Installing all Spack packages..."

# Create, activate, and concretize the Spack environment
PHLEX_SPACK_ENV=/opt/spack-environments/phlex-ci
cat <<'EOF' > /tmp/snapshot-spack.yaml
spack:
  specs:
  - phlex
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
      - "%gcc@15"


    # GCC 15 uses C23 as the default C language, and the versions of
    # libunwind and unuran available in Spack (as of 8/12/2025) do not
    # yet support C23.
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
spack env create -d "$PHLEX_SPACK_ENV" /tmp/snapshot-spack.yaml
spack env activate -d "$PHLEX_SPACK_ENV"
spack concretize

# Install all packages
spack install --fail-fast -j "$(nproc)"

echo "--> All Spack packages installed successfully."

echo "--> Installing developer tools..."

# Install GitHub's act CLI
download_url=$(curl -s https://api.github.com/repos/nektos/act/releases/latest | \
  grep -Ee '"browser_download_url": .*/act_Linux_x86_64\.' | \
  cut -d '"' -f 4)
if [ -z "$download_url" ]; then
  echo "Failed to determine act download URL" >&2
  exit 1
fi
curl -sfSL "$download_url" | tar -C /usr/local/bin -zx act
chmod +x /usr/local/bin/act

# Install GitHub CLI (gh)
curl -fsSL https://cli.github.com/packages/githubcli-archive-keyring.gpg | \
  dd of=/usr/share/keyrings/githubcli-archive-keyring.gpg
chmod go+r /usr/share/keyrings/githubcli-archive-keyring.gpg
echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/githubcli-archive-keyring.gpg] https://cli.github.com/packages stable main" \
  > /etc/apt/sources.list.d/github-cli.list
apt-get update
apt-get install -y --no-install-recommends gh

# Install ruff and gersemi via pip
PYTHONDONTWRITEBYTECODE=1 \
  pip --isolated --no-input --disable-pip-version-check --no-cache-dir install \
  --prefix /usr/local ruff gersemi

echo "--> Developer tools installed successfully."

echo "--> Performing aggressive cleanup..."

# Clean all Spack caches
spack clean --all

# Clean apt caches
apt-get clean
rm -rf /var/lib/apt/lists/*

# Remove the temporary spack environment file
rm -f /tmp/snapshot-spack.yaml

echo "--> Cleanup complete."
echo "Environment snapshot preparation is finished."
