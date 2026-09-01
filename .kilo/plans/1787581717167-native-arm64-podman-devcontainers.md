# Plan: Native arm64 Podman Images and VS Code Devcontainers

Workspace root: `/Users/greenc/work/cet-is/sources/phlex/phlex`

## Goal

Make the repository's `phlex-ci`, `phlex-dev`, and VS Code devcontainer workflows usable from a rootless Podman machine on an Apple-Silicon Mac, using native `linux/arm64` execution and Spack-installed LLVM/Clang as the default development compiler. Preserve the existing amd64 GitHub CI/reference path, keep the generic CMake default preset compiler-neutral, and keep local arm64 images local-only.

## Recovery posture from the most recent execution

The previous execution approved the native-arm64 host gate, repaired step 2 once, then stopped when the step-3 worker modified prohibited `.devcontainer/post-create.sh`. The recorded state claims steps 1 and 2 completed, but the current worktree still has the original `ci/spack.yaml`, `ci/Dockerfile`, and `ci/entrypoint.sh` architecture/compiler settings and contains no `PHLEX_SPACK_TARGET` or `PHLEX_DEFAULT_COMPILER` implementation. The previous completion evidence is therefore not sufficient to skip implementation or verification.

This revision changes the plan source, so the existing `.exec.json` and `.state.json` are stale plan-family artifacts. Compilation must replace the stable artifact only after validation and archive the mismatched state; Execute must not resume the old state or reset it. Prior host evidence may inform step 1, but the new runner contract requires step 1 to be rerun and explicitly approved. No step may restore an entire file set after a failure because the worktree contains unrelated changes in `scripts/git-ai-commit` and `scripts/test/test_git_ai_commit.py`; change provenance must be checked per path.

The previous `.devcontainer/post-create.sh` edit was functionally unnecessary: its current final comment states that `kilo-env.sh` is installed as `/etc/profile.d/kilo-env.sh`, and the profile script is self-contained. Before dispatching step 1, the primary must inspect this file independently. If its worktree diff is exactly the inert prior execution edit, the primary records the pre-change hash and restores only that file from the matching base revision; if the diff contains anything else, execution stops for human review. The primary must not restore the whole `.devcontainer` directory. Step 6 treats the resulting clean hash as the baseline and rejects any later change to this path.

## Decisions and boundaries

- The Mac path is native arm64. It must not silently select `linux/amd64`, `x86_64_v3`, or an emulated Podman machine.
- Linux reference/build nodes are amd64-only, but this plan no longer performs a remote amd64 image build. Existing GitHub CI remains the amd64 regression path; this plan performs local static assertions for the amd64-compatible branch and records actual amd64 regression as deferred to the eventual PR CI run.
- `phlex-ci` remains GCC-default. `phlex-dev` and the locally built VS Code base image default to Spack's `clang` and `clang++`, with Clang explicitly directed to the Spack GCC 15 toolchain and libstdc++. The `default` CMake preset is unchanged. GCC-specific coverage remains available through an explicit GCC environment override.
- Existing GHCR amd64 images and GitHub workflow definitions are not changed, and no arm64 image is pushed to GHCR.
- Local VS Code selection uses explicit `PHLEX_DEV_BASE_IMAGE` and `PHLEX_DEV_CONTAINER_IMAGE` variables. The Compose default remains the pinned GHCR image for clean machines and Codespaces.
- Host TCP services are exposed through an explicit allowlist only. `PHLEX_HOST_RELAY_PORTS` uses comma-separated `source=relay` entries, such as `11434=21434,3000=13000`; no arbitrary host-port scan or wildcard relay is allowed. Headroom's existing ports remain default entries managed by their existing variables.
- Relay validation and Mac Compose acceptance use Podman's `--net=pasta` path with a host listener bound to `127.0.0.1`. Default Podman networking, a host-interface bind, Thunderbolt Bridge, USB-LAN, and a manually assigned Mac address are not equivalent evidence and are out of scope.
- The selected gateway is the first resolving documented Podman alias, preferring `host.docker.internal` and otherwise using `host.containers.internal`. Record the hostname, not a resolved IP.
- `PHLEX_HOST_RELAY_BIND_ADDRESS`, firewall mode (b), `pfctl`, and `socketfilterfw` evidence are removed. macOS relays bind to `127.0.0.1`; the pasta path supplies container reachability and no wildcard host exposure is needed. Linux preserves its existing bind behavior.
- The relay scope is TCP/HTTP services. MCP servers and LSPs that communicate over stdio remain inside the devcontainer or use VS Code's remote extension process; host Unix sockets are outside this plan.
- Local `act` remains an optional amd64-emulation path. It is not part of native-arm64 acceptance and `.actrc` is not redesigned.
- No host machine is initialized, changed to rootful mode, or recreated automatically by repository scripts.
- On macOS, the host-side Podman API socket is not mounted through the macOS home-directory VM share. The Compose devcontainer requires `PHLEX_PODMAN_SOCKET_SOURCE` to be explicitly set to the verified VM-side rootless socket path before VS Code starts; this optional nested-Podman path grants the container control of the user's rootless Podman machine and ordinary development does not invoke it.

## Repository baseline and change ownership

Implementation surfaces are `ci/Dockerfile`, `ci/spack.yaml`, `ci/packages.yaml`, `ci/entrypoint.sh`, `.devcontainer/ensure-repos.sh`, `.devcontainer/kilo-env.sh`, `.devcontainer/Dockerfile`, `.devcontainer/docker-compose.yml`, `.devcontainer/devcontainer.json`, `scripts/build-container-images.sh`, `scripts/README.md`, and `docs/dev/podman-macos.md`.

The following paths are permanently prohibited: `.github/workflows/*`, `.actrc`, `CMakePresets.json`, `scripts/git-ai-commit`, `scripts/test/test_git_ai_commit.py`, `.devcontainer/post-create.sh`, all `.kilo/*`, `kilo.json`, and `AGENTS.md`. The primary records the pre-dispatch status and hashes of the two unrelated script changes and the post-create disposition. After each delegated step, it asserts that only that step's allowed files changed relative to that baseline plus prior successful implementation changes. An out-of-scope path is a blocking failure; do not automatically revert it or clobber a pre-existing change.

## Human-gate evidence and runner contract

Human gates use the execution-plans human-gate runner contract. Execute materializes the named runner script as `/private/tmp/1787581717167-native-arm64-podman-devcontainers-<step-id>.sh`, validates it with the skill-bundled `execution-plans/scripts/validate_runner.py`, sets owner-only permissions, and records its hash, phase receipts, and log hash. The operator runs phases in order as child processes; no multiline shell transcript is pasted into an interactive shell. Every runner has `preflight` first and `verify` last, uses `run_check`, `finish`, `usage`, `set -uo pipefail`, `umask 077`, no `set -e`, and no credentials or model payloads in logs.

Evidence is written outside the repository under `~/.phlex-devcontainer-tmp/plan-evidence/`. Step 1 owns `step-1.md` and `step-1-values.env`; step 7 owns `step-7.md`. Evidence records UTC time, host identity, allowlisted command/output fields, exit status, phase receipts, and the explicit approve or stop decision. Step 7 sources the values file and never re-derives gateway or socket values. Runner `log_policy` is `allowlisted`; credentials, tokens, model payloads, and raw authenticated responses are suppressed.

## Execution limits and safety

- `max_total_dispatches`: 12
- `max_consecutive_failures`: 3
- All implementation and local image-tag operations are idempotent. Re-running a builder may replace only the three declared local tags and reuse build layers.
- No intentionally non-idempotent operation is included. Human startup of a test service occurs only when its reserved port is free; evidence records ownership and the operator stops only processes started for this gate.
- A failed arm64 package build leaves source repositories intact. Recovery is to inspect the failing Spack package, adjust machine resources or declared inputs, and rerun the bounded build; never fall back to amd64 emulation.
- A missing or stale API socket or relay is repaired by rerunning the relevant host phase after the Podman machine is running. Do not expose the Podman API over TCP or switch the machine to rootful mode.

## Steps

### 1. Verify the native rootless Podman host and Compose pasta path

Executor: `human`

Depends on: none.

Allowed files: none

Runner script: `native-arm64-podman-host.sh`; phases: `preflight`, `inspect-host`, `probe-pasta`, `probe-compose-pasta`, `record-values`, `verify`; log policy: `allowlisted`.

Task: Run the named runner phases in order. `preflight` checks the workspace root, Podman, a Compose provider, `socat`, and writable evidence directories. `inspect-host` checks that the Podman machine is running, native arm64/aarch64, rootless, has at least 8 vCPUs, at least 16 GiB of VM memory, at least 100 GiB of free VM disk, and that `podman machine ssh` confirms the rootless VM socket at `/run/user/<uid>/podman/podman.sock`. If the machine is absent or stopped, perform the documented Podman machine initialization or start operation between `inspect-host` and `probe-pasta`, explicitly retaining rootless mode and the resource thresholds, then rerun `inspect-host`. If Compose or `socat` is absent, install it through the host package-management policy between phases, then rerun `preflight`.

`probe-pasta` starts an allowlisted temporary listener on the Mac at `127.0.0.1:<free-port>` and runs the approved native arm64 Alpine probe with Podman's exact `--net=pasta` option. The probe resolves both documented gateway hostnames, connects through the selected hostname, and requires `phlex-probe-ok`. It does not use the pinned amd64 image, a bridge network, a host-interface address, or repository mounts. Select `PHLEX_HOST_GATEWAY` by preferring a resolving `host.docker.internal` and otherwise using `host.containers.internal`. `probe-compose-pasta` uses the selected Compose provider and `PHLEX_DEV_NETWORK_MODE=pasta` to start a minimal temporary service, then repeats the `127.0.0.1` listener probe through that service. It records the provider name and version and fails if Compose does not actually create the pasta network path. `record-values` writes the selected gateway, provider/version, `PHLEX_PODMAN_SOCKET_SOURCE`, `PHLEX_TEST_RELAY_PORTS=25114=25115,25300=25301`, and approved relay mappings to the owned values file. Interactive machine setup or package installation occurs only between named phases and is followed by the specified phase rerun.

Acceptance:

- The machine is running, native arm64/aarch64, rootless, and has a real VM Unix socket.
- The machine has at least 8 vCPUs, at least 16 GiB of VM memory, and at least 100 GiB of free VM disk.
- Compose and `socat` are available.
- The exact `--net=pasta` arm64 probe reaches the `127.0.0.1` listener and prints `phlex-probe-ok`.
- The selected Compose provider, with `PHLEX_DEV_NETWORK_MODE=pasta`, reaches the same listener from a temporary service.
- The selected gateway hostname, provider/version, VM socket path, test relay mappings, approved mappings, machine resources, probe image, and probe port are recorded.
- No rootful service, TCP Podman API, manually assigned interface address, Thunderbolt Bridge, USB-LAN, or amd64/emulated probe is used.

Verification: Run `verify` after the phase sequence. It rechecks every acceptance condition as individually labelled checks, including each numeric resource threshold and Compose-level pasta reachability, verifies the values file contains only required non-secret fields, writes the step receipt, and waits for the operator's explicit approve or stop decision. On failure, stop and rerun step 1 in full after remediation; silence never advances the gate.

Gate: human approval after the runner's `verify` phase and evidence review.

Retry policy: `max_attempts: 1`; `strategy: abort`.

Idempotent: true.

### 2. Make the Spack image definition architecture-aware and set the developer compiler default

Executor: `coder-qwen`. This is a bounded subagent implementation task; the primary must enforce the allowed-file boundary and the baseline scope assertion.

Depends on: step 1.

Allowed files: `ci/Dockerfile`, `ci/spack.yaml`, `ci/packages.yaml`, `ci/entrypoint.sh`.

Task: Generalize the existing image definitions without splitting CI and developer manifests. Add a validated `PHLEX_SPACK_TARGET` build input with `x86_64_v3` for amd64 and `aarch64` for arm64. Propagate it into every current Spack `target=` constraint, including GCC bootstrap, package defaults, CMake, LLVM, and package-specific requirements. Make LLVM's `targets=` backend list architecture-specific and include AArch64 on arm64; do not confuse that list with Spack's microarchitecture requirement. Reject target and host-architecture mismatches before installation. Keep the GCC 15 ABI and Fermilab mirror behavior, and prevent amd64 build caches from satisfying arm64 concretization.

Make `act` and tracebox downloads architecture-aware. Select a verified arm64 act artifact when available or accept explicit `INSTALL_ACT=false`; apply the same explicit artifact check or opt-out to tracebox. Add `PHLEX_DEFAULT_COMPILER` to `ci/entrypoint.sh`: CI defaults to GCC 15 with `CC=gcc` and `CXX=g++`; the developer stage activates the Spack view and defaults to `CC=clang` and `CXX=clang++`, while retaining the GCC 15 PATH and adding a reproducible `--gcc-toolchain` binding for Clang. Preserve an explicit GCC override and explain why it is needed when Spack activation selects Clang through compiler virtuals. Do not change `CMakePresets.json`.

Acceptance: amd64 renders `x86_64_v3` and remains GCC-default; arm64 renders `aarch64` with no x86 microarchitecture requirement; the arm64 LLVM toolchain includes AArch64; the developer image defaults to Clang; the CI image defaults to GCC; a trivial C++23 compile uses Spack GCC 15 libstdc++; and the `default`, `clang-tidy`, `coverage-clang`, and `coverage-gcc` preset files remain unchanged.

Prohibited changes: Do not modify any prohibited path listed above, stage, commit, push, push images, mutate the Podman machine, or use an amd64 cache or binary in an arm64 image.

Verification: Run shell syntax checks, inspect effective Spack configuration, run a bounded arm64 concretize-only check inside the native arm64 build context before a full build, and assert the rendered amd64 branch statically selects `x86_64_v3` without running an amd64 container. Run the baseline scope assertion and record the changed-path manifest and verification output. The subagent must not edit `.devcontainer/post-create.sh` or any other out-of-scope path.

Gate: automatic.

Retry policy: `max_attempts: 2`; `strategy: resume_then_narrow`.

Idempotent: true.

### 3. Implement the allowlisted host relay and map contract

Executor: `coder-qwen`. This is a narrow bounded subagent task; the primary must enforce the allowed-file boundary and the baseline scope assertion.

Depends on: step 1.

Allowed files: `.devcontainer/ensure-repos.sh`, `.devcontainer/kilo-env.sh`.

Task: Preserve Linux behavior, including its existing relay bind behavior, while adding the Darwin `--net=pasta` path. Refactor only the relay and Kilo environment logic: preserve the two Headroom source variables, parse exact `source=relay` entries from `PHLEX_HOST_RELAY_PORTS`, validate non-privileged numeric ports, reject duplicates and source-equals-relay mappings, require listening loopback sources, bind Darwin relays to `127.0.0.1`, and use per-relay process/PID cleanup. Always write empty or populated JSON and environment maps before Compose starts under `~/.phlex-devcontainer-tmp`; expose gateway and map variables; never scan ports or relay wildcard traffic.

Keep Kilo rewriting write-free. Discover `kilo.json` or `kilo.jsonc`, parse the mounted host configuration, rewrite only approved `127.0.0.1`, `localhost`, and `[::1]` URLs in memory using the generated source-to-relay map, and export `KILO_CONFIG_CONTENT`. Never write or back up the host mount and never edit `.devcontainer/post-create.sh`. Generic HTTP MCP, local-model, indexer, and TCP-LSP clients receive the gateway and maps for manual configuration. Stdio MCP, stdio LSP, and host Unix sockets are not tunneled.

Acceptance: Linux retains its existing relay behavior; Darwin requires no `systemctl`, Linux runtime socket path, root privilege, host-interface bind, or TCP Podman API; only explicitly listed listening source ports are relayed; maps are deterministic and generated even when empty; and Kilo rewriting is in-memory and map-driven.

Prohibited changes: Do not modify any prohibited path, especially `.devcontainer/post-create.sh`; do not stage, commit, push, mutate the machine, expose TCP Podman, scan ports, relay wildcard traffic, write host Kilo configuration, or rewrite stdio configuration.

Verification: Run `bash -n` on both shell files, exercise valid/duplicate/malformed/unavailable/unlisted relay mappings without external services, run twice to prove cleanup is limited to owned relays, inspect generated empty and populated maps, test JSON and JSONC Kilo discovery, and run the baseline scope assertion. Do not edit `.devcontainer/post-create.sh`.

Gate: automatic.

Retry policy: `max_attempts: 2`; `strategy: resume_then_narrow`.

Idempotent: true.

### 4. Wire Compose, socket access, and local image selection

Executor: `coder-qwen`. This is a narrow bounded subagent task; the primary must enforce the allowed-file boundary and the baseline scope assertion.

Depends on: steps 1 and 3.

Allowed files: `.devcontainer/Dockerfile`, `.devcontainer/docker-compose.yml`, `.devcontainer/devcontainer.json`.

Task: Add the Compose-selectable `PHLEX_DEV_NETWORK_MODE`, with the Mac workflow resolving to `pasta` and the existing provider-compatible default retained elsewhere. Verify that the chosen Compose provider emits the equivalent Podman network mode. Require `PHLEX_PODMAN_SOCKET_SOURCE` on macOS and use the verified VM-side rootless socket; retain the Linux stable proxy path and warn that the optional mount grants rootless-machine control. Do not mount a Mac home-share API socket, derive a Mac socket from `XDG_RUNTIME_DIR`, call `systemctl`, expose TCP Podman, or change machine rootfulness.

Change the devcontainer Dockerfile and Compose variables for the pinned GHCR base, local base image, and distinct final image tag. Mount the relay-map directory read-only at `/run/phlex-host-relays`, expose gateway/map variables, and keep credential mounts and the existing rootless volume layout. Do not make `post-create.sh` part of the solution.

Acceptance: the local arm64 variables resolve to `localhost/phlex-dev:arm64-local` and `localhost/phlex-devcontainer:arm64-local` without overwriting GHCR references; the Mac Compose service resolves `pasta`; Linux and clean-machine defaults remain usable; and the VM-side socket is the only Mac nested-Podman source.

Prohibited changes: Do not modify any prohibited path, stage, commit, push, mutate the machine, expose TCP Podman, write host Kilo configuration, or change the relay contract implemented in step 3.

Verification: Parse `devcontainer.json` with `python3 -m json.tool`, resolve Compose with local and default variables, inspect the resolved network mode and socket source, validate read-only relay-map mounting, and run the baseline scope assertion. Container-side reachability is deferred to step 7.

Gate: automatic.

Retry policy: `max_attempts: 2`; `strategy: resume_then_narrow`.

Idempotent: true.

### 5. Add an architecture-detecting local image builder and Mac instructions

Executor: `coder-qwen`. This is a bounded subagent implementation task; the primary must enforce the allowed-file boundary and the baseline scope assertion.

Depends on: step 4.

Allowed files: `scripts/build-container-images.sh`, `docs/dev/podman-macos.md`, `scripts/README.md`.

Task: Add an executable repeatable builder that queries the Podman backend architecture, maps amd64 to `x86_64_v3` and arm64 to `aarch64`, refuses unsupported or emulated native-arm64 requests, builds `ci` and `dev` from the correct context with Docker format, and tags only `localhost/phlex-ci:<arch>-local`, `localhost/phlex-dev:<arch>-local`, and `localhost/phlex-devcontainer:<arch>-local`. Build the final layer from the local dev base with the explicit architecture and network variables. Select verified arm64 act and tracebox artifacts or pass explicit opt-outs. Separate an optional arm64 Spack cache from any amd64 cache and preserve optional GPG signing without keys in the repository or output. Never push images or mutate the machine.

Document the complete Mac workflow: native rootless machine and numeric resource gate; Compose, `socat`, VM socket, and preferred gateway checks; the exact `--net=pasta` Compose path; the controlled relay test services; `PHLEX_HOST_RELAY_PORTS='11434=21434,3000=13000'` as a user configuration example; `PHLEX_DEV_NETWORK_MODE=pasta`; local image and socket variables; terminal-launched VS Code with `dev.containers.dockerPath=podman`; devcontainer reopen; generated map contract; stdio limitations; first-listener firewall prompt handling without claiming firewall restriction is required; separate cache and compiler-directory guidance; optional amd64-emulation act; and local-only image policy. Link the guide from `scripts/README.md`.

Acceptance: the builder produces all three local arm64 tags on a native arm64 backend and corresponding amd64 tags on an amd64 backend without workflow changes. The instructions are sufficient without reconstructing omitted values, and all relay state remains under `~/.phlex-devcontainer-tmp`.

Prohibited changes: Do not modify any prohibited path, stage, commit, push, mutate the machine, reuse an amd64 cache for arm64, place GPG keys in the repository or output, or generate repository-local relay state.

Verification: Run shell syntax checks, relay-only dry-run checks, builder architecture and dry-run checks, documentation link checks, and the baseline scope assertion. Do not start a full package build in this step.

Gate: automatic.

Retry policy: `max_attempts: 2`; `strategy: resume_then_narrow`.

Idempotent: true.

### 6. Validate static configuration and implementation scope

Executor: `orchestrator`. This step performs deterministic checks only and does not edit source files.

Depends on: steps 2, 3, 4, and 5.

Allowed files: none

Task: Validate the combined implementation locally without requiring an unidentified remote amd64 node or manual patch transfer. Parse `.devcontainer/devcontainer.json` as JSON, resolve Compose with local and default image variables, verify the Mac `pasta` network selection and socket variable contract, run separate shell syntax checks for every changed shell file, and run builder and relay dry-run checks. Run repository hooks only in a disposable copy or isolated temporary checkout; no fixer may target the workspace. Assert that the original workspace has no hash or path change caused by the hooks.

Assert that all implementation changes relative to the pre-dispatch baseline are limited to the union of steps 2-5 allowed files. Tolerate only the two explicitly recorded unrelated script changes. Assert that the primary's post-create disposition is clean and that prohibited workflow, preset, `.actrc`, config, and credential paths remain unchanged. Existing GitHub CI is the amd64 regression mechanism; do not create a branch, commit, push, transfer a patch, or build an amd64 image on an unavailable reference node.

Acceptance: static checks pass, the local image tags and Compose base/network arguments resolve deterministically, hook execution leaves the workspace unchanged, scope and prohibited-path assertions pass, and unrelated worktree changes remain untouched.

Verification: Run `python3 -m json.tool .devcontainer/devcontainer.json`, both Compose config resolutions, `bash -n` on `ci/entrypoint.sh`, `.devcontainer/ensure-repos.sh`, `.devcontainer/kilo-env.sh`, and `scripts/build-container-images.sh`, non-mutating hook validation in the disposable copy, relay and builder dry-run checks, and the path/hash manifest assertion. Any hook or check that modifies the workspace fails this step; do not restore an entire directory. Record before/after manifests and hook output without copying temporary evidence into the repository.

Gate: automatic.

Retry policy: `max_attempts: 1`; `strategy: abort`.

Idempotent: true.

### 7. Perform the native arm64 end-to-end acceptance run

Executor: `human`

Depends on: step 6.

Allowed files: none during validation; ignored local image, VM, cache, and build directories may change. Evidence is written only under `~/.phlex-devcontainer-tmp/plan-evidence/`.

Runner script: `native-arm64-devcontainer.sh`; phases: `preflight`, `verify-recorded-host`, `build-images`, `create-devcontainer`, `verify-runtime`, `verify-relay`, `verify`; log policy: `allowlisted`.

Task: Run the named runner phases in order. `preflight` checks the values file, native Podman tools, builder, Compose provider, VS Code/Dev Containers prerequisites, and writable evidence paths. `verify-recorded-host` sources the step-1 values, checks the machine, VM socket, provider/version, preferred gateway, and `--net=pasta` probe without changing any selected value. If any recorded host value is stale, stop and rerun step 1; do not choose a replacement address or gateway in step 7.

`build-images` runs the architecture-detecting builder with a separate arm64 cache and verifies the three local arm64 tags and architecture labels. Between `build-images` and `create-devcontainer`, start only the two named controlled test services if their reserved ports are free: `phlex-relay-test-model` listens on `127.0.0.1:25114` and returns a fixed non-secret HTTP JSON health response, and `phlex-relay-test-mcp` listens on `127.0.0.1:25300` and returns a fixed non-secret HTTP JSON health response. The runner owns their temporary process IDs and refuses to start over an occupied port; it never stops an existing process. The test mappings are `25114=25115,25300=25301`. Actual user services such as a local model on `11434` and HTTP MCP on `3000` are documented examples, not hidden prerequisites.

Export the recorded values plus `PHLEX_DEV_NETWORK_MODE=pasta`, `PHLEX_DEV_BASE_IMAGE=localhost/phlex-dev:arm64-local`, and `PHLEX_DEV_CONTAINER_IMAGE=localhost/phlex-devcontainer:arm64-local`, set the host VS Code `dev.containers.dockerPath` to `podman`, launch VS Code from that environment, and reopen the folder between named phases. `create-devcontainer` verifies the Compose service and VM-side socket mount. `verify-runtime` checks the compiler environment, native arm64 image, unchanged compiler-neutral default preset, trivial C++23 compile, configure/build, CTest, GCC override configure, and required developer tools. Network-backed AI availability is reported separately from image correctness.

`verify-relay` verifies the complete `127.0.0.1` to `socat` to the selected gateway path from inside the final pasta container, checks both controlled test services and the generated source-to-relay map, checks Kilo JSON and JSONC in-memory rewriting, makes non-secret health requests, and verifies that an unlisted host port is absent and unreachable. It verifies that stdio MCP and stdio LSP are treated as in-container processes. Optional nested act testing uses only the recorded VM-side Unix socket. Between `verify-relay` and final `verify`, stop only the two test services and relay processes owned by this gate.

Acceptance: `phlex-ci`, `phlex-dev`, and the final VS Code devcontainer run as native arm64 images; ordinary development uses Spack LLVM/Clang without changing `CMakePresets.json`; the GCC 15 headers, libraries, and runtime are selected; configure, build, CTest, and explicit `coverage-gcc` GCC-override configure succeed; the Podman machine remains rootless; optional API access uses only the verified VM-side Unix socket; no amd64 image is selected; required developer tools are usable; approved host TCP services are reachable only through explicit relay mappings; unlisted ports are not forwarded; and stdio MCP/LSP remain in-container.

Verification: The final `verify` phase reasserts every acceptance condition as individually labelled checks, captures image inspection, machine/socket state, compiler paths, CMake compiler identification, CTest results, relay maps, listener checks, negative unlisted-port checks, cleanup ownership, and final `git status --short`, writes the step receipt, and waits for explicit operator approval. Any failed build, relay, native-architecture, rootless, or socket check stops the gate and records the owning recovery action; approval requires a successful configure, build, and test run.

Gate: human approval after the runner's `verify` phase and evidence review.

Retry policy: `max_attempts: 1`; `strategy: abort`.

Idempotent: true.

## Outcome evidence

The goal is achieved only when evidence shows a native rootless arm64 Podman machine and exact pasta probes at both `podman run` and Compose levels, architecture-aware Spack concretization and local image construction, explicit local image selection, Spack LLVM/Clang as the developer default with unchanged CMake presets, a successful native arm64 devcontainer configure/build/test run, safe VM-side optional Podman access, unchanged CI/workflow paths, no GHCR push or required act emulation, and an explicit auditable host-service allowlist with negative unlisted-port evidence. Actual amd64 build regression evidence remains residual work for the eventual PR GitHub CI run.

## Failure recovery and post-execution review

A delegated-step failure is checkpointed with the changed-path manifest, attempt, verification output, and failure reason. Resume only the first incomplete dependency-ready step after narrowing its task. For an out-of-scope change, stop immediately, preserve pre-existing changes, identify the exact new path and hash, and require a targeted correction before resuming. For a human-gate failure, stop with the runner hash, failed phase receipt, log hash, exact operator action, and rerun phase sequence; never infer approval. After execution, classify the goal as `achieved`, `partially_achieved`, `not_achieved`, or `inconclusive`, record evidence and residual work, and recommend one of `none`, `revise_and_reexecute`, `create_supplemental_plan`, or `human_review`.

## Cost Estimate

This estimate is advisory. No defensible USD pricing input is available in the repository or current execution context, so dollar amounts are intentionally omitted rather than invented. The principal cost drivers are delegated implementation/review dispatches and native arm64 Spack source builds; configured retry limits bound dispatch count, while local build time and host resources are not converted into a fabricated dollar amount. Compilation must emit no `cost_estimate` object rather than inventing schema values.

## Architect review record

The architect review of the prior artifact reported 8 blocking findings and 7 non-blocking findings. The blocking findings were: oversized step-3 delegation; mutating hooks under an orchestrator with no allowed files; no disposition for the stale `post-create.sh` edit; undefined concretize environments and forbidden amd64 emulation; no Compose-level pasta probe; unnamed step-7 test services; contradictory step-1 remediation phases; and missing explicit numeric resource acceptance checks.

This revision resolves those findings by splitting relay/map work from Compose wiring, isolating hooks in a disposable copy, adding a guarded per-file post-create disposition, replacing amd64 concretization with a static branch assertion, adding a Compose pasta probe, naming controlled test services and ports with ownership cleanup, removing the contradictory remediation phase, and promoting all resource thresholds to individually verified acceptance conditions. It also records the non-blocking corrections: qualified skill-bundled runner tooling, exact step-id runner paths, explicit Linux bind preservation, deferred amd64 CI evidence, and no fabricated cost object.

New or changed implementation surfaces introduced by this revision are: `PHLEX_TEST_RELAY_PORTS` and the reserved test mappings `25114=25115,25300=25301`; the Compose-level temporary pasta probe; the disposable hook-validation copy; the primary's per-file post-create hash/disposition check; split allowed-file boundaries for steps 3 and 4; and step-7 runner/evidence paths. The complete touched-path set remains `ci/Dockerfile`, `ci/spack.yaml`, `ci/packages.yaml`, `ci/entrypoint.sh`, `.devcontainer/ensure-repos.sh`, `.devcontainer/kilo-env.sh`, `.devcontainer/Dockerfile`, `.devcontainer/docker-compose.yml`, `.devcontainer/devcontainer.json`, `scripts/build-container-images.sh`, `scripts/README.md`, and `docs/dev/podman-macos.md`. Prohibited paths remain unchanged and no config-protected path is assigned to a subagent.

The plan now has a clean source-lint result. This is a Plan self-review after the architect-driven revision; no second broad architect review is dispatched because the revision removes or narrows the reviewed mechanisms rather than introducing a new security boundary. A targeted human-triggered review is required if execution proposes any new host bind, socket source, provider mechanism, non-loopback listener, or protected path.
