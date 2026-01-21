# Use the latest LTS Ubuntu image as a stable, well-known base.
FROM ubuntu:24.04

# Set non-interactive frontend for package installations
ENV DEBIAN_FRONTEND=noninteractive

# Install sudo, create a non-root user named 'jules', and grant passwordless
# sudo privileges. This user will be used to run the environment script.
RUN apt-get update && \
    apt-get install -y --no-install-recommends sudo ca-certificates && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/* && \
    useradd --create-home --shell /bin/bash jules && \
    echo "jules ALL=(ALL) NOPASSWD: ALL" >> /etc/sudoers

# Switch to the non-root user.
USER jules

# Set the working directory where the repository will be mounted.
WORKDIR /workspaces/phlex
