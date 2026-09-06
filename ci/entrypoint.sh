#!/bin/bash

# Disable Spack's user scope to prevent user-level config interference
export SPACK_USER_CONFIG_PATH=/dev/null
export SPACK_DISABLE_LOCAL_CONFIG=true

if [ -f /etc/profile.d/phlex-targets.sh ]; then
  . /etc/profile.d/phlex-targets.sh
fi

: "${PHLEX_SPACK_ENV:=/opt/spack-environments/phlex-ci}"
# PHLEX_SPACK_TARGET must be normalized to x86_64_v3 or aarch64 (Dockerfile handles this)
# Default is x86_64_v3 for backward compatibility
: "${PHLEX_SPACK_TARGET:=x86_64_v3}"
# CI images use GCC as default; developer images use Clang with GCC 15 toolchain for reproducible gcc@15 ABI
: "${PHLEX_DEFAULT_COMPILER:=gcc}"

# Validate PHLEX_SPACK_TARGET is normalized (x86_64_v3 or aarch64)
# Dockerfile normalizes amd64->x86_64_v3 and arm64->aarch64 before this runs
case "$PHLEX_SPACK_TARGET" in
  x86_64_v3)
    : "${PHLEX_LLVM_TARGET:=x86}"
    ;;
  aarch64)
    : "${PHLEX_LLVM_TARGET:=AArch64}"
    ;;
  *)
    echo "ERROR: PHLEX_SPACK_TARGET must be 'x86_64_v3' or 'aarch64'" >&2
    echo "       Got: '$PHLEX_SPACK_TARGET'" >&2
    exit 1
    ;;
esac

# Validate that SPACK target and LLVM target are compatible (no x86_64_v3 + AArch64, no aarch64 + x86)
case "$PHLEX_SPACK_TARGET-$PHLEX_LLVM_TARGET" in
  x86_64_v3-AArch64|aarch64-x86)
    echo "ERROR: Host/target mismatch: PHLEX_SPACK_TARGET=$PHLEX_SPACK_TARGET with PHLEX_LLVM_TARGET=$PHLEX_LLVM_TARGET" >&2
    exit 1
    ;;
  *)
    # Valid combinations: x86_64_v3+x86, aarch64+AArch64
    ;;
esac

. /spack/share/spack/setup-env.sh
spack env activate -d "$PHLEX_SPACK_ENV"

# GCC 15 is required for Phlex and its direct dependencies.
# The Spack view's 'bin' directory contains symlinks to the active compiler.
# When PHLEX_DEFAULT_COMPILER is "gcc", use GCC 15 directly via PATH.
# When PHLEX_DEFAULT_COMPILER is "clang", use Clang with reproducible --gcc-toolchain binding.
if [ "$PHLEX_DEFAULT_COMPILER" = "gcc" ]; then
  # Developer can override CC/CXX via environment; CI uses explicit --target mapping
  # GCC 15 must be used for phlex, root, and their dependencies to share ABI.
  # The spack env activate makes gcc@15 available from the Spack view.
  PATH=$(spack -E location -i gcc@15 %c,cxx=gcc@13)/bin:$PATH
  export CC=gcc CXX=g++
else
  # clang/clang++ with reproducible --gcc-toolchain binding to GCC 15
  # This preserves the GCC 15 ABI while allowing compilation via Clang.
  # The --gcc-toolchain option ensures Clang uses GCC 15's libstdc++ and headers.
  gcc_path=$(spack location -i gcc@15 %c,cxx=gcc@13)
  export CC=clang CXX=clang++
  export CFLAGS="--gcc-toolchain=$gcc_path"
  export CXXFLAGS="--gcc-toolchain=$gcc_path"
fi
