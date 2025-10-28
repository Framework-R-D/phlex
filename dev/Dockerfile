# Dev container Dockerfile for Phlex
# Uses the CI image as a base and adds VS Code dev container requirements
#
# Podman instructions for building an image with this file:
#
#   $ cd <directory with this file>
#   $ podman build --format docker --tag phlex-dev:<tag> [-v <local-buildcache>:/build-cache] .
#   $ podman login ghcr.io --username <username> -p <token>
#   $ podman push phlex-dev:<tag> ghcr.io/framework-r-d/phlex-dev:<tag>
# ... and optionally:
#   $ podman push phlex-dev:<tag> ghcr.io/framework-r-d/phlex-dev:latest
#
# where <tag> is the date (e.g. "2025-08-12").

FROM ghcr.io/framework-r-d/phlex-ci:latest

# Create vscode user with passwordless sudo
RUN apt-get update && \
    apt-get install -y sudo locales curl git vim nano && \
    useradd -m vscode && \
    echo "vscode ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/vscode && \
    chmod 0440 /etc/sudoers.d/vscode

## Set locale (ensure locale files are present)
RUN apt-get update && \
    apt-get install -y locales-all || apt-get install -y locales && \
    locale-gen en_US.UTF-8 || true
ENV LANG=en_US.UTF-8
ENV LC_ALL=en_US.UTF-8

# Set user and workdir for dev container
USER vscode
WORKDIR /workspaces/phlex

# Default shell initialization and healthcheck (optional)
# Note: Podman OCI format ignores SHELL and HEALTHCHECK. Use --format docker if needed.
SHELL ["/bin/bash", "-c"]
HEALTHCHECK CMD curl -f https://github.com || exit 1

# The image is now ready for VS Code dev container use
