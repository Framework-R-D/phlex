# Native macOS Devcontainer

The initializer generates the gitignored `docker-compose.host.yml` before VS Code resolves Compose. On Apple Silicon it requires a running rootless arm64 Podman VM and a locally available Linux/arm64 image, `localhost/phlex-dev:latest` by default. Set `PHLEX_DEV_IMAGE` in the environment launching VS Code to select another image reference. A multi-arch tag or manifest is supported when Podman can resolve its Linux/arm64 member from local storage; the generated Compose service explicitly selects `linux/arm64`. The initializer does not query remote manifests or pull automatically: for a registry image, explicitly run `podman pull --platform linux/arm64 <image-reference>` first. A manifest listing an arm64 member is not sufficient unless that member is available locally. Linux retains the Dockerfile's `ghcr.io/framework-r-d/phlex-dev:latest` default and the engine's native platform selection.

macOS uses pasta networking. Ordinary development does not mount the Podman engine socket or disable SELinux container labeling. Host Kilo configuration and auth inputs are read-only; auth is copied into the container-private data volume when first created.

## Optional Nested Podman

For future in-container `act` use, fully quit VS Code Insiders first, then launch a fresh process from the repository root:

```bash
PHLEX_ENABLE_NESTED_PODMAN=1 code-insiders .
```

Run **Dev Containers: Rebuild and Reopen in Container**. The initializer then mounts the VM's rootless Unix socket at `/tmp/podman.sock`, sets `DOCKER_HOST` and `CONTAINER_HOST`, and adds `security_opt: ["label=disable"]`. This explicitly reduces isolation: the devcontainer can control all containers and resources managed by the VM's rootless engine. The read-only bind mount does not make API operations read-only. No TCP API or macOS Unix-socket relay is used.

To restore the default boundary, quit Insiders, launch it without `PHLEX_ENABLE_NESTED_PODMAN`, and rebuild the devcontainer. Do not edit the generated override; initialization replaces it on each launch.

## Self-tests

The production scripts load fixture listeners, assertions, and test cases only when their `--self-test` mode is requested. `ensure-repos-self-test.sh` remains beside the host initializer; `kilo-env-self-test.sh` is installed at `/usr/local/lib/phlex/kilo-env-self-test.sh`, outside `/etc/profile.d`, so login shells do not source the test suite. Normal Kilo environment setup and relay cleanup work without these optional test helpers. The existing `--self-test` commands retain their fixture-directory requirements and report an error if the corresponding helper is unavailable.
