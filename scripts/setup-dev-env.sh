#!/bin/bash

# This script sets up the development environment for the Phlex project.
# It is based on the contents of the ci/Dockerfile.

set -e

# Clone Spack if it's not already present
if [ ! -d "spack" ]; then
    git clone --depth=2 https://github.com/spack/spack.git
fi

# Set up and configure basic Spack installation
. spack/share/spack/setup-env.sh

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

# Configure a buildcache
spack --timestamp mirror add --scope=site --type binary scisoft_phlex_ci https://www.example.com/

# Create and prepare phlex-development environment
spack versions -s llvm | grep -qEe '^[[:space:]]+21\.1\.4$' || spack checksum -ab llvm 21.1.4
spack --timestamp env create phlex-dev ci/spack.yaml
spack --timestamp env activate phlex-dev

# Display concretized solution
spack --timestamp concretize

parallelism=$(nproc)

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
cat > scripts/activate_dev_env.sh <<EOF
#!/bin/bash

. \$(dirname \${BASH_SOURCE[0]})/../spack/share/spack/setup-env.sh
spack env activate phlex-dev
EOF

chmod +x scripts/activate_dev_env.sh

echo "Development environment setup complete."
echo "To activate the environment, run the following command:"
echo "source scripts/activate_dev_env.sh"
