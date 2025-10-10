# Dockerfile for creating phlex-ci-rg image: a minimal runner-derived
# image with ripgrep available
#
# Podman instructions for building an image with this file:
#
#   $ cd <directory with this file>
#   $ podman build --tag phlex-ci-rg:<tag> .
#   $ podman login ghcr.io --username <username> -p <token>
#   $ podman push phlex-ci-rg:<tag> ghcr.io/framework-r-d/phlex-ci-rg:<tag>
#
# where <tag> is the date (e.g. "2025-08-12").

FROM ghcr.io/actions/actions-runner:latest

USER root
RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends ripgrep \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

USER runner
