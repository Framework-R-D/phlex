# Native macOS Devcontainer

The initializer generates the gitignored `docker-compose.host.yml` before VS Code resolves Compose. The base image defaults to the published `ghcr.io/framework-r-d/phlex-dev:latest`, matching the devcontainer Dockerfile; CI uses `ghcr.io/framework-r-d/phlex-ci:latest` instead. On macOS, the initializer requires a running rootless arm64 Podman VM, explicitly selects `linux/arm64` in the generated Compose service, and inspects the selected image in local storage before VS Code starts. On Linux, the initializer leaves image acquisition and platform selection to the engine.

On macOS, when local inspection cannot resolve the reference and it is not a `localhost/` reference, the initializer runs `podman pull --platform linux/arm64 <image-reference>` once and inspects again. A local manifest list is not sufficient on its own: inspection must resolve a Linux/arm64 image. An inspectable non-arm64 image (or non-Linux image) is rejected without pulling; an incompatible image is also rejected after pulling. The first download can be large; optionally run that pull command manually before launching VS Code. Progress notices go to stdout; failures produce one `ERROR:` line on stderr, with the manual command if pulling fails.

Set `PHLEX_DEV_IMAGE` in the environment launching VS Code to select another image reference, for example a date-stamped image built natively with `scripts/generate-image dev`. The script prints the exact `PHLEX_DEV_IMAGE` value to use in its `PHLEX_DEV_IMAGE=localhost/<build-tag> code .` launch example; substitute `code-insiders` for `code` when using VS Code Insiders. On macOS, local `localhost/` references are never pulled; they must already exist in Podman storage.

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
