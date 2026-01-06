FROM ghcr.io/framework-r-d/phlex-dev:latest

ARG USERNAME=vscode
ARG USER_UID=1000
ARG USER_GID=1000
ARG SPACK_GID=2000

# Create the user and add to spack group
RUN groupadd --gid $USER_GID $USERNAME \
    && useradd --uid $USER_UID --gid $USER_GID --create-home $USERNAME \
    && usermod -aG $SPACK_GID $USERNAME \
    && echo "$USERNAME ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/$USERNAME \
    && chmod 0440 /etc/sudoers.d/$USERNAME

# Setup entrypoint usage in bashrc
RUN echo '. /entrypoint.sh' >> /home/$USERNAME/.bashrc
