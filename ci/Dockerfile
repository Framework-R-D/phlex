# Dockerfile for creating phlex-ci image, which can be used for GitHub actions.
#
# Podman instructions for building an image with this file:
#
#   $ cd <directory with this file>
#   $ podman build --tag phlex-ci:<tag> [-v <local-buildcache>:/build-cache:U] .
#   $ podman login ghcr.io --username <username> -p <token>
#   $ podman push phlex-ci:<tag> ghcr.io/framework-r-d/phlex-ci:<tag>
# ... and optionally:
#   $ podman push phlex-ci:<tag> ghcr.io/framework-r-d/phlex-ci:latest
#
# where <tag> is the date (e.g. "2025-08-12").

FROM gcc:15.2.0
ARG parallelism=18


########################################################################
# Improve compatibility with VSCode dev container

RUN <<EOF
  # Create vscode user with passwordless sudo

  set -euo pipefail
  apt-get update
  apt-get install -y sudo locales-all
  apt-get clean
  rm -rf /var/lib/apt/lists/*
  useradd -m vscode
  echo "vscode ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/vscode
  chmod 0440 /etc/sudoers.d/vscode
EOF

ENV LANG=en_US.UTF-8
ENV LC_ALL=en_US.UTF-8
########################################################################

RUN git clone --depth=2 https://github.com/spack/spack.git && chown -R vscode:vscode /spack

USER vscode
WORKDIR /home/vscode

# Disable Spack's user scope to prevent user-level config interference
ENV SPACK_USER_CONFIG_PATH=/dev/null
ENV SPACK_DISABLE_LOCAL_CONFIG=true

RUN <<EOF
  # Set up and configure basic Spack installation

  # Setup environment
  . /spack/share/spack/setup-env.sh

  set -euo pipefail

  # Add package repositories
  spack --timestamp repo add https://github.com/FNALssi/fnal_art.git
  spack --timestamp repo add https://github.com/Framework-R-D/phlex-spack-recipes.git

  # Discover compilers
  spack --timestamp compiler find

  # Discover externals
  # - Python is excluded because the python provided by the system does
  #   not necessarily include the Python.h header file.  We therefore
  #   need Spack to build Python.
  spack --timestamp external find --exclude python
EOF

RUN <<EOF
  # Configure build caches

  . /spack/share/spack/setup-env.sh

  set -euo pipefail

  # Add public mirror
  spack mirror add --type binary phlex-ci-scisoft https://scisoft.fnal.gov/scisoft/phlex-dev-build-cache

  # Add private mirror with higher priority if available
  if [ -d "/build-cache" ]; then
    spack --timestamp mirror add --type binary \
      phlex-ci-local /build-cache
  fi
EOF

COPY ./spack.yaml /home/vscode/spack.yaml

RUN <<EOF
  # Create and prepare phlex-development environment

  . /spack/share/spack/setup-env.sh

  set -euo pipefail

  spack versions -s llvm | grep -qEe '^[[:space:]]+21\.1\.4$' || spack checksum -ab llvm 21.1.4
  spack --timestamp env create phlex-ci spack.yaml
  spack --timestamp env activate phlex-ci

  # Display concretized solution
  spack --timestamp concretize
EOF

USER root
COPY ./entrypoint.sh /

USER vscode
RUN <<EOF
  # Install most of the tools as their own layer

  . /entrypoint.sh

  set -euo pipefail

  spack --timestamp install --fail-fast -j $parallelism -p 1 \
    --no-add --only-concrete --no-check-signature \
    cmake lcov ninja py-cmake-format py-gcovr py-pip
  # Clean up to reduce layer size
  spack clean -dfs
EOF

RUN <<EOF
  # Install llvm as its own layer

  . /entrypoint.sh

  set -euo pipefail

  spack --timestamp install --fail-fast -j $parallelism -p 1 \
    --no-add --only-concrete --no-check-signature llvm
  # Clean up to reduce layer size
  spack clean -dfs
EOF

RUN <<EOF
  # Install almost everything else

  . /entrypoint.sh

  set -euo pipefail

  spack --timestamp install --fail-fast -j $parallelism -p 1 \
    --no-add --only-concrete --no-check-signature \
    $(spack find -r --no-groups | sed -Ene 's&^ - &&p' | grep -v phlex)
  # Clean up to reduce layer size
  spack clean -dfs
EOF

RUN <<EOF
  # Install Phlex' dependencies

  . /entrypoint.sh

  set -euo pipefail

  spack --timestamp install --fail-fast -j $parallelism -p 1 \
    --no-add --only-concrete --only dependencies --no-check-signature \
    phlex
  # Clean up to reduce layer size
  spack clean -dfs
EOF

RUN <<EOF
  # Write a buildcache if mounted

  . /entrypoint.sh # Outside the if to reduce container startup time

  set -euo pipefail

  if [ -d "/build-cache" ]; then
    spack --timestamp buildcache create -u --rebuild-index \
    /build-cache $(spack find --no-groups -LI | \
      sed -Ene 's&^\[\+\] +([^[:space:]]+).*$&/\1&p')
    spack mirror rm phlex-ci-local >/dev/null 2>&1 || true
  fi
EOF

USER root
RUN <<EOF
  # Install ruff

   . /entrypoint.sh

  set -euo pipefail

  PYTHONDONTWRITEBYTECODE=1 pip --isolated --no-input --disable-pip-version-check --no-cache-dir install --prefix /usr/local ruff
  rm -rf ~/.spack
EOF

USER vscode
