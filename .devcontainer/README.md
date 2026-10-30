# Native macOS Devcontainer

The initializer generates the gitignored `docker-compose.host.yml` before VS Code resolves Compose. On Apple Silicon it requires a running rootless arm64 Podman VM. The base image defaults to the published `ghcr.io/framework-r-d/phlex-dev:latest` on every platform, matching the Dockerfile and CI. The generated Compose service explicitly selects `linux/arm64`, and the initializer verifies that the selected reference resolves to a Linux/arm64 image in local storage before VS Code starts; an emulated amd64 member is rejected. A manifest list stored locally is not sufficient on its own: when a registry reference has no local Linux/arm64 member, the initializer runs `podman pull --platform linux/arm64 <image-reference>` once, and reports that same command if the pull fails. Linux keeps the engine's native platform selection.

Set `PHLEX_DEV_IMAGE` in the environment launching VS Code to select another image reference, for example a date-stamped image built natively with `scripts/generate-image dev`. The script prints the exact `PHLEX_DEV_IMAGE` value to use. Local `localhost/` references are never pulled; they must already exist in Podman storage.

macOS uses pasta networking. Ordinary development does not mount the Podman engine socket or disable SELinux container labeling. Host Kilo configuration and auth inputs are read-only; auth is copied into the container-private data volume when first created.

## Optional Nested Podman

For future in-container `act` use, fully quit VS Code Insiders first, then launch a fresh process from the repository root:

```bash
PHLEX_ENABLE_NESTED_PODMAN=1 code-insiders .
```

Run **Dev Containers: Rebuild and Reopen in Container**. The initializer then mounts the VM's rootless Unix socket at `/tmp/podman.sock`, sets `DOCKER_HOST` and `CONTAINER_HOST`, and adds `security_opt: ["label=disable"]`. This explicitly reduces isolation: the devcontainer can control all containers and resources managed by the VM's rootless engine. The read-only bind mount does not make API operations read-only. No TCP API or macOS Unix-socket relay is used.

To restore the default boundary, quit Insiders, launch it without `PHLEX_ENABLE_NESTED_PODMAN`, and rebuild the devcontainer. Do not edit the generated override; initialization replaces it on each launch.

## Self-tests

`kilo-env-self-test.sh` and `ensure-repos.sh` execute their own unit tests when the `--self-test` option is provided. Fixture listeners, assertions, and test cases are loaded by sourcing the appropriate `*-self-test.sh` file, and the tests executed in lieu of each script executing its production code.
