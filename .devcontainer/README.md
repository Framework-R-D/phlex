# Native macOS Devcontainer

The initializer generates the gitignored `docker-compose.host.yml` before VS Code resolves Compose. On Apple Silicon it requires a running rootless arm64 Podman VM and selects the local `localhost/phlex-dev:2026-09-21-aarch64` image. Set `PHLEX_DEV_IMAGE` in the environment launching VS Code to select another locally built arm64 image. Linux retains the Dockerfile's GHCR default.

macOS uses pasta networking. Ordinary development does not mount the Podman engine socket or disable SELinux container labeling. Host Kilo configuration and auth inputs are read-only; auth is copied into the container-private data volume when first created.

## Optional Nested Podman

For future in-container `act` use, fully quit VS Code Insiders first, then launch a fresh process from the repository root:

```bash
PHLEX_ENABLE_NESTED_PODMAN=1 code-insiders .
```

Run **Dev Containers: Rebuild and Reopen in Container**. The initializer then mounts the VM's rootless Unix socket at `/tmp/podman.sock`, sets `DOCKER_HOST` and `CONTAINER_HOST`, and adds `security_opt: ["label=disable"]`. This explicitly reduces isolation: the devcontainer can control all containers and resources managed by the VM's rootless engine. The read-only bind mount does not make API operations read-only. No TCP API or macOS Unix-socket relay is used.

To restore the default boundary, quit Insiders, launch it without `PHLEX_ENABLE_NESTED_PODMAN`, and rebuild the devcontainer. Do not edit the generated override; initialization replaces it on each launch.
