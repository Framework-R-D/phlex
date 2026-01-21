#!/bin/bash
set -euo pipefail

# This script prepares a development environment snapshot.
# It uses sudo for privileged operations.

echo "Starting environment snapshot preparation..."

echo "--> Installing and configuring system-level dependencies..."

# Use sudo for all system-level package management
sudo apt-get update
sudo apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    curl \
    git \
    gnupg \
    locales-all \
    lsb-release \
    software-properties-common \
    sudo

echo "--> Verifying execution environment..."
echo "    Distributor ID: $(lsb_release -is)"
echo "    Release:        $(lsb_release -rs)"
echo "    Codename:       $(lsb_release -cs)"
echo "    Architecture:   $(uname -m)"

echo "--> System-level dependencies installed successfully."

echo "--> Installing and configuring Spack..."

# Create a user-owned directory for Spack to be installed into
export SPACK_DEV_ROOT="/opt/spack-dev"
sudo mkdir -p "$SPACK_DEV_ROOT"
sudo chown -R "$(id -u):$(id -g)" "$SPACK_DEV_ROOT"

# Clone Spack
git clone --depth=2 https://github.com/spack/spack.git "${SPACK_DEV_ROOT}/spack"

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

# All Spack operations can now be run as the regular user
(
  set -euo pipefail

  echo "--> Configuring Spack repositories and settings..."

  # Set environment variables for Spack
  export SPACK_USER_CONFIG_PATH=/dev/null
  export SPACK_DISABLE_LOCAL_CONFIG=true
  # shellcheck source=/dev/null
  . "${SPACK_DEV_ROOT}/spack/share/spack/setup-env.sh"

  # Configure Spack repos
  SPACK_REPO_ROOT="${SPACK_DEV_ROOT}/spack-repos"
  mkdir -p "$SPACK_REPO_ROOT"
  git clone --depth=1 https://github.com/FNALssi/fnal_art.git "$SPACK_REPO_ROOT/fnal_art"
  git clone --depth=1 https://github.com/Framework-R-D/phlex-spack-recipes.git "$SPACK_REPO_ROOT/phlex-spack-recipes"

  spack repo add --scope site "$SPACK_REPO_ROOT/phlex-spack-recipes/spack_repo/phlex"
  spack repo add --scope site "$SPACK_REPO_ROOT/fnal_art/spack_repo/fnal_art"
  spack repo set --scope site --destination "$SPACK_REPO_ROOT" builtin
  spack compiler find
  spack external find --exclude python

  spack config --scope defaults add "config:build_stage:${SPACK_DEV_ROOT}/spack-stage"
  spack config --scope defaults add "config:source_cache:${SPACK_DEV_ROOT}/spack-cache/downloads"
  spack config --scope defaults add "config:misc_cache:${SPACK_DEV_ROOT}/spack-cache/misc"

  echo "--> Installing all Spack packages..."

  # Create, activate, and concretize the Spack environment
  PHLEX_SPACK_ENV="${SPACK_DEV_ROOT}/spack-environments/phlex-ci"
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
    ruff gersemi

  # Clean all Spack caches
  echo "--> Cleaning Spack caches..."
  spack clean --all
)
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
