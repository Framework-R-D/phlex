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

# Create and prepare phlex-development environment
spack env activate --create PHLEXDEV

# Special handling for libunwind, root, and unuran
# - GCC 15 uses C23 as the default C language, and the versions of
#   libunwind and unuran available in Spack (as of 8/12/2025) do not
#   yet support C23.
# - ROOT should be built with the same version of the C++ standard as
#   will be used by Phlex (C++20 for now).
spack add phlex+form cxxstd=20 %gcc@15 \
    ^libunwind cflags='-std=c17' \
    ^root cxxstd=20 \
    ^unuran cflags='-std=c17'
spack add cmake
spack add catch2

# Display concretized solution
spack concretize

# Install almost everything
spack install --fail-fast $parallelism \
  boost \
  catch2 \
  cmake \
  fmt \
  intel-tbb \
  jsonnet \
  root \
  spdlog
EOF

COPY ./entrypoint.sh /
RUN chmod +x /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
