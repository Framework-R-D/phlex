FROM ghcr.io/framework-r-d/phlex-dev:latest

ARG USERNAME=vscode
ARG USER_UID=1000
ARG USER_GID=1000
ARG SPACK_GID=2000

# Validate Python site-packages symlink
RUN <<'VALIDATE_PYTHON_SYMLINK'
set -euo pipefail

SPACK_VIEW_LIB=/opt/spack-environments/phlex-ci/.spack-env/view/lib

# Find all versioned Python site-packages directories
python_dirs=("$SPACK_VIEW_LIB"/python3.*)
count=0
for dir in "${python_dirs[@]}"; do
  if [ -d "$dir" ]; then
    count=$((count + 1))
    python_dir="$dir"
  fi
done

# Ensure exactly one versioned Python site-packages directory exists
if [ $count -eq 0 ]; then
  echo "ERROR: No versioned Python site-packages directory found in $SPACK_VIEW_LIB" >&2
  exit 1
elif [ $count -gt 1 ]; then
  echo "ERROR: Multiple versioned Python site-packages directories found in $SPACK_VIEW_LIB:" >&2
  ls -d "$SPACK_VIEW_LIB"/python3.* >&2
  exit 1
fi

# Create the symlink if it doesn't exist or update it if it's incorrect
python_link="$SPACK_VIEW_LIB/python"
if [ -L "$python_link" ]; then
  current_target=$(readlink "$python_link")
  expected_target=$(basename "$python_dir")
  if [ "$current_target" != "$expected_target" ]; then
    echo "Updating $python_link -> $expected_target"
    ln -sfn "$expected_target" "$python_link"
  fi
elif [ -e "$python_link" ]; then
  echo "ERROR: $python_link exists but is not a symlink" >&2
  exit 1
else
  echo "Creating $python_link -> $(basename "$python_dir")"
  ln -sn "$(basename "$python_dir")" "$python_link"
fi

# Verify the symlink is correct
if [ ! -L "$python_link" ]; then
  echo "ERROR: Failed to create symlink at $python_link" >&2
  exit 1
fi

echo "Python symlink validated: $python_link -> $(readlink "$python_link")"
VALIDATE_PYTHON_SYMLINK

# Create the user and add to spack group
RUN groupadd --gid $USER_GID $USERNAME \
    && useradd --uid $USER_UID --gid $USER_GID --create-home $USERNAME \
    && usermod -aG $SPACK_GID $USERNAME \
    && echo "$USERNAME ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/$USERNAME \
    && chmod 0440 /etc/sudoers.d/$USERNAME

# Setup entrypoint usage in bashrc
RUN echo '. /entrypoint.sh' >> /home/$USERNAME/.bashrc
