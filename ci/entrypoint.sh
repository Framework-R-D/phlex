#!/bin/bash

# Disable Spack's user scope to prevent user-level config interference
export SPACK_USER_CONFIG_PATH=/dev/null
export SPACK_DISABLE_LOCAL_CONFIG=true

: "${PHLEX_SPACK_ENV:=/opt/spack-environments/phlex-ci}"

. /spack/share/spack/setup-env.sh
spack env activate -d "$PHLEX_SPACK_ENV"
PATH=$(spack -E location -i gcc@15 %c,cxx=gcc@13)/bin:$PATH
