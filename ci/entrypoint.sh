#!/bin/bash

# Disable Spack's user scope to prevent user-level config interference
export SPACK_USER_CONFIG_PATH=/dev/null
export SPACK_DISABLE_LOCAL_CONFIG=true

: "${PHLEX_SPACK_ENV:=/opt/spack-environments/phlex-ci}"
: "${PHLEX_SPACK_TARGET:=amd64}"
: "${PHLEX_DEFAULT_COMPILER:=gcc}"

# Map PHLEX_SPACK_TARGET to Spack's internal target names
# amd64 -> x86_64_v3, arm64 -> aarch64
case "$PHLEX_SPACK_TARGET" in
  amd64)
    export PHLEX_SPACK_TARGET="x86_64_v3"
    export PHLEX_LLVM_TARGET="x86"
    ;;
  arm64)
    export PHLEX_SPACK_TARGET="aarch64"
    export PHLEX_LLVM_TARGET="AArch64"
    ;;
  *)
    # If already a Spack target name, use as-is; otherwise fail
    case "$PHLEX_SPACK_TARGET" in
      x86_64_v3|aarch64|arm64|amd64) ;;
      *)
        echo "ERROR: PHLEX_SPACK_TARGET must be 'amd64', 'arm64', or a valid Spack target name" >&2
        echo "       Got: '$PHLEX_SPACK_TARGET'" >&2
        exit 1
        ;;
    esac
    ;;
esac

# Set PHLEX_LLVM_TARGET if not already set
: "${PHLEX_LLVM_TARGET:=x86}"

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
