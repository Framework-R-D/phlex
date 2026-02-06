# syntax=docker/dockerfile:1.4

# Multi-target Dockerfile for building the Phlex CI and development images.

# Podman instructions for building tagged images with this file:
#
#   $ podman build --format docker [-v ~/.gnupg:/run/gpg] [-v <local-spack-cache>:/build-cache] --target ci   --tag phlex-ci:<tag>  .
#   $ podman build --format docker [-v ~/.gnupg:/run/gpg] [-v <local-spack-cache>:/build-cache] --target dev  --tag phlex-dev:<tag> .
#   $ podman login ghcr.io --username <username> -p <token>
#   $ podman push phlex-ci:<tag>  ghcr.io/framework-r-d/phlex-ci:<tag>
#   $ podman push phlex-dev:<tag> ghcr.io/framework-r-d/phlex-dev:<tag>
# ... and optionally push either image as "latest".

# where <tag> is the date (e.g. "2025-08-12").

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
    cmake \
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
    sudo
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
cd /spack
git checkout 0d99be382e592d6f4f2f5e314610585cbf0a716a
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
rm -rf "$SPACK_REPO_ROOT/fnal_art" "$SPACK_REPO_ROOT/phlex-spack-recipes"
mkdir -p "$SPACK_REPO_ROOT"
git clone --depth=1 https://github.com/FNALssi/fnal_art.git "$SPACK_REPO_ROOT/fnal_art"
git clone --depth=1 https://github.com/Framework-R-D/phlex-spack-recipes.git "$SPACK_REPO_ROOT/phlex-spack-recipes"
chgrp -R spack "$SPACK_REPO_ROOT"
chmod -R g+rwX "$SPACK_REPO_ROOT"
find "$SPACK_REPO_ROOT" -type d -exec chmod g+s {} +

. /spack/share/spack/setup-env.sh

spack --timestamp repo add --scope site $SPACK_REPO_ROOT/phlex-spack-recipes/spack_repo/phlex
spack --timestamp repo add --scope site $SPACK_REPO_ROOT/fnal_art/spack_repo/fnal_art
spack --timestamp repo set --scope site --destination $SPACK_REPO_ROOT builtin
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

# Register binary caches for Spack installs and optional local cache volume
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

# We must use the system compiler to build GCC 15, and we want to
# ensure that GCC 15's dependencies are also built with the system
# compiler to avoid a concretization loop where GCC 15 would be
# required to build itself or its dependencies.
system_compiler=$( { spack config get compilers | grep -oEe 'gcc@[0-9.]+' | head -n1; } || true)
spack install --fail-fast -j "$(nproc)" \
      gcc @15 +binutils+bootstrap+graphite~nvptx+piclibs+profiled+strip \
      build_type=Release \
      languages=c,c++,fortran,lto \
      %"${system_compiler:-gcc}" target=x86_64_v3

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
spack versions -s llvm | grep -qEe '^[[:space:]]+21\.1\.4$' || spack checksum -ab llvm 21.1.4
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
  cmake lcov ninja py-gcovr py-pip

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

SPACK_INSTALL_PHLEX_DEPENDENCIES

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
# Install developer tooling (gersemi, ruff)

RUN <<'INSTALL_DEV_TOOLING'
set -euo pipefail

# Install developer tooling not needed for CI
. /entrypoint.sh

# Install ruff, gersemi, and jsonnetfmt via pip
PYTHONDONTWRITEBYTECODE=1 \
  pip --isolated --no-input --disable-pip-version-check --no-cache-dir install \
      ruff gersemi jsonnet
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
