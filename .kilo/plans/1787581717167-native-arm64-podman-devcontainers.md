# Plan: Native arm64 Podman Images and VS Code Devcontainers

## Goal

Make the repository's `phlex-ci`, `phlex-dev`, and VS Code devcontainer workflows usable from a rootless Podman machine on an Apple-Silicon Mac, using native `linux/arm64` execution and Spack-installed LLVM/Clang as the default development compiler. Preserve the existing amd64 GitHub CI/reference path, keep the generic CMake default preset compiler-neutral, and keep local arm64 images local-only.

## Decisions and boundaries

- The Mac path is native arm64. It must not silently select `linux/amd64`, `x86_64_v3`, or an emulated Podman machine.
- Linux reference/build nodes are amd64-only. They validate the existing CI path and the amd64 branch of the architecture-aware container build; they cannot produce arm64 artifacts.
- `phlex-ci` remains GCC-default so existing CI matrix jobs retain their explicit GCC/Clang behavior. `phlex-dev` and the locally built VS Code base image default to Spack's `clang` and `clang++` through an image environment setting, with Clang explicitly directed to the Spack GCC 15 toolchain and libstdc++. The `default` CMake preset is unchanged. GCC-specific coverage remains available through an explicit GCC environment override.
- The existing GHCR amd64 images and GitHub workflow definitions are not changed, and no arm64 image is pushed to GHCR.
- Local VS Code selection uses the explicit `PHLEX_DEV_BASE_IMAGE` environment variable. The Compose default remains the pinned GHCR image for clean machines and Codespaces.
- Host TCP services are exposed through an explicit allowlist only. `PHLEX_HOST_RELAY_PORTS` uses comma-separated `source=relay` entries, such as `11434=21434,3000=13000`; no arbitrary host-port scan or wildcard relay is allowed. Headroom's two existing ports remain default entries managed by their existing variables.
- The relay scope is TCP/HTTP services. MCP servers and LSPs that communicate over stdio remain inside the devcontainer or use VS Code's remote extension process; host Unix sockets are outside this plan.
- Local `act` remains a secondary amd64-emulation path. It is not part of native-arm64 acceptance and its `.actrc` configuration is not redesigned.
- No host machine is initialized, changed to rootful mode, or recreated automatically by repository scripts. Host setup is documented and checked before container creation.
- On macOS, the host-side Podman API socket is not mounted through the macOS home-directory VM share. The Compose devcontainer requires `PHLEX_PODMAN_SOCKET_SOURCE` to be explicitly set to the verified VM-side rootless socket path before VS Code starts; this preserves the existing socket-compatible container layout without using it for ordinary development commands. Nested `act` remains optional.
- macOS host-service relays use the existing Podman host endpoint and firewall policy. The Thunderbolt Bridge and unused USB-LAN interface are out of scope because neither has a verified peer or route from the Podman VM.
- On macOS, `PHLEX_HOST_RELAY_BIND_ADDRESS` is required. Exactly one of two modes is permitted, and the mode chosen in step 1 is binding on steps 3, 4, and 6: (a) a specific non-wildcard host-side address that is assignable on the macOS host and verified reachable from the Podman VM; or (b) `0.0.0.0` together with recorded firewall evidence restricting each approved relay port to the Podman VM source subnet. Any other value, or mode (b) without recorded firewall evidence, fails closed.

## Repository baseline

The relevant implementation surfaces are:

- `ci/Dockerfile`, `ci/spack.yaml`, `ci/packages.yaml`, and `ci/entrypoint.sh` for the multi-target images, Spack concretization, and runtime compiler selection.
- `.devcontainer/Dockerfile`, `.devcontainer/docker-compose.yml`, `.devcontainer/devcontainer.json`, `.devcontainer/ensure-repos.sh`, `.devcontainer/kilo-env.sh`, and `.devcontainer/post-create.sh` for the VS Code layer, host socket relay, companion repositories, and secondary developer tools.
- `CMakePresets.json` for the compiler-neutral `default` preset and explicit Clang presets.
- `.github/workflows/cmake-build.yaml`, `.github/workflows/coverage.yaml`, `.github/workflows/clang-tidy-check.yaml`, and related workflows for amd64 CI behavior.

The current worktree has unrelated modifications in `scripts/git-ai-commit` and `scripts/test/test_git_ai_commit.py`; the implementation must not modify or stage them.

## Research findings

- Podman on macOS runs containers inside a Linux Podman machine. The machine is rootless by default and exposes a Docker-compatible API socket whose path is available from `podman machine inspect` under `.ConnectionInfo.PodmanSocket.Path`.
- Linux instructions using `systemctl --user` and `$XDG_RUNTIME_DIR/podman/podman.sock` do not apply to the macOS host. A host-side stable proxy is required by the current Compose bind mount unless the configuration is changed to interpolate the dynamic socket path.
- Podman Compose is a wrapper around an external Compose provider. The host must provide a working `podman compose` provider before VS Code can create the Compose devcontainer.
- Podman documents `host.docker.internal` and `host.containers.internal` for Podman machines; the existing Kilo relay design should retain `host.docker.internal` but must be validated on the native machine.
- A container cannot generally reach a host service bound only to the host's `127.0.0.1` through `localhost`. The existing Headroom relays solve this by forwarding selected loopback ports to alternate host ports reachable through `host.docker.internal`; the same mechanism is needed for approved local AI, HTTP MCP, indexing, and TCP LSP services.
- The current relay script has three portability gaps that affect host-tool access: it assumes Linux `setsid` and `ss`, it hard-codes only Headroom ports, and it does not expose a container-visible map of rewritten endpoint ports. The extension must retain the Linux behavior while adding Darwin listener detection, an allowlisted relay map, and Kilo/general-tool endpoint guidance.
- Podman machine API sockets are remote-backend resources. A Unix socket file on the macOS host's home-directory share is not a reliable socket endpoint inside the Linux VM. The Compose path must distinguish the Linux host proxy from the VM-side rootless socket and must warn that mounting the rootless API socket gives the devcontainer control over the user's Podman machine.
- Spack supports generic `aarch64` and Apple-specific targets, while the current repository hard-codes `x86_64_v3`. The Linux container should use `aarch64` for the native arm64 developer image and retain `x86_64_v3` for amd64 CI/reference builds.
- Spack build caches are architecture- and compiler-specific. The arm64 build must tolerate source builds when the Fermilab mirror only contains amd64 binaries, and any local cache must be separate from an amd64 cache.
- VS Code Dev Containers can use Podman through the `dev.containers.dockerPath` host setting, but Podman is documented as a Docker-compatible alternative rather than an officially supported Docker engine.

Authoritative references used during planning:

- [Podman macOS installation](https://podman.io/docs/installation)
- [`podman machine init`](https://docs.podman.io/en/latest/markdown/podman-machine-init.1.html)
- [`podman machine start`](https://docs.podman.io/en/latest/markdown/podman-machine-start.1.html)
- [`podman machine inspect`](https://docs.podman.io/en/latest/markdown/podman-machine-inspect.1.html)
- [`podman system service`](https://docs.podman.io/en/latest/markdown/podman-system-service.1.html)
- [`podman compose`](https://docs.podman.io/en/latest/markdown/podman-compose.1.html)
- [Spack architecture specifiers](https://spack.readthedocs.io/en/latest/spec_syntax.html#architecture-specifiers)
- [Spack compiler configuration](https://spack.readthedocs.io/en/latest/configuring_compilers.html)
- [Spack build caches](https://spack.readthedocs.io/en/latest/binary_caches.html)
- [VS Code alternate Docker options](https://code.visualstudio.com/remote/advancedcontainers/docker-options)

## Execution limits and safety

- `max_total_dispatches`: 12
- `max_consecutive_failures`: 3
- All repository edits and image-tag operations are designed to be repeatable. Re-running the image builder may replace local tags and reuse build layers, but it must not delete source repositories, host credentials, Podman machines, or remote images.
- No intentionally non-idempotent operation is included. Podman machine initialization is a human precondition and is not automated by this plan. Local build-cache index updates must not be run concurrently by multiple builders.
- A failed arm64 package build must leave the source tree intact. Recovery is to inspect the failing Spack package, adjust Podman machine resources or the declared build inputs, and rerun the same bounded build; do not fall back to amd64 emulation.
- A missing or stale API proxy must be repaired by rerunning the host preflight/relay setup after the Podman machine is running. Do not expose the Podman API over TCP or switch the machine to rootful mode.

## Human-gate evidence

All human-gate evidence is written outside the repository, under `~/.phlex-devcontainer-tmp/plan-evidence/`, one Markdown file per step: `step-1.md`, `step-5.md`, and `step-6.md`. Each file records, in order: UTC timestamp, host identification, every command run verbatim, its full output, its exit status, and the operator's explicit approve or stop decision with the reason. Step 1 additionally writes `step-1-values.env` containing `PHLEX_HOST_GATEWAY`, `PHLEX_HOST_RELAY_BIND_ADDRESS`, the bind mode, `PHLEX_PODMAN_SOCKET_SOURCE`, and the approved relay mappings; step 6 sources that file rather than re-deriving the values. Never record credentials, tokens, or model payloads. These files are host state, not repository files, and do not violate any step's `Allowed files: none`.

## Steps

### 1. Verify the native rootless Podman host and required providers

Executor: `human`

Depends on: none.

Allowed files: none.

Task: Before implementation or validation, verify that the Mac host can provide a native arm64 rootless Podman backend, a Compose provider, and the host utilities required by the existing relay design. If no machine exists, initialize one with rootless mode explicitly selected and start it using the Podman documentation. Allocate at least 8 vCPUs, 16 GiB of VM memory, and 100 GiB of free VM disk for the full LLVM/Spack source build; 24 GiB of memory is preferred when the Mac permits it. Install a Compose provider and `socat` through the host's package-management policy when either is absent. Do not install or use a rootful Podman service.

Run the following from the repository root. `PROBE_IMAGE` must be a native arm64 image containing BusyBox `nc` and `nslookup`; use `docker.io/library/alpine:3.20` unless the operator has approved another arm64 image. Do not use the pinned `phlex-dev` GHCR image here: it is amd64 and would require emulation, which the plan forbids. `PROBE_PORT` is any free host TCP port in 20000-29999 that is not a configured relay port.

```bash
export PROBE_IMAGE=docker.io/library/alpine:3.20
export PROBE_PORT=25999
podman machine inspect --format '{{.State}} rootful={{.Rootful}} memory={{.Resources.Memory}} cpus={{.Resources.CPUs}} disk={{.Resources.DiskSize}} socket={{.ConnectionInfo.PodmanSocket.Path}}'
podman info --format '{{.Host.Arch}} rootless={{.Host.Security.Rootless}}'
podman compose version; echo "compose_exit=$?"
command -v socat; echo "socat_exit=$?"
machine_uid="$(podman machine ssh id -u | tr -d '\\r')"
podman machine ssh test -S "/run/user/${machine_uid}/podman/podman.sock"; echo "vm_socket_exit=$?"
podman run --rm --platform linux/arm64 "$PROBE_IMAGE" sh -lc 'nslookup host.docker.internal; nslookup host.containers.internal'
ifconfig | awk '/^[a-z0-9]+:/ {iface=$1} /inet / {print iface, $2}'
```

Choose `PHLEX_HOST_GATEWAY` as the first documented alias that resolves in the probe output. Choose the bind-address mode defined in "Decisions and boundaries". The container-visible gateway address is a VM-side address and is never a valid macOS bind target; in mode (a) the bind address must be one of the host addresses listed by the `ifconfig` command above. Do not use a Thunderbolt Bridge or USB-LAN address unless a peer and route have been separately supplied and verified.

Verify the chosen bind address end to end with a temporary listener, then stop it:

```bash
export PHLEX_HOST_GATEWAY=<host.docker.internal-or-host.containers.internal>
export BIND_MODE=<a-or-b>
export BIND_ADDR=<chosen-bind-address>
socat TCP-LISTEN:${PROBE_PORT},bind=${BIND_ADDR},reuseaddr,fork EXEC:'/bin/echo phlex-probe-ok' &
probe_pid=$!
podman run --rm --platform linux/arm64 "$PROBE_IMAGE" sh -lc "nc -w 3 ${PHLEX_HOST_GATEWAY} ${PROBE_PORT}"
kill "$probe_pid"
```

For mode (b), before approving, also run and record the complete output of `sudo pfctl -sr` and `sudo /usr/libexec/ApplicationFirewall/socketfilterfw --getglobalstate`; approve only when the recorded firewall rules restrict every approved relay port to the Podman VM source subnet. The probe passes only when the container prints `phlex-probe-ok`. Record `PROBE_IMAGE`, `PROBE_PORT`, every command's output and exit status, the selected gateway, the selected bind mode and address, the VM socket path, the machine resource values, and any mode (b) firewall evidence in `~/.phlex-devcontainer-tmp/plan-evidence/step-1.md`. Write the selected values to `~/.phlex-devcontainer-tmp/plan-evidence/step-1-values.env`. The probe container must not mount or modify repository files.

Acceptance:

- The machine state is `running`, `.Rootful` is `false`, and the machine API socket path is a real Unix socket.
- `podman info` reports `arm64` or `aarch64` for the backend and reports rootless operation.
- `podman compose version` succeeds.
- `socat` is available. `socat` is mandatory: it implements both the stable host socket proxy and every host relay required by steps 3 and 6, and `.devcontainer/ensure-repos.sh` skips relays with a warning when it is absent. If `socat` cannot be installed, stop at this gate; do not approve.
- `podman machine ssh` confirms the rootless VM socket exists and supplies the value used for `PHLEX_PODMAN_SOCKET_SOURCE` when optional nested Podman access is enabled.
- A selected `PHLEX_HOST_GATEWAY`, a bind mode compliant with "Decisions and boundaries", and a successful `phlex-probe-ok` result are recorded; the values are passed to later steps rather than rediscovered implicitly.

Verification:

- Record the exact output of every command above, the approved test port, the selected gateway and bind address, and the Podman machine resource values in the file named in "Human-gate evidence". The human gate approval must explicitly state which bind mode was selected and why the recorded evidence satisfies it.
- Stop at this gate if the backend reports amd64, rootful mode, an unavailable Compose provider, missing `socat`, a failed `phlex-probe-ok` result, or bind evidence that does not satisfy the selected mode. After remediation, resume by rerunning step 1 in full; do not approve from partial evidence.

Gate: human approval after the preflight output is recorded.

Retry policy: `max_attempts: 1`; `strategy: abort`.

Idempotent: true.

### 2. Make the Spack image definition architecture-aware and set the developer compiler default

Executor: `coder-qwen`. This is a bounded subagent implementation task. The primary orchestrator must dispatch it and enforce the listed allowed-file and prohibited-change boundaries; it must not edit these files directly.

Depends on: step 1.

Allowed files: `ci/Dockerfile`, `ci/spack.yaml`, `ci/packages.yaml`, `ci/entrypoint.sh`.

Task: Generalize the existing image build without splitting the CI and developer definitions. Add a validated `PHLEX_SPACK_TARGET` build input with `x86_64_v3` as the amd64 default and `aarch64` as the arm64 value. Render or otherwise pass that value into every current microarchitecture constraint, including the GCC bootstrap, the global package requirement, CMake, LLVM, and any package-specific target requirement. Separately change the LLVM backend `targets=` variant from the current x86-only value to an architecture-specific value that includes `aarch64` on arm64; do not confuse the LLVM backend target list with Spack's `target=` microarchitecture requirement. Reject a target/host-architecture mismatch early rather than allowing an arm64 build to begin with `x86_64_v3` or an x86-only LLVM backend.

Preserve the existing GCC 15 bootstrap and ABI constraints, but make all architecture-specific paths use Spack queries or the selected target rather than literal amd64 paths. Keep the existing Fermilab mirror and local cache behavior, while ensuring a cache built for amd64 cannot satisfy arm64 concretization accidentally. Make the non-Spack downloads architecture-aware: select the Linux arm64 `act` archive when it exists, otherwise support an explicit `INSTALL_ACT=false` arm64 build; apply the same explicit architecture check or opt-out to tracebox rather than downloading an amd64 binary into an arm64 image.

Add a `PHLEX_DEFAULT_COMPILER` runtime switch to `ci/entrypoint.sh`. The CI stage must default to GCC 15 and continue exporting `CC=gcc` and `CXX=g++`. The developer stage must set `PHLEX_DEFAULT_COMPILER=clang`, export `CC=clang` and `CXX=clang++` after activating the Spack view, and export a reproducible Clang driver flag such as `--gcc-toolchain=$(spack location -i gcc@15)` through the compiler flags used by CMake and direct shell builds. Preserve the existing GCC 15 `PATH` prepend because it is the reason Clang can discover the intended C++ runtime. Keep the GCC 15 bin directory available so an explicit GCC build remains possible. Do not change `CMakePresets.json`.

Retain a comment explaining that the current GCC override exists because Spack's environment activation selects Clang through compiler virtuals; the new switch must be an explicit override layered on top of that behavior, not a deletion of the GCC path setup.

Acceptance:

- An amd64 build selects `x86_64_v3`, retains the current GCC 15 CI ABI rules, and defaults to GCC.
- An arm64 build selects `aarch64`, concretizes without any `x86_64_v3` requirement, and contains Spack-provided LLVM/Clang tools.
- `phlex-dev` defaults to `clang`/`clang++`; `phlex-ci` defaults to `gcc`/`g++`.
- `clang++ -print-targets` includes the native arm64 backend and a trivial C++23 compile uses the Spack GCC 15 libstdc++.
- The generic `default` CMake preset remains compiler-neutral and the explicit `clang-tidy` and `coverage-clang` presets remain unchanged. `coverage-gcc` is documented and tested with `PHLEX_DEFAULT_COMPILER=gcc` so it never inherits the Clang default accidentally.

Prohibited changes: Do not modify `CMakePresets.json`, `.github/workflows/*`, `.actrc`, `scripts/git-ai-commit`, or `scripts/test/test_git_ai_commit.py`. Do not stage, commit, or push changes, push images, mutate the Podman machine, or permit an amd64 cache or binary in the arm64 image.

Verification:

- Run shell syntax checks on `ci/entrypoint.sh`.
- Inspect the rendered or effective Spack configuration before installation and assert that its target matches the selected architecture.
- Run bounded concretize-only checks for both `PHLEX_SPACK_TARGET=x86_64_v3` on amd64 and `PHLEX_SPACK_TARGET=aarch64` on arm64 before any full image build.
- Build the `ci` and `dev` targets once on an amd64 reference node and once on the native arm64 Podman machine in later validation steps.

Gate: automatic.

Retry policy: `max_attempts: 2`; `strategy: resume_then_narrow`.

Idempotent: true.

### 3. Make the host relay and Compose devcontainer path macOS-safe

Executor: `coder-qwen`. This is a bounded subagent implementation task. The primary orchestrator must dispatch it and enforce the listed allowed-file and prohibited-change boundaries; it must not edit these files directly.

Depends on: steps 1 and 2.

Allowed files: `.devcontainer/ensure-repos.sh`, `.devcontainer/kilo-env.sh`, `.devcontainer/Dockerfile`, `.devcontainer/docker-compose.yml`, `.devcontainer/devcontainer.json`.

Task: Preserve the existing Linux rootless path while adding a Darwin path. On macOS, discover the rootless VM socket with `podman machine ssh`, verify it exists at the VM's `/run/user/<uid>/podman/podman.sock`, require `PHLEX_PODMAN_SOCKET_SOURCE` before Compose is invoked, and use that path as the remote-backend bind source. Do not mount the macOS host API socket through the home-directory share, derive the Mac socket from `XDG_RUNTIME_DIR`, call `systemctl`, bind a TCP Podman API, or change machine rootfulness. Make the relay process launch portable because macOS does not provide the Linux `setsid` command by default. Keep the dummy-socket fallback only on Linux when the optional nested-container source is absent; on Darwin fail early with an actionable message rather than creating a host-side socket that cannot cross the VM share. Add a security warning that mounting the VM rootless API socket grants the container control over the user's rootless Podman machine.

Refactor the existing Headroom relay helper into a single allowlisted TCP relay path. Preserve `HEADROOM_AZURE_PORT` and `HEADROOM_OW_PORT` as the default approved source ports and add `PHLEX_HOST_RELAY_PORTS` with the exact `source=relay` syntax. Validate numeric ports in the non-privileged range, reject duplicates and source-equals-relay mappings, reject already-bound relay ports, and require a listening host loopback service before starting a relay. Use a `PHLEX_HOST_RELAY_BIND_ADDRESS` value: Darwin requires an explicit host-side value chosen during host preflight and rejects an unset value, while Linux retains the current `0.0.0.0` default for compatibility. On Darwin, reject `0.0.0.0` unless firewall evidence restricts approved relay ports to the Podman VM source subnet. Bind only the selected alternate relay port, record successful mappings in a generated host-relay map under `~/.phlex-devcontainer-tmp`, and use a safe per-relay process/PID cleanup strategy rather than killing unrelated `socat` processes. Do not scan ports, relay all host traffic, or accept a wildcard allowlist.

Always write an empty or populated JSON map and environment map before Compose starts, mount the containing `~/.phlex-devcontainer-tmp` directory read-only at `/run/phlex-host-relays`, and expose `PHLEX_HOST_GATEWAY`, `PHLEX_HOST_RELAY_FILE`, and `PHLEX_HOST_RELAYS_ENV` to container processes. Use the gateway and bind address selected during step 1. Update `kilo-env.sh` to safely discover either `kilo.json` or `kilo.jsonc`, leave the original configuration untouched when parsing fails, match `127.0.0.1`, `localhost`, and `[::1]`, and rewrite only the in-container Kilo configuration under the container user's home using the generated source-to-relay map rather than a fixed offset. Back up that in-container configuration before a successful rewrite. Never rewrite the macOS host's `~/.config/kilo` or `~/.kilo` files. Do not claim that generic MCP configuration is automatically rewritten: provide the map and gateway variables for manually configured HTTP MCP, local model, indexer, and TCP LSP clients. Do not claim that stdio MCP or stdio LSP can be tunneled. Defer container-side gateway and relay reachability verification to step 6, after the final test container exists. Keep credential mounts and the existing rootless volume layout intact.

Change `.devcontainer/Dockerfile` to accept `PHLEX_DEV_BASE_IMAGE`, defaulting to the pinned GHCR image. Change Compose to pass that build argument and to accept `PHLEX_DEV_CONTAINER_IMAGE`, with a distinct local default image tag. Make the Podman socket source an explicit `PHLEX_PODMAN_SOCKET_SOURCE` variable: Linux defaults to the stable proxy path, while Mac instructions set it to the VM-side rootless socket when nested act is intentionally enabled. The Compose file must continue to work with the remote GHCR default when the environment variable is unset. Evaluate existing `:Z` bind flags on macOS during the real create; retain them if the Podman version accepts them, otherwise switch the affected mounts to long-form bind syntax and preserve Linux rootless labeling through a Linux-only Compose path.

Acceptance:

- Linux hosts continue using the current rootless socket discovery and relay behavior.
- macOS hosts do not mount the host API socket through the VM share. Compose uses only the explicitly exported verified VM-side rootless socket path; ordinary development does not invoke `act`, while the socket remains available for the existing optional compatibility path.
- No host-side script requires `systemctl`, a Linux runtime socket path, root privileges, or a TCP Podman API on Darwin.
- `PHLEX_HOST_RELAY_PORTS` forwards only explicitly listed loopback ports, produces a deterministic source-to-relay map, and leaves unlisted host services unreachable from the devcontainer through this mechanism.
- Kilo provider URLs are safely rewritten from approved loopback ports to their relay ports, while generic clients receive the selected gateway name and map file without unsafe automatic configuration changes.
- The generated relay listener is documented as a host exposure boundary. Darwin uses the bind mode selected in step 1, and step 6 verifies reachability from the final test container. In mode (a) it is bound only to the recorded specific address; in mode (b) it is bound to `0.0.0.0` and the recorded firewall rules restrict each relay port to the Podman VM source subnet. The implementation must not expose a wildcard port range, log credentials, or claim that a VM interface address is a stable macOS bind target.
- `PHLEX_DEV_BASE_IMAGE=localhost/phlex-dev:arm64-local` causes the final devcontainer layer to build from the local arm64 base, while an unset variable still selects `ghcr.io/framework-r-d/phlex-dev:2026-07-23`.
- The final Compose service has an explicit local image name and does not overwrite a GHCR reference.

Prohibited changes: Do not modify `.github/workflows/*`, `.actrc`, `CMakePresets.json`, `scripts/git-ai-commit`, or `scripts/test/test_git_ai_commit.py`. Do not stage, commit, or push changes, push images, mutate the Podman machine, expose a TCP Podman API, scan ports, relay wildcard traffic, rewrite the macOS host Kilo configuration, or rewrite stdio MCP/LSP configuration.

Verification:

- Run `bash -n .devcontainer/ensure-repos.sh`.
- Run `bash -n .devcontainer/kilo-env.sh`.
- Run `python3 -m json.tool .devcontainer/devcontainer.json >/dev/null` rather than passing JSON to the shell parser.
- Run `podman compose -f .devcontainer/docker-compose.yml config` once with the local image variables set and once with them unset; inspect both resolved base-image arguments.
- Run the host relay setup twice with a test loopback TCP listener and one unlisted port; verify that the second run replaces only its own relay, the generated map contains only the approved listening port, the unlisted port has no relay, and the stable Linux Podman socket remains usable. Defer container-side gateway reachability to step 6, after the final test container exists.

Gate: automatic.

Retry policy: `max_attempts: 2`; `strategy: resume_then_narrow`.

Idempotent: true.

### 4. Add an architecture-detecting local image builder and Mac instructions

Executor: `coder-qwen`. This is a bounded subagent implementation task. The primary orchestrator must dispatch it and enforce the listed allowed-file and prohibited-change boundaries; it must not edit these files directly.

Depends on: step 3.

Allowed files: `scripts/build-container-images.sh`, `docs/dev/podman-macos.md`, `scripts/README.md`.

Task: Add an executable, repeatable builder that queries `podman info` for the backend architecture, maps amd64 to `x86_64_v3` and arm64 to `aarch64`, and builds the `ci` and `dev` targets from the correct `ci` build context with `--format docker`. Tag local results as `localhost/phlex-ci:<arch>-local` and `localhost/phlex-dev:<arch>-local`, then build the Compose final layer as `localhost/phlex-devcontainer:<arch>-local` with `PHLEX_DEV_BASE_IMAGE` set to the local base tag. Select the arm64 `act` archive and verify its executable architecture when available; pass an explicit `INSTALL_ACT=false` fallback when it is not available. Select or explicitly disable tracebox based on a verified arm64 artifact. Reject unsupported architectures and refuse to proceed when a native arm64 request would use an amd64 backend.

Support an optional, explicitly named local Spack cache directory such as `PHLEX_SPACK_CACHE_ARM64`; never reuse an amd64 cache directory for arm64 packages. Preserve optional GPG signing arguments without placing keys in the repository or command output. The builder must not push images or modify the Podman machine. Make repeated runs replace only the three local tags and reuse build layers.

Document the complete Mac workflow and host-tool network contract:

1. Start or verify a rootless native arm64 Podman machine and allocate at least 8 vCPUs, 16 GiB of memory, and 100 GiB of free VM disk.
2. Verify `podman compose`, `socat`, the VM-side rootless socket, and the selected `PHLEX_HOST_GATEWAY`; do not assume a Thunderbolt Bridge or USB-LAN address is reachable from the VM.
3. Set `PHLEX_HOST_RELAY_BIND_ADDRESS` to the verified existing host-side address reachable from the Podman machine, set `PHLEX_HOST_RELAY_PORTS` for approved TCP services, and start those services before VS Code's `initializeCommand` runs. If the bind address is `0.0.0.0`, install or verify firewall rules allowing only the Podman VM source subnet and approved relay ports before starting the relay.
4. Run the builder from the repository root.
5. Export `PHLEX_DEV_BASE_IMAGE=localhost/phlex-dev:arm64-local`, `PHLEX_DEV_CONTAINER_IMAGE=localhost/phlex-devcontainer:arm64-local`, and `PHLEX_PODMAN_SOCKET_SOURCE=/run/user/<machine-uid>/podman/podman.sock` in the shell from which VS Code is launched. The socket variable is required by the Compose mount; nested `act` remains an optional consumer of that socket.
6. Configure the host-side VS Code setting `dev.containers.dockerPath` to `podman`.
7. Reopen or rebuild the folder in the devcontainer.
8. Remove the image variables and optional socket/relay variables to return to the pinned GHCR base and no custom host relays.

Document the explicit host-service allowlist. Show `PHLEX_HOST_RELAY_PORTS='11434=21434,3000=13000'` as a concrete configuration for a loopback local model and an HTTP MCP service, state that the source services must be listening before `initializeCommand` runs, and explain that clients inside the container use the selected gateway plus `:21434` and `:13000`. Document the generated JSON and environment maps and exported gateway variables as the integration contract for indexers and TCP LSP clients. State that stdio MCP and stdio LSP processes should run in the devcontainer and that host Unix sockets are not forwarded. Explain that relay ports are deliberately bound for Podman-machine reachability; only explicitly approved services with appropriate authentication may be listed. Document the macOS application-firewall prompt caused by the first non-loopback `socat` listener and require the operator to verify the chosen host-side bind address, firewall restriction, and gateway from a test container. State that the Thunderbolt Bridge and USB-LAN interface are not used unless a separately documented peer and route are supplied.

Document that the first arm64 build normally builds from source because the existing Fermilab cache is amd64-oriented, that build directories must be recreated when switching compilers, that `coverage-gcc` requires an explicit GCC environment override, and that local images are not published. Document `act` only as a separate optional amd64-emulation feature, not as a native development requirement. Link the new guide from `scripts/README.md`.

Acceptance:

- The builder produces all three local arm64 tags on a native arm64 Podman backend.
- The builder produces the corresponding amd64 tags on an amd64 Linux backend without changing CI workflow files.
- The instructions are sufficient for a developer to select the local image explicitly from a terminal-launched VS Code process.
- All generated host relay state is outside the repository under `~/.phlex-devcontainer-tmp`; no repository-local generated file is required.

Prohibited changes: Do not modify `.github/workflows/*`, `.actrc`, `CMakePresets.json`, `scripts/git-ai-commit`, or `scripts/test/test_git_ai_commit.py`. Do not stage, commit, or push changes, push images, mutate the Podman machine, reuse an amd64 cache for arm64, place GPG keys in the repository or command output, or generate repository-local relay state.

Verification:

- Run `bash -n scripts/build-container-images.sh`.
- Run a relay-only dry run that validates `PHLEX_HOST_RELAY_PORTS` syntax, rejects malformed or duplicate mappings, and does not start a relay when a source port is not listening.
- Run the builder's architecture-detection and dry-run checks without starting a full package build.
- Run the documentation and shell checks required by the repository's pre-commit configuration, including the new link from `scripts/README.md`.

Gate: automatic.

Retry policy: `max_attempts: 2`; `strategy: resume_then_narrow`.

Idempotent: true.

### 5. Validate static configuration and the amd64 reference path

Executor: `human`

Depends on: steps 2, 3, and 4.

Allowed files: the temporary clean reference checkout on the operator-provided amd64 Linux node may contain the files modified by steps 2-4; the primary worktree remains unchanged. Evidence is written only under the path defined in "Human-gate evidence".

Task: Validate the implementation before the native Mac gate. Use JSON parsing for `.devcontainer/devcontainer.json`, Compose configuration resolution for both image-selection modes, separate shell syntax checks for every changed shell file, the repository's detected `prek` or `pre-commit` command on the changed files, and a Linux rootless devcontainer acceptance run. The Linux run must start the Compose service, run `ensure-repos.sh` with a loopback test service, verify the Headroom/default relay behavior and generated map, test Kilo URL rewriting without exposing credentials, and confirm that the stable Linux Podman socket still works for the optional act configuration. Build the `ci` and `dev` targets on an amd64 Linux reference node with `PHLEX_SPACK_TARGET=x86_64_v3` and verify that the CI image remains GCC-default and the developer image is Clang-default.

Run these checks against both concrete amd64 images and record every line of output:

```bash
for image in localhost/phlex-ci:amd64-local localhost/phlex-dev:amd64-local; do
  podman image inspect --format '{{.Architecture}} {{.Os}}' "$image"
  podman run --rm "$image" sh -lc 'echo "CC=$CC CXX=$CXX"; command -v spack clang clang++ clangd clang-tidy cmake ninja python3 gh gcc g++; clang++ -print-targets | head -20; clang++ -v 2>&1 | grep -i "gcc installation"; file "$(command -v act)" 2>/dev/null || echo "act absent"'
done
```

Expected: the `ci` image reports `CC=gcc CXX=g++`; the `dev` image reports `CC=clang CXX=clang++`; every listed tool resolves inside the Spack view; the reported GCC installation is the Spack GCC 15 prefix, not `/usr`; and `file` reports the image's own architecture for any downloaded binary.

Run representative image checks for both stages: architecture, `spack` availability, the selected compiler variables, `clang`, `clang++`, `clangd`, `clang-tidy`, CMake, Ninja, Python, `gh`, the developer-only formatting tools, the native LLVM backend, the GCC 15 toolchain selection, and the arm64 or amd64 architecture of downloaded binaries. Exercise the relay parser with valid, duplicate, malformed, unavailable, and unlisted ports without contacting external services. Confirm that existing GitHub workflow files remain unchanged and that no push command or registry mutation is part of the builder.

Acceptance:

- Static checks pass.
- The amd64 images build and run with the existing CI architecture and compiler behavior.
- The local builder's tags and the Compose base-image argument resolve deterministically.
- No unrelated worktree changes are touched.

Verification:

```bash
python3 -m json.tool .devcontainer/devcontainer.json >/dev/null
podman compose -f .devcontainer/docker-compose.yml config >/dev/null
PHLEX_DEV_BASE_IMAGE=localhost/phlex-dev:amd64-local PHLEX_DEV_CONTAINER_IMAGE=localhost/phlex-devcontainer:amd64-local podman compose -f .devcontainer/docker-compose.yml config >/dev/null
bash -n ci/entrypoint.sh
bash -n .devcontainer/ensure-repos.sh
bash -n .devcontainer/kilo-env.sh
bash -n scripts/build-container-images.sh
PREKCOMMAND=$(command -v prek || command -v pre-commit || true)
git diff --binary -- ci .devcontainer scripts ':!scripts/git-ai-commit' ':!scripts/test/test_git_ai_commit.py' docs > /tmp/phlex-before-hooks.patch
[ -z "$PREKCOMMAND" ] || "$PREKCOMMAND" run --show-diff-on-failure --files ci/Dockerfile ci/spack.yaml ci/packages.yaml ci/entrypoint.sh .devcontainer/Dockerfile .devcontainer/docker-compose.yml .devcontainer/devcontainer.json .devcontainer/ensure-repos.sh .devcontainer/kilo-env.sh scripts/build-container-images.sh scripts/README.md docs/dev/podman-macos.md
git diff --binary -- ci .devcontainer scripts ':!scripts/git-ai-commit' ':!scripts/test/test_git_ai_commit.py' docs > /tmp/phlex-after-hooks.patch
cmp -s /tmp/phlex-before-hooks.patch /tmp/phlex-after-hooks.patch
```

Fixer hooks rewrite files in place. If the `cmp` command fails, revert only the hook-applied changes with `git checkout -- <paths>`, record the before/after patch difference as evidence, and return the finding to the step 2-4 executors; this step must not leave hook-applied edits in the worktree. The two temporary patch files are evidence helpers and must not be copied into the repository or reference image.

Use the operator-provided amd64 Linux reference node. Before starting, record the node hostname, the access transport (for example `ssh <user>@<host>`), and confirmation that `podman`, a Compose provider, `socat`, and `python3` are present; stop at this gate if any is missing. Because steps 2-4 prohibit staging or committing, transfer the working-tree changes without creating a commit: run `git diff -- ci .devcontainer scripts ':!scripts/git-ai-commit' ':!scripts/test/test_git_ai_commit.py' docs > /tmp/phlex-arm64.patch` on the Mac, copy it to a clean checkout of the same base revision on the reference node (`git rev-parse HEAD` must match on both), and apply it with `git apply --check` followed by `git apply`. Do not transfer `scripts/git-ai-commit` or `scripts/test/test_git_ai_commit.py` changes. On that node, run the exact verification command block above, run the image builder with `PHLEX_SPACK_TARGET=x86_64_v3`, start the Linux Compose service, and execute the Linux relay acceptance described in step 3. Record the node hostname, OS and kernel architecture, repository revision, concrete Spack target, image architecture labels, compiler paths, package availability, relay map, Compose output, and command exit statuses in the file named in "Human-gate evidence". Do not copy credentials or model payloads into the evidence. This is a human-run validation gate because the current execution environment does not provide a declared remote-node transport.

Failure and stop conditions:

- Any command in the verification block exits non-zero, any expected value above does not match, a fixer changes a file outside the transferred implementation paths, or the reference node is unavailable: stop, record the failure, and do not approve.
- On a stop, execution resumes by returning the specific finding to the owning step (2, 3, or 4) and re-running this step in full; do not partially re-approve.

Gate: human approval after static checks and the Linux reference evidence are recorded.

Retry policy: `max_attempts: 1`; `strategy: abort`.

Idempotent: true.

### 6. Perform the native arm64 end-to-end acceptance run

Executor: `human`

Depends on: step 5.

Allowed files: none during validation; build and CMake output must remain in ignored local image, VM, cache, and build directories. Evidence is written only under the path defined in "Human-gate evidence".

Task: On the Apple-Silicon Mac, source `~/.phlex-devcontainer-tmp/plan-evidence/step-1-values.env`, use its recorded gateway, bind mode, bind address, socket path, and approved relay mappings, and do not re-derive them. Start only the approved host services before the devcontainer initialize hook; export `PHLEX_PODMAN_SOCKET_SOURCE` as the verified VM-side rootless socket; run the new builder with a separate arm64 cache if one is configured; verify `localhost/phlex-ci:arm64-local`, `localhost/phlex-dev:arm64-local`, and `localhost/phlex-devcontainer:arm64-local`; and build the VS Code Compose devcontainer. Launch VS Code from the same shell that exports the image, socket, and relay variables, and confirm the host setting `dev.containers.dockerPath` is `podman` before reopening the folder. If the recorded address is stale or no longer reachable, stop and repeat step 1 rather than selecting a different address. Inside the final devcontainer, verify the runtime environment and build Phlex using the unchanged compiler-neutral default preset while the dev image supplies Clang:

```bash
podman image exists localhost/phlex-ci:arm64-local && podman image exists localhost/phlex-dev:arm64-local && podman image exists localhost/phlex-devcontainer:arm64-local; echo "tags_exit=$?"
rm -rf build
cmake --preset default -B build
cmake --build build
ctest --test-dir build --output-on-failure --test-timeout 90
```

Verify that `CC=clang`, `CXX=clang++`, `clang++`, `clangd`, and `clang-tidy` resolve into the Spack view, that `clang++ -print-targets` includes AArch64, that `clang++ -v` shows the GCC 15 toolchain, that a trivial C++23 compile succeeds, that CMake's generated compiler is Clang, and that the image reports arm64. Verify that the VS Code extensions and Kilo/Headroom configuration can start without requiring rootful Podman. Run one explicit `coverage-gcc` configure with `PHLEX_DEFAULT_COMPILER=gcc` set in the shell to prove the GCC-specific preset does not inherit the Clang default. If local AI credentials or a Headroom relay are not available, record those checks as optional while still verifying the container-side tools and configuration paths.

For the approved example TCP service `11434=21434`, start the loopback source service before the devcontainer initialize hook, record its source and relay ports, and verify the complete path from the host loopback listener through the host `socat` relay using the step 1 bind mode, the selected `PHLEX_HOST_GATEWAY`, and the devcontainer. Check the generated source-to-relay map, Kilo provider URL rewriting for JSON and JSONC config discovery, and a client request from inside the container. Verify that an unlisted host port is not present in the map and is not reachable through the relay. Verify that the relay listener matches the step 1 recorded mode: in mode (a) it is bound only to the recorded specific address; in mode (b) it is bound to `0.0.0.0` and the recorded firewall rules restrict each relay port to the Podman VM source subnet, verified with the exact firewall command output recorded in step 1. Confirm that a stdio MCP server and the configured VS Code LSP path are treated as in-container processes rather than incorrectly routed through TCP. If optional nested act is tested, verify it reaches the VM-side rootless socket rather than the macOS host socket.

Acceptance:

- `phlex-ci`, `phlex-dev`, and the final VS Code devcontainer all run as native arm64 images.
- The ordinary development build uses Spack LLVM/Clang without changing `CMakePresets.json`.
- The Clang driver uses the Spack GCC 15 headers, libraries, and runtime rather than the Ubuntu system GCC 13 installation.
- The full configured build and test command succeeds. If it fails, the gate is not approved: record the failing Spack spec, the reproducible arm64-specific cause, and a bounded recovery action, stop, and resume at the owning implementation step after remediation. Approval requires a successful configure, build, and test run.
- The Podman machine remains rootless, optional API access uses only the verified VM-side Unix socket, and no amd64 image is selected by the native builder.
- Local secondary tools remain usable: `rg`, `gh`, `clangd`, `clang-tidy`, `prek`, Python tooling, and the Kilo command are present; network-backed AI access is reported separately from image correctness.
- Approved host-based AI, HTTP MCP, indexer, and TCP LSP endpoints are reachable through their explicit relay mappings, while unlisted ports are not forwarded. Stdio MCP and stdio LSP workflows remain usable inside the devcontainer.

Verification:

- Capture `podman machine inspect`, `podman machine ssh` socket checks, `podman info`, local image inspection, compiler paths, CMake compiler identification, and CTest results in the file named in "Human-gate evidence".
- Capture the allowlist, generated relay map, relay listener checks, container-side endpoint checks, and the negative test for an unlisted port without recording credentials or model payloads.
- Confirm `git status --short` contains no generated untracked source or credential files beyond intentionally ignored build/cache state.
- Leave `.actrc` and all GitHub workflow files unchanged.

Gate: human approval based on the recorded arm64 acceptance evidence.

Retry policy: `max_attempts: 1`; `strategy: abort`.

Idempotent: true.

## Outcome evidence

The implementation is complete only when the recorded evidence shows:

1. A rootless native arm64 Podman machine and working Compose provider.
2. Successful architecture-aware Spack concretization and image construction for arm64, with a separate amd64 reference result.
3. Explicit local image selection through `PHLEX_DEV_BASE_IMAGE` and a separate final devcontainer tag.
4. Spack LLVM/Clang as the default developer compiler while the generic CMake preset remains unchanged.
5. A successful native arm64 VS Code devcontainer build and representative Phlex configure/build/test run.
6. No GHCR push, rootful Podman transition, CI workflow redesign, or required local `act` emulation.
7. An explicit, auditable host-service relay allowlist preserves access to approved host-based AI and development services without forwarding arbitrary loopback ports.

## Architect review record

The first architect review of this Markdown artifact reported 12 blocking findings and 9 non-blocking findings. This revision addresses the blocking findings in the following sections: LLVM backend architecture and arm64 binary downloads in step 2; GCC 15 libstdc++ selection and `coverage-gcc` override behavior in steps 2 and 6; VM-side Podman socket selection and API security in steps 1, 3, 4, and 6; selectable Podman host gateway and bind-address security in steps 3, 4, and 6; safe Kilo JSON/JSONC discovery and map-driven provider rewriting in steps 3 and 6; unconditional relay-map generation and directory mounting in step 3; Linux end-to-end relay regression in step 5; the human gate for the operator-provided amd64 reference node in step 5; and separate shell/JSON verification commands in step 5.

This revision introduces or makes explicit the following implementation surfaces:

- Paths: `scripts/build-container-images.sh`, `docs/dev/podman-macos.md`, `/run/user/<machine-uid>/podman/podman.sock`, `/run/phlex-host-relays`, `~/.phlex-devcontainer-tmp/host-relays.json`, and `~/.phlex-devcontainer-tmp/host-relays.env`. The first two are planned repository files; the remaining paths are runtime or host state.
- Commands: `podman machine ssh`, `podman machine ssh id -u`, `podman machine ssh test -S`, `python3 -m json.tool`, `clang++ -print-targets`, and `clang++ -v`.
- Dependency mechanisms: architecture-specific `act` release selection, explicit tracebox opt-out, Spack `targets=` backend selection, Spack GCC 15 toolchain flags, an external Compose provider, `socat`, and the `PHLEX_HOST_RELAY_PORTS`, `PHLEX_HOST_RELAY_BIND_ADDRESS`, `PHLEX_HOST_GATEWAY`, `PHLEX_HOST_RELAYS_ENV`, and `PHLEX_PODMAN_SOCKET_SOURCE` environment contracts.
- Configuration touches: Compose's local base-image argument, final image tag, VM-side socket source, host-relay directory mount, gateway variables, Kilo JSON/JSONC selection, and the `scripts/README.md` link to the new Mac guide.

The post-review blocking-finding count recorded here is 12 at the initial review time. A targeted executor-boundary review then reported four blocking findings: invalid subagent syntax, stale compiled state, missing gateway/bind-address provenance, and insufficient per-step prohibited-change boundaries. This revision addresses those findings by naming `coder-qwen` directly, requiring step 1 to record gateway and bind values, deferring container reachability to step 6, adding explicit prohibitions to steps 2-4, and making the human validation gate explicit. The revised implementation surfaces are unchanged except for the delegated `coder-qwen` worker assignment; no new repository path, command, filesystem object, dependency mechanism, or configuration touch is introduced by this revision. The revised source must pass the deterministic source lint before compilation.

A targeted Architect review of human-gated steps 1, 5, and 6 reported 13 blocking findings. This revision addresses the findings by making the macOS bind modes normative, replacing the duplicated preflight commands with an arm64 probe and temporary listener procedure, requiring `socat`, defining an external evidence store, defining the amd64 reference-node transport and patch transfer, making hook checks detect mutations, specifying image checks and stop/resume behavior, requiring a successful final build and test before approval, and carrying the recorded bind mode through step 6. This revision introduces or makes explicit these additional surfaces: paths `~/.phlex-devcontainer-tmp/plan-evidence/step-1.md`, `~/.phlex-devcontainer-tmp/plan-evidence/step-5.md`, `~/.phlex-devcontainer-tmp/plan-evidence/step-6.md`, `~/.phlex-devcontainer-tmp/plan-evidence/step-1-values.env`, `/tmp/phlex-arm64.patch`, `/tmp/phlex-before-hooks.patch`, and `/tmp/phlex-after-hooks.patch`; commands `ifconfig`, `awk`, `nslookup`, `nc`, the temporary `socat` listener, `pfctl`, `socketfilterfw`, `podman image exists`, `rm -rf build`, `git diff --binary`, `git apply --check`, `git apply`, and `cmp`; the arm64 `docker.io/library/alpine:3.20` probe-image dependency; the operator-provided amd64 reference-node transport; and the host firewall-evidence requirement for wildcard binding. No repository implementation path, workflow, or configuration file is newly assigned to a subagent. The revised source must pass deterministic source lint before recompilation.
