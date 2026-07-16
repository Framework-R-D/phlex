FROM ghcr.io/framework-r-d/phlex-dev:2026-06-24

# Validate Python site-packages symlink.
# Invoke bash explicitly: the script uses bash features (pipefail, arrays,
# shopt). Do NOT rely on the base image's SHELL being inherited -- the OCI
# image config has no Shell field, so podman/buildah builds run RUN under
# /bin/sh and these heredocs would fail. `RUN bash <<...` works on both
# docker (BuildKit) and podman/buildah regardless of image format.
RUN bash <<'VALIDATE_PYTHON_SYMLINK'
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

# Remove the Ubuntu minimal dpkg excludes file so that subsequent package
# installs populate /usr/share/man and /usr/share/doc normally.  Without this,
# every apt-get install silently strips man pages and other documentation,
# producing the "This system has been minimized" warning whenever `man` is run.
# Also install man-db (the `man` command and database) and groff (the troff
# rendering pipeline required to display man pages), then reinstall git and
# git-man to restore man pages that were already stripped at image build time.
# mandoc is added as a lightweight alternative renderer (e.g. for CI linting).
RUN bash <<'INSTALL_MAN_TOOLS'
set -euo pipefail
# Remove the dpkg excludes file so future installs populate /usr/share/man.
rm -f /etc/dpkg/dpkg.cfg.d/excludes
apt-get update
apt-get install -y --no-install-recommends man-db groff mandoc
# Reinstall git and git-man to recover already-stripped man pages.
apt-get install -y --no-install-recommends --reinstall git git-man
apt-get clean
rm -rf /var/lib/apt/lists/*
# Ubuntu minimal diverts /usr/bin/man to a stub shell script that prints the
# "This system has been minimized" warning.  Installing man-db is not enough
# to restore the real binary — the divert must be explicitly removed.
if [ "$(dpkg-divert --truename /usr/bin/man)" = "/usr/bin/man.REAL" ]; then
    rm -f /usr/bin/man
    dpkg-divert --quiet --remove --rename /usr/bin/man
fi
INSTALL_MAN_TOOLS

# Install dev utils
# container communication and provide a 'docker' command alias.
RUN bash <<'INSTALL_DEV_UTILS'
set -euo pipefail
apt-get update
apt-get install -y --no-install-recommends jq podman ssh socat tree yq
apt purge nodejs npm -y && apt autoremove -y || true
apt-get install -y curl --no-install-recommends --autoremove
curl -fsSL https://deb.nodesource.com/setup_26.x | bash -
apt-get install -y nodejs
apt-get clean
rm -rf /var/lib/apt/lists/*
INSTALL_DEV_UTILS

RUN bash <<'INSTALL_NPM_PACKAGES'
set -euo pipefail
npm config set allow-scripts=@kilocode/cli --location=user
npm install -g @kilocode/cli
INSTALL_NPM_PACKAGES

# Wire up the root user's .bashrc to source the Spack environment via
# /entrypoint.sh on every interactive shell.
RUN printf '. /entrypoint.sh\n' >> /root/.bashrc

# Ensure /run/user/0 exists and is owned by root with 700 permissions to
# allow GPG agent to function correctly when running as root.
RUN mkdir -p /run/user/0 && chmod 700 /run/user/0 && chown root:root /run/user/0
