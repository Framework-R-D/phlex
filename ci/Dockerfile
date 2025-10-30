# Dockerfile for creating phlex-ci image, which can be used for GitHub actions.
#
# Podman instructions for building an image with this file:
#
#   $ cd <directory with this file>
#   $ podman build --tag phlex-ci:<tag> [-v <local-buildcache>:/build-cache] .
#   $ podman login ghcr.io --username <username> -p <token>
#   $ podman push phlex-ci:<tag> ghcr.io/framework-r-d/phlex-ci:<tag>
# ... and optionally:
#   $ podman push phlex-ci:<tag> ghcr.io/framework-r-d/phlex-ci:latest
#
# where <tag> is the date (e.g. "2025-08-12").

FROM gcc:15.2.0
ARG parallelism=18

RUN git clone --depth=2 https://github.com/spack/spack.git

RUN <<EOF
  # Set up and configure basic Spack installation

  # Setup environment
  . spack/share/spack/setup-env.sh

  # Add package repositories
  spack --timestamp repo add https://github.com/FNALssi/fnal_art.git
  spack --timestamp repo add https://github.com/Framework-R-D/phlex-spack-recipes.git

  # Discover compilers
  spack --timestamp compiler find --scope=site

  # Discover externals
  # - Python is excluded because the python provided by the system does
  #   not necessarily include the Python.h header file.  We therefore
  #   need Spack to build Python.
  spack --timestamp external find --exclude python --scope=site
EOF

COPY ./phlex-deps-cetmodules.patch /

RUN <<EOF
  # Patch the Phlex recipe if necessary
  . spack/share/spack/setup-env.sh
  spack cd -P phlex && git apply /phlex-deps-cetmodules.patch || true
  rm -f /phlex-deps-cetmodules.patch
EOF

RUN <<EOF
  # Configure a remote build cache
  . spack/share/spack/setup-env.sh
  spack --timestamp mirror add --scope=site --type binary \
    phlex_dev_scisoft https://scisoft.fnal.gov/scisoft/phlex-dev-build-cache
EOF

RUN <<EOF
  # Configure a local buildcache if mounted at build-time
  if [ -d "/build-cache" ]; then
    . spack/share/spack/setup-env.sh
    spack --timestamp mirror add --scope=site --type binary \
      phlex_dev_local /build-cache
  fi
EOF

COPY ./spack.yaml /

RUN <<EOF
  # Create and prepare phlex-development environment

  . spack/share/spack/setup-env.sh
  spack versions -s llvm | grep -qEe '^[[:space:]]+21\.1\.4$' ||
    spack checksum -ab llvm 21.1.4
  spack --timestamp env create phlex-ci spack.yaml
  spack --timestamp env activate phlex-ci

  # Display concretized solution
  spack --timestamp concretize
EOF

COPY ./entrypoint.sh /

RUN chmod +x /entrypoint.sh

RUN <<EOF
  # Install most of the tools as their own layer
  . /entrypoint.sh
  spack --timestamp install --fail-fast -j $parallelism -p 1 \
    --no-add --only-concrete --no-check-signature \
    cmake lcov ninja py-cmake-format py-gcovr
  # Clean up to reduce layer size
  spack clean -dfs
EOF

RUN <<EOF
  # Install llvm as its own layer
  . /entrypoint.sh
  spack --timestamp install --fail-fast -j $parallelism -p 1 \
    --no-add --only-concrete --no-check-signature llvm
  # Clean up to reduce layer size
  spack clean -dfs
EOF

RUN <<EOF
  # Install almost everything else
  . /entrypoint.sh
  spack --timestamp install --fail-fast -j $parallelism -p 1 \
    --no-add --only-concrete --no-check-signature \
    $(spack find -r --no-groups | sed -Ene 's&^ - &&p' | grep -v phlex)
  # Clean up to reduce layer size
  spack clean -dfs
EOF

RUN <<EOF
  # Install Phlex' dependencies
  . /entrypoint.sh
  spack --timestamp install --fail-fast -j $parallelism -p 1 \
    --no-add --only-concrete --only dependencies --no-check-signature \
    phlex
  # Clean up to reduce layer size
  spack clean -dfs
EOF

RUN <<EOF
  # Write a buildcache if mounted
  . /entrypoint.sh # Outside the if to reduce container startup time
  if [ -d "/build-cache" ]; then
    spack --timestamp buildcache create -u --rebuild-index \
    /build-cache $(spack find --no-groups -LI | \
      sed -Ene 's&^\[\+\] +([^[:space:]]+).*$&/\1&p')
    spack mirror rm --scope=site phlex_dev_local
  fi
EOF
