FROM ghcr.io/framework-r-d/phlex-dev:latest

# Validate Python site-packages symlink
RUN <<'VALIDATE_PYTHON_SYMLINK'
set -euo pipefail
shopt -s nullglob

SPACK_VIEW_LIB=/opt/spack-environments/phlex-ci/.spack-env/view/lib

# Find all versioned Python site-packages directories
python_dirs=("$SPACK_VIEW_LIB"/python3.*)
count=${#python_dirs[@]}

# Ensure exactly one versioned Python site-packages directory exists
if [ $count -eq 0 ]; then
  echo "ERROR: No versioned Python site-packages directory found in $SPACK_VIEW_LIB" >&2
  exit 1
elif [ $count -gt 1 ]; then
  echo "ERROR: Multiple versioned Python site-packages directories found in $SPACK_VIEW_LIB:" >&2
  printf '  %s\n' "${python_dirs[@]}" >&2
  exit 1
fi

# Get the single Python directory and its basename
python_dir="${python_dirs[0]}"
python_basename=$(basename "$python_dir")

# Create the symlink if it doesn't exist or update it if it's incorrect
python_link="$SPACK_VIEW_LIB/python"
if [ -L "$python_link" ]; then
  current_target=$(readlink "$python_link")
  if [ "$current_target" != "$python_basename" ]; then
    echo "Updating $python_link -> $python_basename"
    ln -sfn "$python_basename" "$python_link"
  fi
elif [ -e "$python_link" ]; then
  echo "ERROR: $python_link exists but is not a symlink" >&2
  exit 1
else
  echo "Creating $python_link -> $python_basename"
  ln -sn "$python_basename" "$python_link"
fi

# Verify the symlink is correct
if [ ! -L "$python_link" ]; then
  echo "ERROR: Failed to create symlink at $python_link" >&2
  exit 1
fi

# Verify the symlink target is a valid directory
if [ ! -d "$python_link" ]; then
  echo "ERROR: Symlink $python_link does not point to a valid directory" >&2
  exit 1
fi

echo "Python symlink validated: $python_link -> $(readlink "$python_link")"
VALIDATE_PYTHON_SYMLINK

# Install podman client and socat inside the container to support nested
# container communication and provide a 'docker' command alias.
RUN <<'INSTALL_PODMAN_CLIENT'
set -euo pipefail
apt-get update
apt-get install -y --no-install-recommends podman socat
apt-get clean
rm -rf /var/lib/apt/lists/*
INSTALL_PODMAN_CLIENT

# Wire up the root user's .bashrc to source the Spack environment via
# /entrypoint.sh on every interactive shell.
RUN printf '. /entrypoint.sh\n' >> /root/.bashrc

# Ensure /run/user/0 exists and is owned by root with 700 permissions to
# allow GPG agent to function correctly when running as root.
RUN mkdir -p /run/user/0 && chmod 700 /run/user/0 && chown root:root /run/user/0
