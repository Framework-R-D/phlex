#!/bin/bash
set -euo pipefail

# This script prepares a development environment snapshot.
# It uses sudo for privileged operations.

{ set +x; } >/dev/null 2>&1
echo "Starting environment snapshot preparation..."

echo "--> Installing and configuring system-level dependencies..."
set -x

# Use sudo for all system-level package management

sudo DEBIAN_FRONTEND=noninteractive apt-get update
sudo DEBIAN_FRONTEND=noninteractive apt-get upgrade -y --no-install-recommends
sudo DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    cmake \
    curl \
    git \
    gnupg \
    locales-all \
    libcurl4-openssl-dev \
    libssl-dev \
    lsb-release \
    ninja-build \
    software-properties-common \
    sudo

sudo DEBIAN_FRONTEND=noninteractive apt autoremove -y
sudo apt-get clean
sudo rm -rf /var/lib/apt/lists/*

# Set locale
export LANG=en_US.UTF-8
export LC_ALL=en_US.UTF-8

{ set +x; } >/dev/null 2>&1
echo "--> Verifying execution environment..."
echo "    Distributor ID: $(lsb_release -is)"
echo "    Release:        $(lsb_release -rs)"
echo "    Codename:       $(lsb_release -cs)"
echo "    Architecture:   $(uname -m)"
echo "--> System-level dependencies installed successfully."
echo $'\n'
#echo "--------------------------------------------------------------------------------"
#echo "--> System packages installed:"
#apt list | bzip2 -c | gpg --enarmor
#echo "--------------------------------------------------------------------------------"
#echo $'\n'

echo "--> Installing and configuring Spack..."
set -x

# Create a user-owned directory for Spack to be installed into
export SPACK_DEV_ROOT="/opt/spack-dev"
sudo mkdir -p "$SPACK_DEV_ROOT"
sudo chown -R "$(id -u):$(id -g)" "$SPACK_DEV_ROOT"

# Clone Spack
git clone --depth=2 https://github.com/spack/spack.git "${SPACK_DEV_ROOT}/spack"

# All Spack operations can now be run as the regular user

( # Subshell to prevent unwanted leakage of Spack shell environment
  export SNAPSHOT_SPACK_YAML=/app/ci/spack.yaml
  set -euo pipefail

  { set +x; } >/dev/null 2>&1
  echo "--> Configuring Spack repositories and settings..."

  # Set environment variables for Spack
  export SPACK_USER_CONFIG_PATH=/dev/null
  export SPACK_DISABLE_LOCAL_CONFIG=true

  { set +x; } >/dev/null 2>&1
  # shellcheck source=/dev/null
  . "${SPACK_DEV_ROOT}/spack/share/spack/setup-env.sh"
  set -x

  # Configure Spack repos
  SPACK_REPO_ROOT="${SPACK_DEV_ROOT}/spack-repos"
  mkdir -p "$SPACK_REPO_ROOT"
  git clone --depth=1 https://github.com/FNALssi/fnal_art.git "$SPACK_REPO_ROOT/fnal_art"
  git clone --depth=1 https://github.com/Framework-R-D/phlex-spack-recipes.git "$SPACK_REPO_ROOT/phlex-spack-recipes"

  { set +x; } >/dev/null 2>&1
  spack repo add --scope site "$SPACK_REPO_ROOT/phlex-spack-recipes/spack_repo/phlex"
  spack repo add --scope site "$SPACK_REPO_ROOT/fnal_art/spack_repo/fnal_art"
  spack repo set --scope site --destination "$SPACK_REPO_ROOT" builtin
  spack compiler find --scope site
  spack compiler rm --scope site llvm || true
  spack compilers
  spack external find --exclude llvm --exclude python --exclude automake --exclude autoconf \
        --exclude ccache --exclude gawk --exclude go --exclude libtool --exclude m4 --exclude npm \
        --exclude pkgconf --exclude openssh --exclude rust

  spack config --scope defaults add "config:build_stage:${SPACK_DEV_ROOT}/spack-stage"
  spack config --scope defaults add "config:source_cache:${SPACK_DEV_ROOT}/spack-cache/downloads"
  spack config --scope defaults add "config:misc_cache:${SPACK_DEV_ROOT}/spack-cache/misc"
  spack config --scope defaults add "config:install_tree:padded_length:255"
  spack config --scope defaults add "config:url_fetch_method:curl"

  { set +x; } >/dev/null 2>&1
  spack --timestamp gpg init
  spack --timestamp mirror add --type binary phlex-ci-scisoft \
        https://scisoft.fnal.gov/scisoft/spack-packages/phlex-dev
  spack --debug --timestamp buildcache list -LNav
  spack --timestamp buildcache keys -fit
  spack --timestamp buildcache check-index phlex-ci-scisoft
  spack --timestamp gpg list

  { set +x; } >/dev/null 2>&1
  echo "--> Installing GCC 15.x ..."
  set -x

  spack --timestamp install --cache-only --fail-fast -j "$(nproc)" \
        gcc @15 +binutils+bootstrap+graphite~nvptx+piclibs+profiled+strip \
        build_type=Release \
        languages=c,c++,fortran,lto

  { set +x; } >/dev/null 2>&1
  echo "--> Concretizing Phlex Spack environment ..."
  set -x

  # Create, activate, and concretize the Spack environment
  PHLEX_SPACK_ENV="${SPACK_DEV_ROOT}/spack-environments/phlex-ci"
  spack env create -d "$PHLEX_SPACK_ENV" "$SNAPSHOT_SPACK_YAML"
  spack env activate -d "$PHLEX_SPACK_ENV"
  spack concretize

  { set +x; } >/dev/null 2>&1
  echo "--> Installing all environment roots except Phlex ..."
  set -x

  spack --timestamp install --cache-only --fail-fast -j $(nproc) -p 1 \
        --no-add --only-concrete \
        $(spack find -r --no-groups | sed -Ene 's&^ - &&p' | grep -v phlex)

  { set +x; } >/dev/null 2>&1
  echo "--> Installing Phlex dependencies ..."
  set -x

  spack --timestamp install --cache-only --fail-fast -j $(nproc) -p 1 \
        --no-add --only-concrete --only dependencies --no-check-signature \
        phlex

  { set +x; } >/dev/null 2>&1
  echo "--> Spack packages installed."
  set -x

  # Install ruff and gersemi via pip
  { set +x; } >/dev/null 2>&1
  echo "--> Installing developer tools (ruff, gersemi)..."
  set -x

  PYTHONDONTWRITEBYTECODE=1 \
    pip --isolated --no-input --disable-pip-version-check --no-cache-dir install \
    ruff gersemi

  # Clean all Spack caches
  { set +x; } >/dev/null 2>&1
  echo "--> Cleaning Spack caches..."
  set -x

  spack clean -dfs
)

{ set +x; } >/dev/null 2>&1
echo "--> Spack and Python tools setup complete."

echo "--> Installing additional developer tools (act, gh)..."
set -x

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

{ set +x; } >/dev/null 2>&1
echo "--> Additional developer tools installed successfully."

echo "--> Performing final cleanup..."
set -x

# Clean apt caches
sudo apt-get clean
sudo rm -rf /var/lib/apt/lists/*

{ set +x; } >/dev/null 2>&1
echo "--> Cleanup complete."
echo "Environment snapshot preparation is finished."
