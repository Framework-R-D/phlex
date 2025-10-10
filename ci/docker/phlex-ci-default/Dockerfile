# Dockerfile for creating phlex-ci image, which can be used for GitHub actions.
#
# Podman instructions for building an image with this file:
#
#   $ cd <directory with this file>
#   $ podman build --tag phlex-ci:<tag> .
#   $ podman login ghcr.io --username <username> -p <token>
#   $ podman push phlex-ci:<tag> ghcr.io/framework-r-d/phlex-ci:<tag>
#
# where <tag> is the date (e.g. "2025-08-12").

FROM gcc:15.2.0
ARG parallelism=18

RUN git clone --depth=2 https://github.com/spack/spack.git
RUN <<EOF
# Setup environment
. spack/share/spack/setup-env.sh

# Add package repositories
spack repo add https://github.com/FNALssi/fnal_art.git
spack repo add https://github.com/Framework-R-D/phlex-spack-recipes.git

# Discover compilers
spack compiler find

# Discover externals
# - Python is excluded because the python provided by the system does
#   not necessarily include the Python.h header file.  We therefore
#   need Spack to build Python.
spack external find --exclude python
EOF

COPY ./spack.yaml /

RUN <<EOF
# Setup environment
. spack/share/spack/setup-env.sh

# Create and prepare phlex-development environment
spack env create phlex-ci spack.yaml
spack env activate phlex-ci

# Display concretized solution
spack concretize

# Install almost everything
spack install --fail-fast -j $parallelism -p 1 $(spack find -r --no-groups | sed -Ene 's&^ - &&p' | grep -v phlex)
EOF

COPY ./entrypoint.sh /
RUN chmod +x /entrypoint.sh
