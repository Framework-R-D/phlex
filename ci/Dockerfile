# syntax=docker/dockerfile:1.4

# Multi-target Dockerfile for building the Phlex CI and development images.

# Podman instructions for building tagged images with this file:
#
#   $ podman build --format docker \
#       [-v <local-spack-cache>:/build-cache] \
#       [-v ~/.gnupg:/run/gpg] \
#       --target ci   --tag phlex-ci:<tag>  .
#   $ podman build --format docker \
#       [-v <local-spack-cache>:/build-cache] \
#       [-v ~/.gnupg:/run/gpg] \
#       --target dev  --tag phlex-dev:<tag> .
#   $ podman login ghcr.io --username <username> -p <token>
#   $ podman push phlex-ci:<tag>  ghcr.io/framework-r-d/phlex-ci:<tag>
#   $ podman push phlex-dev:<tag> ghcr.io/framework-r-d/phlex-dev:<tag>
# ... and optionally push either image as "latest".
#
# where <tag> is the date (e.g. "2025-08-12").
#
# Binary package signing (optional):
#   When -v <local-spack-cache>:/build-cache is supplied, newly built
#   packages are pushed to that cache.  They are signed when a GPG
#   private key is available; otherwise they are written unsigned.
#   The signing key is selected in this order:
#     1. The GPG_KEY_ID build argument (--build-arg GPG_KEY_ID=<key-id>).
#     2. The first private key found in GNUPGHOME (/run/gpg), which is
#        populated by mounting a GPG home directory:
#          -v ~/.gnupg:/run/gpg
#   If neither source yields a key the packages are written unsigned.
#
#   The local cache mirror is registered with --unsigned so that packages
#   built in earlier layers can be read back without requiring a signature.
#
# Running act inside a phlex-dev container:
#   act uses the Docker SDK to communicate with a container daemon; it
#   does not require a Docker/Podman CLI inside the image.  Mount the
#   host's rootless Podman socket and advertise it via DOCKER_HOST:
#
#     $ podman run --rm -it \
#         -v ${XDG_RUNTIME_DIR}/podman/podman.sock:/run/podman/podman.sock \
#         -e DOCKER_HOST=unix:///run/podman/podman.sock \
#         phlex-dev:<tag>
#
#   The devcontainer.json already configures both the mount and the env
#   var automatically when using VS Code / GitHub Codespaces.
#   Host prerequisite: systemctl --user enable --now podman.socket

ARG GPG_KEY_ID
FROM ubuntu:24.04 AS base

ENV DEBIAN_FRONTEND=noninteractive
ENV LANG=en_US.UTF-8
ENV LC_ALL=en_US.UTF-8
ENV SPACK_USER_CONFIG_PATH=/dev/null
ENV SPACK_DISABLE_LOCAL_CONFIG=true
ENV PHLEX_SPACK_ENV=/opt/spack-environments/phlex-ci

SHELL ["/bin/bash", "-c"]

########################################################################
# Install essential build tools, git, and Python

RUN <<'INSTALL_ESSENTIALS'
set -euo pipefail

# Install locale support required by tooling and Spack
apt-get update
apt-get upgrade -y --no-install-recommends
apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    curl \
    file \
    git \
    gnupg \
    locales-all \
    libcurl4-openssl-dev \
    libssl-dev \
    lsb-release \
    ninja-build \
    software-properties-common \
    sudo \
    unzip
apt autoremove -y
apt-get clean
rm -rf /var/lib/apt/lists/*
INSTALL_ESSENTIALS

########################################################################
# Clone Spack checkout used by both CI and dev images

RUN <<'CLONE_SPACK'
set -euo pipefail

# Clone Spack checkout used by both CI and dev images
git clone https://github.com/spack/spack.git /spack
CLONE_SPACK

########################################################################
# Prepare shared group and directories for Spack data/cache

RUN <<'PREP_SPACK_GROUP'
set -euo pipefail

# Prepare shared group and directories for Spack data/cache
groupadd --gid 2000 spack
mkdir -p /opt/spack-stage /opt/spack-cache/downloads /opt/spack-cache/misc /opt/spack-environments
chgrp -R spack /spack /opt/spack-stage /opt/spack-cache /opt/spack-environments
chmod -R g+rwX /spack /opt/spack-stage /opt/spack-cache /opt/spack-environments
find /spack /opt/spack-stage /opt/spack-cache /opt/spack-environments -type d -exec chmod g+s {} +
PREP_SPACK_GROUP

########################################################################
# Configure Spack: add repos, compilers, externals, and cache locations

RUN <<'SPACK_CONFIGURE_DEFAULTS'
set -euo pipefail

SPACK_REPO_ROOT=/opt/spack-repos
rm -rf "$SPACK_REPO_ROOT/fnal_art" "$SPACK_REPO_ROOT/phlex-spack-recipes" "$SPACK_REPO_ROOT/spack-packages"
mkdir -p "$SPACK_REPO_ROOT"
git clone --depth=1 https://github.com/FNALssi/fnal_art.git "$SPACK_REPO_ROOT/fnal_art"
git clone --depth=1 https://github.com/Framework-R-D/phlex-spack-recipes.git "$SPACK_REPO_ROOT/phlex-spack-recipes"
git clone https://github.com/spack/spack-packages.git "$SPACK_REPO_ROOT/spack-packages"
chgrp -R spack "$SPACK_REPO_ROOT"
chmod -R g+rwX "$SPACK_REPO_ROOT"
find "$SPACK_REPO_ROOT" -type d -exec chmod g+s {} +

. /spack/share/spack/setup-env.sh

spack --timestamp repo add --scope site $SPACK_REPO_ROOT/phlex-spack-recipes/spack_repo/phlex
spack --timestamp repo add --scope site $SPACK_REPO_ROOT/fnal_art/spack_repo/fnal_art
spack --timestamp repo add --scope site $SPACK_REPO_ROOT/spack-packages/repos/spack_repo/builtin
spack --timestamp compiler find

# Discover externals
#
# - Python is excluded because the python provided by the system does
#   not necessarily include the Python.h header file.  We therefore need
#   Spack to build Python.
spack --timestamp external find --exclude python

spack config --scope defaults add config:build_stage:/opt/spack-stage
spack config --scope defaults add config:source_cache:/opt/spack-cache/downloads
spack config --scope defaults add config:misc_cache:/opt/spack-cache/misc
spack config --scope defaults add "config:install_tree:padded_length:255"
SPACK_CONFIGURE_DEFAULTS

ENV GNUPGHOME=/run/gpg
ENV SPACK_GNUPGHOME=/run/gpg

########################################################################
# Configure binary mirrors used during installs

RUN <<'SPACK_CONFIGURE_MIRRORS'
set -euo pipefail

# Register binary caches for Spack installs and optional local cache volume.
# The local cache is added with --unsigned so that packages built in earlier
# layers (which may be unsigned) can be consumed without signature checks.
# buildcache keys installs the public verification keys advertised by all
# configured mirrors so that signed packages from those mirrors can be
# verified during subsequent installs.
. /spack/share/spack/setup-env.sh

spack mirror add --type binary phlex-ci-scisoft https://scisoft.fnal.gov/scisoft/spack-packages/phlex-dev

if [ -d "/build-cache" ]; then
  spack --timestamp mirror add --unsigned phlex-dev-local /build-cache
fi

spack buildcache keys -it
SPACK_CONFIGURE_MIRRORS

########################################################################
# Use Spack to install the GCC we want to use

RUN <<'SPACK_INSTALL_GCC'
set -euo pipefail

# Install GCC outside the Phlex development environment.
. /spack/share/spack/setup-env.sh

spack --timestamp install --fail-fast -j "$(nproc)" -p 1 \
      --no-check-signature \
      gcc@15.2 target=x86_64_v3 \
      +binutils+bootstrap+graphite~nvptx+piclibs+profiled+strip \
      build_type=Release \
      languages=c,c++,fortran,lto

if [ -d "/build-cache" ]; then
  key_id=${GPG_KEY_ID:-$({ gpg --list-secret-keys --with-colons | grep -Ee '^sec' | cut -d: -f 5 | head -n1; } 2>/dev/null || true)}
  if [ -n "$key_id" ]; then
    key_args=(--key "${key_id}")
  else
    key_args=(-u)
  fi
  spack --timestamp buildcache create ${key_args[*]:+"${key_args[@]}"} -j $(nproc) --update-index \
        /build-cache $(spack find --no-groups -LI | \
        sed -Ene 's&^\[\+\] +([^[:space:]]+).*$&/\1&p')
fi

# Make GCC external to simplify concretization
spack compiler find $(spack location -i gcc@15.2)

spack clean -dfs
SPACK_INSTALL_GCC

########################################################################
# Create, activate, and concretize the phlex-ci Spack environment

COPY spack.yaml /tmp/spack.yaml
RUN <<'SPACK_CONCRETIZE_ENV'
set -euo pipefail

# Build the phlex-ci environment lockfile with consistent permissions
. /spack/share/spack/setup-env.sh

mkdir -p "$(dirname "$PHLEX_SPACK_ENV")"
rm -rf "$PHLEX_SPACK_ENV"
spack --timestamp env create -d "$PHLEX_SPACK_ENV" /tmp/spack.yaml
spack --timestamp env activate -d "$PHLEX_SPACK_ENV"
spack --timestamp concretize
chgrp -R spack "$PHLEX_SPACK_ENV"
chmod -R g+rwX "$PHLEX_SPACK_ENV"
find "$PHLEX_SPACK_ENV" -type d -exec chmod g+s {} +
SPACK_CONCRETIZE_ENV

########################################################################
# Install common tooling layer (CMake, Ninja, coverage tools, etc.)

COPY entrypoint.sh /entrypoint.sh

RUN <<'INSTALL_TOOLING_CORE'
set -euo pipefail

# Install core build utilities through Spack for both images
. /entrypoint.sh

spack --timestamp install --fail-fast -j $(nproc) -p 1 \
  --no-add --only-concrete --no-check-signature \
  cmake lcov ninja py-gcovr

if [ -d "/build-cache" ]; then
  key_id=${GPG_KEY_ID:-$({ gpg --list-secret-keys --with-colons | grep -Ee '^sec' | cut -d: -f 5 | head -n1; } 2>/dev/null || true)}
  if [ -n "$key_id" ]; then
    key_args=(--key "${key_id}")
  else
    key_args=(-u)
  fi
  spack --timestamp buildcache create ${key_args[*]:+"${key_args[@]}"} -j $(nproc) --update-index \
        /build-cache $(spack find --no-groups -LI | \
        sed -Ene 's&^\[\+\] +([^[:space:]]+).*$&/\1&p')
fi

spack clean -dfs
INSTALL_TOOLING_CORE

########################################################################
# Install LLVM toolchain separately for caching efficiency

RUN <<'SPACK_INSTALL_LLVM'
set -euo pipefail

# Install LLVM in its own layer to improve cache reuse
. /entrypoint.sh

spack --timestamp install --fail-fast -j $(nproc) -p 1 \
  --no-add --only-concrete --no-check-signature llvm

if [ -d "/build-cache" ]; then
  key_id=${GPG_KEY_ID:-$({ gpg --list-secret-keys --with-colons | grep -Ee '^sec' | cut -d: -f 5 | head -n1; } 2>/dev/null || true)}
  if [ -n "$key_id" ]; then
    key_args=(--key "${key_id}")
  else
    key_args=(-u)
  fi
  spack --timestamp buildcache create ${key_args[*]:+"${key_args[@]}"} -j $(nproc) --update-index \
        /build-cache $(spack find --no-groups -LI | \
        sed -Ene 's&^\[\+\] +([^[:space:]]+).*$&/\1&p')
fi

spack clean -dfs
SPACK_INSTALL_LLVM

########################################################################
# Install remaining tools (excluding phlex itself) in one layer

RUN <<'SPACK_INSTALL_REMAINING_PACKAGES'
set -euo pipefail

# Install all concretized packages except phlex to share cache
. /entrypoint.sh

spack --timestamp install --fail-fast -j $(nproc) -p 1 \
  --no-add --only-concrete --no-check-signature \
  $(spack find -r --no-groups | sed -Ene 's&^ - &&p' | grep -v phlex)

if [ -d "/build-cache" ]; then
  key_id=${GPG_KEY_ID:-$({ gpg --list-secret-keys --with-colons | grep -Ee '^sec' | cut -d: -f 5 | head -n1; } 2>/dev/null || true)}
  if [ -n "$key_id" ]; then
    key_args=(--key "${key_id}")
  else
    key_args=(-u)
  fi
  spack --timestamp buildcache create ${key_args[*]:+"${key_args[@]}"} -j $(nproc) --update-index \
        /build-cache $(spack find --no-groups -LI | \
        sed -Ene 's&^\[\+\] +([^[:space:]]+).*$&/\1&p')
fi

spack clean -dfs
SPACK_INSTALL_REMAINING_PACKAGES

########################################################################
# Install Phlex dependencies (but not Phlex itself) for CI builds

RUN <<'SPACK_INSTALL_PHLEX_DEPENDENCIES'
set -euo pipefail

# Materialize dependencies needed for CI builds without installing Phlex
. /entrypoint.sh

spack --timestamp install --fail-fast -j $(nproc) -p 1 \
  --no-add --only-concrete --only dependencies --no-check-signature \
  phlex

spack clean -dfs
SPACK_INSTALL_PHLEX_DEPENDENCIES

########################################################################
# Verify Python was built with SSL support

RUN <<'VERIFY_PYTHON_SSL'
set -euo pipefail

# Confirm that the Spack-built Python provides a working ssl module,
# which requires that Python was compiled against the expected SSL library.
. /entrypoint.sh

python -c "
import ssl
ctx = ssl.create_default_context()
print('Python SSL OK:', ssl.OPENSSL_VERSION)
" || {
  echo "ERROR: Spack Python does not provide a working ssl module." >&2
  echo "Check that Python was built with OpenSSL support (openssl~shared is a common culprit)." >&2
  exit 1
}
VERIFY_PYTHON_SSL

########################################################################
# Publish buildcache artifacts if a cache volume is mounted

RUN <<'WRITE_BUILDCACHE'
set -euo pipefail

# Export buildcache entries when /build-cache is provided as a volume.
# Signs packages using the key identified by GPG_KEY_ID or the first
# private key found in GNUPGHOME (/run/gpg); writes unsigned if neither
# is available.
. /entrypoint.sh

if [ -d "/build-cache" ]; then
  key_id=${GPG_KEY_ID:-$({ gpg --list-secret-keys --with-colons | grep -Ee '^sec' | cut -d: -f 5 | head -n1; } 2>/dev/null || true)}
  if [ -n "$key_id" ]; then
    key_args=(--key "${key_id}")
  else
    key_args=(-u)
  fi
  spack --timestamp buildcache create ${key_args[*]:+"${key_args[@]}"} -j $(nproc) --update-index \
        /build-cache $(spack find --no-groups -LI | \
        sed -Ene 's&^\[\+\] +([^[:space:]]+).*$&/\1&p')
fi

spack mirror rm phlex-dev-local >/dev/null 2>&1 || true

spack clean -dfs

# Drop the temporary Spack environment descriptor
rm -f /tmp/spack.yaml

WRITE_BUILDCACHE

########################################################################
# Verify Python was built with SSL support

RUN <<'VERIFY_PYTHON_SSL'
set -euo pipefail

# Confirm that the Spack-built Python provides a working ssl module,
# which requires that Python was compiled against the expected SSL library.
. /entrypoint.sh

python -c "
import ssl
ctx = ssl.create_default_context()
print('Python SSL OK:', ssl.OPENSSL_VERSION)
" || {
  echo "ERROR: Spack Python does not provide a working ssl module." >&2
  echo "Check that Python was built with OpenSSL support (openssl~shared is a common culprit)." >&2
  exit 1
}
VERIFY_PYTHON_SSL

########################################################################
# Install Perfetto SDK for profiling

RUN <<'INSTALL_PERFETTO'
set -euo pipefail

# Install Perfetto SDK and tools
apt-get update
apt-get install -y --no-install-recommends \
  wget
apt-get clean
rm -rf /var/lib/apt/lists/*

mkdir -p /opt/perfetto
cd /opt/perfetto
PERFETTO_VERSION=v51.0
wget -O perfetto.h https://raw.githubusercontent.com/google/perfetto/${PERFETTO_VERSION}/sdk/perfetto.h
wget -O perfetto.cc https://raw.githubusercontent.com/google/perfetto/${PERFETTO_VERSION}/sdk/perfetto.cc
chmod 644 perfetto.h perfetto.cc

# Install tracebox for system-wide profiling.
# Note: get.perfetto.dev/tracebox serves the latest binary with no versioned
# or checksum-verifiable download currently offered by the Perfetto project.
# Revisit when the project provides versioned tracebox releases with integrity
# metadata.
wget -O tracebox https://get.perfetto.dev/tracebox
chmod +x tracebox
mv tracebox /usr/local/bin/
INSTALL_PERFETTO

########################################################################
# Finalize CI image stage

FROM base AS ci
WORKDIR /root
USER root

########################################################################
# Dev image additions (tooling and dev user configuration)

FROM base AS dev
ENV SPACK_GROUP=spack

########################################################################
# Install developer tooling (gersemi, prek, ruff)

RUN <<'INSTALL_DEV_TOOLING'
set -euo pipefail

# Install developer tooling not needed for CI
. /entrypoint.sh

# Install uv for Python tooling management
curl -LsSf https://astral.sh/uv/install.sh | \
  env INSTALLER_NO_MODIFY_PATH=1 UV_INSTALL_DIR=/usr/local/bin sh

# Install ruff, gersemi, and prek as isolated tools via uv
export UV_TOOL_BIN_DIR=/usr/local/bin
export UV_TOOL_DIR=/usr/local/share/uv/tools
uv tool install ruff
uv tool install gersemi
uv tool install prek

uv cache clean
rm -rf ~/.spack
INSTALL_DEV_TOOLING

########################################################################
# Prepare workspace directory writable by the developer group

RUN <<'PREP_DEV_WORKSPACE'
set -euo pipefail

# Prepare workspace directory writable by the developer group
mkdir -p /workspaces
chgrp $SPACK_GROUP /workspaces
chmod g+rwX /workspaces
PREP_DEV_WORKSPACE

########################################################################
# Install GitHub's act CLI for local workflow testing

RUN <<'INSTALL_ACT'
set -euo pipefail

# Install GitHub's act CLI for local workflow testing
download_url=$(curl -s https://api.github.com/repos/nektos/act/releases/latest | \
  grep -Ee '"browser_download_url": .*/act_Linux_x86_64\.' | \
  cut -d '"' -f 4)
if [ -z "$download_url" ]; then
  echo "Failed to determine act download URL" >&2
  exit 1
fi
curl -sfSL "$download_url" | tar -C /usr/local/bin -zx act
chmod +x /usr/local/bin/act
INSTALL_ACT

########################################################################
# Install GitHub CLI (gh)

RUN <<'INSTALL_GH_CLI'
set -euo pipefail

# Install GitHub CLI (gh)
curl -fsSL https://cli.github.com/packages/githubcli-archive-keyring.gpg | \
  dd of=/usr/share/keyrings/githubcli-archive-keyring.gpg
chmod go+r /usr/share/keyrings/githubcli-archive-keyring.gpg
echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/githubcli-archive-keyring.gpg] https://cli.github.com/packages stable main" \
  > /etc/apt/sources.list.d/github-cli.list
apt-get update
apt-get install -y --no-install-recommends gh
apt-get clean
rm -rf /var/lib/apt/lists/*
INSTALL_GH_CLI

SHELL ["/bin/bash", "-c"]
HEALTHCHECK CMD curl -f https://github.com || exit 1
