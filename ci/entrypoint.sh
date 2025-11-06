#!/bin/bash

# Disable Spack's user scope to prevent user-level config interference
export SPACK_USER_CONFIG_PATH=/dev/null
export SPACK_DISABLE_LOCAL_CONFIG=true

. /spack/share/spack/setup-env.sh
spack env activate phlex-ci
