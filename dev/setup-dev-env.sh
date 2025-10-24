#!/bin/bash -x

# This script sets up the development environment for the Phlex project.
# It is based on the contents of the ci/Dockerfile.

set -e

# Clone Spack if it's not already present
if [ ! -d "spack" ]; then
    git clone --depth=2 https://github.com/spack/spack.git
fi

parallelism=$(nproc)
script_dir="$(dirname "${BASH_SOURCE[0]}")"

# Set up and configure basic Spack installation
. spack/share/spack/setup-env.sh

# Add package repositories
spack --timestamp repo add \
      https://github.com/FNALssi/fnal_art.git || true
spack --timestamp repo add \
      https://github.com/Framework-R-D/phlex-spack-recipes.git || true

# Discover externals
# - Python is excluded because the python provided by the system does
#   not necessarily include the Python.h header file.  We therefore
#   need Spack to build Python.
spack --timestamp external find --exclude=meson --exclude=python

# Configure a buildcache
spack --timestamp mirror add --scope=site --type binary scisoft_phlex_ci \
      https://scisoft.fnal.gov/scisoft/phlex-dev-build-cache/ || true

# Remove existing environments
rm -rf "$SPACK_ROOT"/var/spack/environments/*

# Create and prepare an environment to build GCC
spack --timestamp env create gcc-15-2-0 \
      "${script_dir}/gcc-15-2-0.yaml"
spack --timestamp env activate gcc-15-2-0

# Concretize and build GCC
spack --timestamp concretize
spack --timestamp install --fail-fast -j $parallelism -p 1 \
      --no-add --only-concrete --no-check-signature
spack --timestamp env deactivate gcc-15-2-0


# Create and prepare phlex-development environment
spack versions -s llvm | grep -qEe '^[[:space:]]+21\.1\.4$' ||
  spack checksum -ab llvm 21.1.4
spack --timestamp env create phlex-dev "${script_dir}/../ci/spack.yaml"
spack load gcc@15
spack compiler add --scope=env:gcc-15-2-0 gcc@15
spack --timestamp env activate phlex-dev


# Display concretized solution
spack --timestamp concretize

# Install everything except Phlex (we'll be developing that, not
# building it from tagged source)
spack --timestamp install --fail-fast -j $parallelism -p 1 \
    --no-add --only-concrete --no-check-signature \
    $(spack find -r --no-groups | sed -Ene 's&^ - &&p' | grep -v phlex)

# Now install Phlex' dependencies
spack --timestamp install --fail-fast -j $parallelism -p 1 \
    --no-add --only-concrete --only dependencies --no-check-signature \
    phlex

# Generate the activation script
cat > activate_dev_env.sh <<EOF
#!/bin/bash

. \"$SPACK_ROOT/share/spack/setup-env.sh\"
spack env activate phlex-dev
EOF

chmod +x activate-dev-env.sh

echo "Development environment setup complete."
echo "To activate the environment, run the following command:"
echo "source activate-dev-env.sh"
