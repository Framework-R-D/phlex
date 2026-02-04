# syntax=docker/dockerfile:1.4
FROM ubuntu:24.04

# Avoid prompts from apt
ENV DEBIAN_FRONTEND=noninteractive

# Install bootstrap dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    curl \
    git \
    locales \
    locales-all \
    lsb-release \
    python3 \
    python3-pip \
    sudo \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Create a user 'jules' with sudo privileges
ARG USERNAME=jules
ARG USER_UID=1000
ARG USER_GID=1000

RUN groupadd --gid $USER_GID $USERNAME \
    && useradd --uid $USER_UID --gid $USER_GID --create-home $USERNAME \
    && echo "$USERNAME ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/$USERNAME \
    && chmod 0440 /etc/sudoers.d/$USERNAME

USER $USERNAME
WORKDIR /app

# The repository should be mounted at /app when running this container
# docker run -it --rm -v $(pwd):/app phlex-jules-dev /bin/bash

CMD ["/bin/bash"]
