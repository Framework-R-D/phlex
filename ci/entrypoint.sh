#!/bin/bash
# vi: set ft=sh b:is_bash=1 fenc=utf-8 :
# -*- Local Variables:
# -*- mode: shell-script
# -*- coding: utf-8
# -*- sh-shell: bash
# -*- End:

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
# CI images default to GCC; developer images use Clang with the same GCC runtime.
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

case "$PHLEX_DEFAULT_COMPILER" in
  gcc|clang) ;;
  *)
    echo "ERROR: PHLEX_DEFAULT_COMPILER must be 'gcc' or 'clang'" >&2
    echo "       Got: '$PHLEX_DEFAULT_COMPILER'" >&2
    exit 1
    ;;
esac

. /spack/share/spack/setup-env.sh
spack env activate -d "$PHLEX_SPACK_ENV"

# Resolve the bootstrapped compiler outside the active environment, where it
# may appear only as an external. Prefer the image's recorded versions; older
# images without a carrier must have a unique matching installation.
gcc_path=$(spack -E location -i \
  "gcc${PHLEX_GCC_VERSION:+@${PHLEX_GCC_VERSION}}" \
  "%c,cxx=gcc${PHLEX_NATIVE_GCC_VERSION:+@${PHLEX_NATIVE_GCC_VERSION}}") || return 1

if [ "$PHLEX_DEFAULT_COMPILER" = "gcc" ]; then
  PATH="$gcc_path/bin:$PATH"
  export CC=gcc CXX=g++
else
  # Spack's Clang config sets --gcc-install-dir, which makes --gcc-toolchain
  # unused (and fatal under -Werror). Bind the exact installation instead.
  gcc_libgcc=$("$gcc_path/bin/gcc" -print-libgcc-file-name) || return 1
  export CC=clang CXX=clang++
  export CFLAGS="--gcc-install-dir=${gcc_libgcc%/*}"
  export CXXFLAGS="--gcc-install-dir=${gcc_libgcc%/*}"
fi
