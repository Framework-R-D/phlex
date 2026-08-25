#!/bin/bash

# Disable Spack's user scope to prevent user-level config interference
export SPACK_USER_CONFIG_PATH=/dev/null
export SPACK_DISABLE_LOCAL_CONFIG=true

: "${PHLEX_SPACK_ENV:=/opt/spack-environments/phlex-ci}"

# Detect architecture and set PHLEX_SPACK_TARGET and PHLEX_SPACK_COMPILER
# PHLEX_DEFAULT_COMPILER controls the default compiler for the image type:
#   - CI images (phlex-ci): PHLEX_DEFAULT_COMPILER=gcc (default if not set)
#   - Dev images (phlex-dev): PHLEX_DEFAULT_COMPILER=clang (default if not set)
# PHLEX_SPACK_COMPILER can override PHLEX_DEFAULT_COMPILER
#
# Defaults:
#   - amd64 -> x86_64_v3, gcc@15 (via PHLEX_DEFAULT_COMPILER=gcc)
#   - arm64 -> aarch64, clang (via PHLEX_DEFAULT_COMPILER=clang)

: "${PHLEX_SPACK_TARGET:-}"
: "${PHLEX_SPACK_COMPILER:-}"
: "${PHLEX_DEFAULT_COMPILER:-}"

ARCH=$(uname -m)

case "$ARCH" in
  x86_64)
    # amd64 architecture
    : "${PHLEX_SPACK_TARGET:=x86_64_v3}"
    : "${PHLEX_DEFAULT_COMPILER:=gcc}"
    ;;
  aarch64|arm64)
    # arm64 architecture
    : "${PHLEX_SPACK_TARGET:=aarch64}"
    : "${PHLEX_DEFAULT_COMPILER:=clang}"
    ;;
  *)
    echo "ERROR: Unsupported architecture: $ARCH" >&2
    echo "Supported architectures: x86_64, aarch64/arm64" >&2
    echo "Set PHLEX_SPACK_TARGET to override target architecture" >&2
    exit 1
    ;;
esac

# Verify architecture matches target
case "$PHLEX_SPACK_TARGET" in
  x86_64_v3)
    if [[ "$ARCH" != "x86_64" ]]; then
      echo "ERROR: PHLEX_SPACK_TARGET=x86_64_v3 requires x86_64 architecture, found: $ARCH" >&2
      exit 1
    fi
    ;;
  aarch64)
    if [[ "$ARCH" != "aarch64" && "$ARCH" != "arm64" ]]; then
      echo "ERROR: PHLEX_SPACK_TARGET=aarch64 requires aarch64/arm64 architecture, found: $ARCH" >&2
      exit 1
    fi
    ;;
  *)
    echo "ERROR: Unknown PHLEX_SPACK_TARGET: $PHLEX_SPACK_TARGET" >&2
    echo "Valid targets: x86_64_v3, aarch64" >&2
    exit 1
    ;;
esac

# Set PHLEX_SPACK_COMPILER from PHLEX_DEFAULT_COMPILER if not explicitly set
# This allows the developer image to default to Clang while CI defaults to GCC
: "${PHLEX_SPACK_COMPILER:=$PHLEX_DEFAULT_COMPILER}"

# Set compiler selection based on PHLEX_SPACK_COMPILER
case "$PHLEX_SPACK_COMPILER" in
  gcc)
    export CC=gcc
    export CXX=g++
    ;;
  clang|apple-clang)
    export CC=clang
    export CXX=clang++
    ;;
  *)
    echo "ERROR: Unknown PHLEX_SPACK_COMPILER: $PHLEX_SPACK_COMPILER" >&2
    echo "Valid compilers: gcc, clang, apple-clang" >&2
    exit 1
    ;;
esac

# Export architecture variables for use in spack.yaml
export PHLEX_SPACK_TARGET
export PHLEX_SPACK_COMPILER
export PHLEX_DEFAULT_COMPILER

. /spack/share/spack/setup-env.sh
spack env activate -d "$PHLEX_SPACK_ENV"

# Apply architecture-specific config overrides before concretization
# Order matters: apply target first, then compiler
if [ -n "${PHLEX_SPACK_TARGET:-}" ]; then
  spack config add "concretizer:targets:arch:${PHLEX_SPACK_TARGET}" 2>/dev/null || true
fi

if [ -n "${PHLEX_SPACK_COMPILER:-}" ]; then
  spack config add "concretizer:compiler:name:${PHLEX_SPACK_COMPILER}" 2>/dev/null || true
fi

# Set PATH to include the compiler (for gcc@15, use the Spack-built version)
if [ "${PHLEX_SPACK_COMPILER:-gcc}" = "gcc" ]; then
  PATH=$(spack -E location -i gcc@15 %c,cxx=gcc@13)/bin:$PATH
fi
