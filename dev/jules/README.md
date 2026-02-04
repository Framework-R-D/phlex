# Jules - Phlex Development Environment

This directory contains the necessary tools to create a consistent and reproducible development environment for Jules, the AI software engineer, to work on the Phlex repository.

## Purpose

The primary goal of this setup is to provide a "clean slate" environment that mirrors the one Jules uses for development tasks. This allows Phlex developers to:

-   Reliably test and debug the environment preparation script (`prepare-environment.sh`).
-   Ensure that any changes to the environment setup will work as expected when provided to Jules.
-   Onboard new developers who may need to interact with or configure Jules's environment.

## Local Development and Testing Workflow

To test or modify the environment preparation script, you should use the provided `Dockerfile` to create a local container.

### Prerequisites

-   Docker must be installed and running on your local machine.

### Steps

1.  **Build the Docker Image**

    From the root of the Phlex repository, run the following command to build the development image:

    ```bash
    docker build -t phlex-jules-dev -f dev/jules/Dockerfile .
    ```

    This will create a local Docker image named `phlex-jules-dev` based on the latest stable `ubuntu:24.04` image. It configures a non-root user `jules` with passwordless `sudo`, creating a minimal, well-defined environment in which to run the preparation script.

2.  **Run the Container**

    Start an interactive session inside the container, mounting your local Phlex repository into the container's workspace:

    ```bash
    docker run -it --rm --volume "$(pwd):/app" phlex-jules-dev /bin/bash
    ```

    You are now inside a shell in the container at the `/app` directory. Any changes you make to the repository files on your host machine will be immediately reflected inside the container.

3.  **Run the Environment Preparation Script**

    From within the container, you can now execute the script to test it:

    ```bash
    dev/jules/prepare-environment.sh
    ```

    If the script completes successfully, the environment is correctly configured.

## Standard Procedure for Jules

When assigning a task to Jules that requires a specific environment, the standard procedure is to instruct Jules to execute the preparation script directly.

For example, the instruction could be:

> Please run `dev/jules/prepare-environment.sh` to set up the development environment before you begin.
