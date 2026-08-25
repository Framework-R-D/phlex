# Plan: Native arm64 Podman Images and VS Code Devcontainers

Workspace root: `/Users/greenc/work/cet-is/sources/phlex/phlex`

## Goal

Make the repository's `phlex-ci`, `phlex-dev`, and VS Code devcontainer workflows usable from a rootless Podman machine on an Apple-Silicon Mac, using native `linux/arm64` execution and Spack-installed LLVM/Clang as the default development compiler. Preserve the existing amd64 GitHub CI/reference path, keep the generic CMake default preset compiler-neutral, and keep local arm64 images local-only.

## Recovery posture from the most recent execution

The most recent execution approved the native-arm64 host gate and dispatched steps 2 and 3 in parallel. Step 2 reported success and changed its four allowed CI files. Step 3 (the prior single relay/Kilo step) was manually aborted while exercising relay-map verification; its partial changes remain in `.devcontainer/ensure-repos.sh` and `.devcontainer/kilo-env.sh` and must be preserved for per-path review. No current evidence authorizes skipping either implementation step after the plan-family mismatch.

The compiled artifact changed during execution without a matching source revision: its declared source hash did not match the Markdown, its schema validation failed, and the checkpoint still described an earlier, differently-shaped artifact. This is a plan-integrity failure, not a resumable step result. The next compile must replace the stable artifact only after source lint passes and must archive the mismatched state. Execute must not resume or reset an invalid state. The primary must retain the step-2 and step-3 worktree changes, establish their provenance per path, and reverify them against the newly compiled artifact.

The manually aborted dispatch that touched both `.devcontainer/ensure-repos.sh` and `.devcontainer/kilo-env.sh` consumed one implementation attempt. This revision splits that prior single step into step 3a (`.devcontainer/ensure-repos.sh`) and step 3b (`.devcontainer/kilo-env.sh`). The consumed attempt is charged against **step 3a**, because the blocking defect that caused the abort (a top-level `local` outside a function) was in `ensure-repos.sh`; step 3a therefore has one attempt remaining out of `max_attempts: 2` at its first post-compilation dispatch. Step 3b starts with its full attempt budget, since no dispatch has targeted `kilo-env.sh` in isolation. Both steps must inspect the existing partial diff in their respective file before making further changes; neither may discard that diff without recording its pre-change hash and a reason.

The previous `.devcontainer/post-create.sh` concern is not present in the current partial diff. Before compilation, the primary still records its hash and confirms that it remains unchanged. No step may restore an entire file set after a failure because unrelated changes in `scripts/git-ai-commit` and `scripts/test/test_git_ai_commit.py` must remain untouched. A manually aborted implementation step is not successful evidence; after compilation, rerun the owning step with its attempt and partial-output history recorded.

## Reconciliation findings incorporated in this revision

The aborted step-3 implementation exposed requirements that were stated too generally for safe execution. This revision requires a macOS-portable listener check (`lsof` on Darwin and `ss` on Linux, with an explicit prerequisite failure when neither is available), fail-closed mapping validation, deterministic maps that contain only ready relays, listener-first `socat` address ordering, and PID-file cleanup that proves process ownership before killing anything. It also requires the Darwin VM-socket integrity guard to abort before Compose rather than warn and continue, and pins the exact read-only host Kilo path used by the in-memory rewrite. These are clarifications of the reviewed relay boundary, not permission to add a new host interface, socket source, wildcard listener, or protected path.

A second architect review of the first revision found 9 further blocking findings, all addressed in this Plan-finalization revision without a further architect dispatch (see the Architect review record for the complete resolution list): duplicate human-gate phase IDs; unreachable remediation phases; loss of Kilo agent/skill/command assets from a proposed dual-mount scheme; an undefined mechanism for the optional macOS nested-Podman socket; ambiguous Podman-socket-source semantics between Linux and Darwin; a delegated-verification write path that would hang on an unapproved directory; an unremediated `shellcheck` requirement; an oversized step 3 after its own abort; and undefined attempt accounting for that abort.

## Decisions and boundaries

- The Mac path is native arm64. It must not silently select `linux/amd64`, `x86_64_v3`, or an emulated Podman machine.
- Linux reference/build nodes are amd64-only, but this plan no longer performs a remote amd64 image build. Existing GitHub CI remains the amd64 regression path; this plan performs local static assertions for the amd64-compatible branch and records actual amd64 regression as deferred to the eventual PR CI run.
- `phlex-ci` remains GCC-default. `phlex-dev` and the locally built VS Code base image default to Spack's `clang` and `clang++`, with Clang explicitly directed to the Spack GCC 15 toolchain and libstdc++. The `default` CMake preset is unchanged. GCC-specific coverage remains available through an explicit GCC environment override.
- Existing GHCR amd64 images and GitHub workflow definitions are not changed, and no arm64 image is pushed to GHCR.
- Local VS Code selection uses explicit `PHLEX_DEV_BASE_IMAGE` and `PHLEX_DEV_CONTAINER_IMAGE` variables. The Compose default remains the pinned GHCR image for clean machines and Codespaces.
- Host TCP services are exposed through an explicit allowlist only. `PHLEX_HOST_RELAY_PORTS` uses comma-separated numeric `source=relay` entries, such as `11434=21434,3000=13000`; source and relay must differ, and malformed, duplicate, privileged, unavailable, or unlisted mappings fail closed without starting a relay. No arbitrary host-port scan or wildcard relay is allowed. Headroom's existing source variables and Linux behavior remain intact.
- Relay validation and Mac Compose acceptance use Podman's `--net=pasta` path with a host listener bound to `127.0.0.1`. Darwin listener detection uses `lsof`; Linux listener detection uses `ss`; neither implementation may scan a port range. Linux's preserved existing relay behavior may bind approved relay listeners to `0.0.0.0`; that exposure is explicitly limited to the numeric allowlist and is not used by the Darwin path. Default Podman networking, a host-interface bind, Thunderbolt Bridge, USB-LAN, and a manually assigned Mac address are not equivalent evidence and are out of scope.
- The selected gateway is the first resolving documented Podman alias, preferring `host.docker.internal` and otherwise using `host.containers.internal`. Record the hostname, not a resolved IP.
- `PHLEX_HOST_RELAY_BIND_ADDRESS`, firewall mode (b), `pfctl`, and `socketfilterfw` evidence are removed. macOS relays bind to `127.0.0.1`; the pasta path supplies container reachability and no wildcard host exposure is needed. Linux preserves its existing bind behavior.
- The relay scope is TCP/HTTP services. MCP servers and LSPs that communicate over stdio remain inside the devcontainer or use VS Code's remote extension process; host Unix sockets are outside this plan.
- Local `act` remains an optional amd64-emulation path. It is not part of native-arm64 acceptance and `.actrc` is not redesigned.
- No host machine is initialized, changed to rootful mode, or recreated automatically by repository scripts.
- `PHLEX_PODMAN_SOCKET_SOURCE` is always an absolute, existing Unix-socket path on the macOS or Linux host filesystem; it is never used as a bare identifier. `PHLEX_PODMAN_SOCKET_KIND` is the separate descriptive identifier, carrying `vm-rootless` (Darwin) or `linux-proxy` (Linux). On Linux, `PHLEX_PODMAN_SOCKET_SOURCE` is the existing `${XDG_RUNTIME_DIR:-/run/user/<uid>}/podman/podman.sock` path already used by `ensure-repos.sh`; this is unchanged from current behavior. On Darwin, `PHLEX_PODMAN_SOCKET_SOURCE` is the Podman machine's own locally-exposed connection socket, obtained from `podman machine inspect --format '{{.ConnectionInfo.PodmanSocket.Path}}'` — a genuine macOS-local Unix socket file that Podman itself creates and maintains, not a manually bridged path and not the macOS home-directory VM share. The separate `podman machine ssh test -S /run/user/<uid>/podman/podman.sock` check in step 1 is an integrity check that the VM-side rootless API backing that local socket is genuinely rootless and reachable; it is never itself used as a Compose mount source. The existing, already-safe `${HOME}/.podman-proxy/podman.sock` Compose mount (with its existing dummy-socket fallback when no real source exists) is retained unconditionally on both platforms as the single nested-Podman mechanism; no new conditional mount mechanism is introduced. Ordinary development does not invoke this optional nested-Podman path.
- The host Kilo directory `${HOME}/.config/kilo` is mounted read-only at the same in-container path it already uses, `/root/.config/kilo`, replacing the current read-write mount. This preserves in-container access to mounted agents, skills, and commands under that directory while satisfying the write-free requirement for Kilo configuration rewriting; there is no separate `/run/phlex-host-kilo` path. `${HOME}/.phlex-devcontainer-tmp/relays` is mounted read-only at `/run/phlex-host-relays`; this exact path and its two filenames (`relay-map.json`, `relay-map.env`) are the only additional container inputs for the in-memory Kilo rewrite.

## Repository baseline and change ownership

Implementation surfaces are `ci/Dockerfile`, `ci/spack.yaml`, `ci/packages.yaml`, `ci/entrypoint.sh`, `.devcontainer/ensure-repos.sh`, `.devcontainer/kilo-env.sh`, `.devcontainer/Dockerfile`, `.devcontainer/docker-compose.yml`, `.devcontainer/devcontainer.json`, `scripts/build-container-images.sh`, `scripts/README.md`, and `docs/dev/podman-macos.md`.

The following paths are permanently prohibited for delegated implementation steps: `.github/workflows/*`, `.actrc`, `CMakePresets.json`, `scripts/git-ai-commit`, `scripts/test/test_git_ai_commit.py`, `.devcontainer/post-create.sh`, all `.kilo/*`, `kilo.json`, and `AGENTS.md`. Compiler and checkpoint helpers may write only their designated plan-family artifacts through the primary execution path. The primary records the pre-dispatch status and hashes of the two unrelated script changes and the post-create disposition. After each delegated step, it asserts that only that step's allowed files changed relative to that baseline plus prior successful implementation changes. An out-of-scope path is a blocking failure; do not automatically revert it or clobber a pre-existing change.

## Human-gate evidence and runner contract

Human gates use the execution-plans human-gate runner contract. Execute materializes the named runner script as `/private/tmp/1787581717167-native-arm64-podman-devcontainers-<step-id>.sh`, validates it with the skill-bundled `execution-plans/scripts/validate_runner.py`, sets owner-only permissions, and records its hash, phase receipts, and log hash. The operator runs phases in order as child processes; no multiline shell transcript is pasted into an interactive shell. Every runner has `preflight` first and `verify` last, uses `run_check`, `finish`, `usage`, `set -uo pipefail`, `umask 077`, no `set -e`, and no credentials or model payloads in logs. Each named phase appears exactly once in a runner's phase list; a phase that must be repeated after remediation is described in prose as an operator-driven rerun, never as a duplicate phase-list entry.

Evidence is written outside the repository under `~/.phlex-devcontainer-tmp/plan-evidence/`. Step 1 owns `step-1.md` and `step-1-values.env`; step 7 owns `step-7.md`. Evidence records UTC time, host identity, allowlisted command/output fields, exit status, phase receipts, and the explicit approve or stop decision. Step 7 sources the values file and never re-derives gateway or socket values. Runner `log_policy` is `allowlisted`; credentials, tokens, model payloads, and raw authenticated responses are suppressed.

### Human operator command protocol

The operator's current working directory for every phase command is `/Users/greenc/work/cet-is/sources/phlex/phlex`. Execute each phase as a child process with `bash <runner> <phase>`; do not source the runner. The runner path is supplied by Execute and has a plan- and step-unique basename. Before the first phase, confirm that the runner is executable and that its reported SHA-256 matches the checkpoint. Do not run any phase if the runner hash, workspace root, or plan evidence directory does not match the checkpoint.

Every listed command must exit with status 0 and print the stated result. A nonzero status is an abort unless the immediately preceding phase explicitly identifies it as the one allowed remediation condition. An abort records the failed command and phase, runs no later phase, and does not receive human approval. After the final `verify` command, approve only when the runner reports zero failures, every required evidence file exists, and every command in the applicable list exited 0. The final decision is written to the step evidence before Execute resumes; silence or a partial log never counts as approval.

### Step 1 command list and failure routing

From the workspace root, run these commands in order. Expected results and abort rules are part of the gate:

1. `bash <step-1-runner> preflight` — exits 0 and reports the workspace, Podman, Compose provider, `socat`, `shellcheck`, platform listener tool, and evidence directory checks as passed. If it fails only because one of Compose, `socat`, `shellcheck`, or the platform listener tool is missing, continue with command 2; any other failure aborts step 1.
2. `bash <step-1-runner> remediate-tools` — run only after the missing-tool-only result from command 1. It exits 0, records the host package-policy action or an explicit no-op, and does not change Podman machine state. Any failure aborts step 1.
3. `bash <step-1-runner> preflight` — rerun after command 2. It must exit 0 with all prerequisite checks passed; otherwise abort step 1.
4. `bash <step-1-runner> inspect-host` — exits 0 only when the exact machine state is running, architecture is native arm64/aarch64, rootless mode is enabled, resources meet 8 vCPUs / 16 GiB / 100 GiB thresholds, and `podman machine ssh test -S /run/user/<uid>/podman/podman.sock` succeeds. If it fails only because the machine is absent or stopped, continue with command 5; any wrong state, architecture, rootfulness, resource, or socket-integrity result aborts step 1.
5. `bash <step-1-runner> remediate-host` — run only for the absent/stopped result from command 4. It exits 0 after initializing an absent machine with rootless mode, 8 CPUs, 16384 MiB memory, and 100 GiB disk, or starting a stopped machine without recreating or changing rootfulness. Any failure aborts step 1.
6. `bash <step-1-runner> inspect-host` — rerun after command 5. It must exit 0 with all exact host checks passed; otherwise abort step 1.
7. `bash <step-1-runner> probe-pasta` — exits 0 only after the native arm64 `docker.io/library/alpine:3.22` container using exact `--net=pasta` reaches the owned `127.0.0.1:25110` listener and prints `phlex-probe-ok`, with the selected documented gateway recorded. Any nonzero result aborts step 1; do not substitute bridge, host-interface, amd64, or emulated networking.
8. `bash <step-1-runner> probe-compose-pasta` — exits 0 only after the owned Compose project `phlex-pasta-probe` using `PHLEX_DEV_NETWORK_MODE=pasta` reaches the owned `127.0.0.1:25112` listener and records provider name/version. Any nonzero result aborts step 1.
9. `bash <step-1-runner> teardown-probes` — exits 0 only after stopping the named gate-owned probes/listeners and confirming ports 25110 and 25112 are free. A process not proven to be gate-owned must not be stopped; inability to complete owned cleanup aborts step 1.
10. `bash <step-1-runner> record-values` — exits 0 and writes only the required non-secret fields to `~/.phlex-devcontainer-tmp/plan-evidence/step-1-values.env`, including the local Podman-machine socket source, `vm-rootless` socket kind, gateway, provider/version, resource values, probe identity, probe ports, and `25114=25115,25300=25301` test mappings. Any missing, stale, or extra sensitive field aborts step 1.
11. `bash <step-1-runner> verify` — exits 0, reports zero failures, validates every acceptance condition and evidence receipt, and records the explicit `approve` or `stop` decision. Any nonzero result or missing acceptance evidence aborts step 1 and requires no downstream dispatch.

Do not run commands 2, 5, or any later phase unless its stated condition is met. Do not rerun a satisfied remediation phase. In-gate reruns of a failed check are permitted only after the named remediation or a diagnostic correction that does not alter the selected host values.

Final verification is successful only when command 11 exits 0 with zero failures, every acceptance condition and required evidence receipt is present, and the operator records `approve` before Execute resumes.

### Step 7 command list and failure routing

After step 6 is complete, use the same workspace root and the step-1 evidence. Before launching VS Code, confirm `~/.phlex-devcontainer-tmp/plan-evidence/step-1-values.env` exists and is the source of all recorded gateway and socket values. Export only the plan-specified non-secret values: `PHLEX_DEV_NETWORK_MODE=pasta`, `PHLEX_DEV_BASE_IMAGE=localhost/phlex-dev:arm64-local`, `PHLEX_DEV_CONTAINER_IMAGE=localhost/phlex-devcontainer:arm64-local`, `PHLEX_TEST_RELAY_PORTS=25114=25115,25300=25301`, and `PHLEX_HOST_RELAY_PORTS=25114=25115,25300=25301`. Configure the host VS Code Dev Containers setting `dev.containers.dockerPath` to `podman`, then launch VS Code from the workspace root with `code --reuse-window .`.

Run these commands in order. Every command must exit 0; any nonzero result aborts step 7, except that stale recorded host values require stopping and rerunning step 1 rather than selecting replacements:

1. `bash <step-7-runner> preflight` — reports the step-1 values file, native Podman tools, local image builder, Compose provider, VS Code/Dev Containers prerequisites, and writable evidence paths as passed. Failure aborts step 7.
2. `bash <step-7-runner> verify-recorded-host` — exits 0 only when the recorded machine, local socket, VM-side rootless socket, provider/version, gateway, and pasta probe still match without re-derivation. A stale value aborts step 7 and routes back to step 1.
3. `bash <step-7-runner> build-images` — exits 0 only after the builder uses a separate arm64 cache and verifies native arm64 `localhost/phlex-ci:arm64-local`, `localhost/phlex-dev:arm64-local`, and `localhost/phlex-devcontainer:arm64-local` tags. Failure aborts step 7; do not retry with amd64 or emulation.
4. `bash <step-7-runner> create-devcontainer` — exits 0 only after the VS Code devcontainer is created or reopened with the local arm64 images, `pasta` network mode, read-only `/root/.config/kilo` and `/run/phlex-host-relays` mounts, and the existing nested-Podman socket mount. An occupied port or non-owned process must not be stopped; failure aborts step 7.
5. `bash <step-7-runner> verify-runtime` — exits 0 only after native image inspection, Spack Clang default selection, GCC 15 header/runtime probes, unchanged compiler-neutral presets, C++23 compilation, configure, build, CTest, explicit GCC `coverage-gcc` override configure, and required developer-tool checks pass. Any failed build or test aborts step 7.
6. `bash <step-7-runner> verify-relay` — exits 0 only after controlled services and owned relays pass positive health checks through the pasta path, generated maps are deterministic, JSON and JSONC Kilo rewriting is in-memory and map-driven, unlisted ports are absent/unreachable, and stdio MCP/LSP remain in-container. Any relay, map, negative-port, or process-boundary failure aborts step 7.
7. `bash <step-7-runner> verify` — exits 0, reports zero failures, writes `~/.phlex-devcontainer-tmp/plan-evidence/step-7.md`, records image/compiler/build/test/relay hashes and final `git status --short`, and records the explicit final approval. Any nonzero result or missing evidence aborts step 7.

Before final approval, stop only the two controlled test services and relay processes owned by step 7, then rerun command 7 if cleanup is part of the runner's final verification. Final verification is successful only when all listed commands exited 0, the step-7 receipt says `approve`, and no prohibited repository path changed.

## Execution limits and safety

- `max_total_dispatches`: 18
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

Runner script: `native-arm64-podman-host.sh`; phases: `preflight`, `remediate-tools`, `inspect-host`, `remediate-host`, `probe-pasta`, `probe-compose-pasta`, `teardown-probes`, `record-values`, `verify`; log policy: `allowlisted`.

Task: Run the named runner phases in order, with explicit failure routing. `preflight` checks the workspace root, Podman, a Compose provider, `socat`, `shellcheck`, `lsof` on Darwin or `ss` on Linux, and writable evidence directories. If `preflight` fails only because Compose, `socat`, `shellcheck`, or the listener-detection tool is missing, run `remediate-tools` once through the host package-management policy, then rerun `preflight`; if it fails for any other reason, stop. `inspect-host` checks that the Podman machine is running (exact state match, not a substring), native arm64/aarch64 (exact architecture field), rootless, has at least 8 vCPUs, at least 16 GiB of VM memory, at least 100 GiB of free VM disk, and that `podman machine ssh test -S /run/user/<uid>/podman/podman.sock` confirms the rootless VM socket as an integrity check. If `inspect-host` fails only because the machine is absent or stopped, run `remediate-host` once — initializing it with rootless mode, 8 CPUs, 16384 MiB memory, and 100 GiB disk if absent, or simply starting it without recreating or changing rootfulness if stopped — then rerun `inspect-host`; if it fails for any other reason (wrong state, wrong architecture, rootful, a resource threshold, or a missing VM socket), stop. Neither remediation phase performs any mutation when its triggering condition is already satisfied; each records a passed no-op in that case. No interactive setup occurs outside these two named phases, and an in-gate phase rerun before an approve/stop decision does not consume the step attempt.

`probe-pasta` reserves `127.0.0.1:25110`, starts an allowlisted listener owned by this gate, and runs the approved native arm64 `docker.io/library/alpine:3.22` probe with Podman's exact `--net=pasta` option in the named container `phlex-pasta-probe`. The probe resolves both documented gateway hostnames, connects through the selected hostname, and requires `phlex-probe-ok`. It does not use a pinned amd64 image, a bridge network, a host-interface address, or repository mounts. Select `PHLEX_HOST_GATEWAY` by preferring a resolving `host.docker.internal` and otherwise using `host.containers.internal`. `probe-compose-pasta` writes a temporary Compose project owned by this gate, uses project name `phlex-pasta-probe`, reserves `127.0.0.1:25112`, and uses the selected Compose provider and `PHLEX_DEV_NETWORK_MODE=pasta` to repeat the listener probe through a minimal temporary service. It records the provider name and version and fails if Compose does not actually create the pasta network path. `teardown-probes` stops only the named gate-owned container, Compose project, and listener PIDs recorded by this gate, then verifies that ports 25110 and 25112 are free. `record-values` writes the selected gateway, provider/version, `PHLEX_PODMAN_SOCKET_SOURCE` (the Podman machine's own local `ConnectionInfo.PodmanSocket.Path`, obtained via `podman machine inspect`, never the VM-internal path), `PHLEX_PODMAN_SOCKET_KIND=vm-rootless`, `PHLEX_TEST_RELAY_PORTS=25114=25115,25300=25301`, and approved relay mappings to the owned values file.

Acceptance:

- The machine is running (exact state match), native arm64/aarch64 (exact architecture match), rootless, and has a real VM Unix socket confirmed by `podman machine ssh test -S`.
- The machine has at least 8 vCPUs, at least 16 GiB of VM memory, and at least 100 GiB of free VM disk.
- Compose, `socat`, and `shellcheck` are available; the Darwin/Linux listener-detection tool is available.
- The exact `--net=pasta` arm64 probe reaches the `127.0.0.1` listener and prints `phlex-probe-ok`.
- The selected Compose provider, with `PHLEX_DEV_NETWORK_MODE=pasta`, reaches the same listener from a temporary service.
- The selected gateway hostname, provider/version, local Podman-machine socket path, confirmed VM-side socket integrity, test relay mappings, approved mappings, machine resources, probe image, and probe ports are recorded.
- No rootful service, TCP Podman API, manually assigned interface address, Thunderbolt Bridge, USB-LAN, or amd64/emulated probe is used.

Verification: Run `verify` after the complete phase sequence. It rechecks every acceptance condition as individually labelled checks, including exact state/architecture matching, each numeric resource threshold, exact VM-socket integrity, listener-tool and `shellcheck` capability, and Compose-level pasta reachability; verifies the values file contains only required non-secret fields; writes phase receipts for both no-op and performed remediation; and waits for the operator's explicit approve or stop decision. On failure, stop and record the failed phase and recovery action; do not infer approval or consume a second step attempt for an in-gate phase rerun.

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

### 3a. Implement the allowlisted host relay contract in `ensure-repos.sh`

Executor: `coder-qwen`. This is a narrow bounded subagent task; the primary must enforce the allowed-file boundary and the baseline scope assertion.

Depends on: step 1.

Allowed files: `.devcontainer/ensure-repos.sh`.

Task: Preserve Linux behavior, including its existing Headroom source variables, existing relay bind behavior, and existing `${XDG_RUNTIME_DIR:-/run/user/<uid>}/podman/podman.sock` real-socket source, while adding the Darwin `--net=pasta` path. Refactor only the relay-management logic in this file. Parse exact comma-separated numeric `source=relay` entries from `PHLEX_HOST_RELAY_PORTS`; validate the complete set before starting anything; reject malformed, duplicate, privileged, source-equals-relay, unavailable, or unlisted mappings with a nonzero result; and write deterministic empty maps before returning failure. Detect a listening loopback source with `lsof -nP -a -iTCP:<source> -sTCP:LISTEN` on Darwin or an equivalent exact-port `ss` query on Linux, never a port-range scan. Start `socat` with the relay listener address first and the loopback source connection second, bind Darwin relays to `127.0.0.1`, and record a mapping only after the relay listener is ready. Use per-relay PID files containing the owned PID and command identity; on rerun, remove only stale processes whose recorded command identity matches, and never use `pkill -f` for these relays. Keep those relays alive after `initializeCommand` exits: do not register an exit trap that kills successful relays; the final human gate (step 7) owns their teardown, while each rerun cleans only verified stale owned PIDs. Always write sorted empty or populated JSON and shell-environment maps to `${PHLEX_RELAY_STATE_DIR:-$HOME/.phlex-devcontainer-tmp/relays}` before Compose starts; never relay wildcard traffic.

Add a `PHLEX_RELAY_STATE_DIR` override (defaulting to `$HOME/.phlex-devcontainer-tmp/relays` when unset) that this script uses for every relay map, environment-map, and PID-file path. This script's own delegated verification in this step must set `PHLEX_RELAY_STATE_DIR=/private/tmp/1787581717167-native-arm64-podman-devcontainers-step-3a-relaytest` (an approved external-directory path) for every automated test invocation and must never write under `~/.phlex-devcontainer-tmp` during that verification; production runs (steps 1, 6, and 7) use the unmodified default path.

On Darwin, source the step-1 values file before Compose preparation. Set `PHLEX_PODMAN_SOCKET_SOURCE` to the recorded local Podman-machine `ConnectionInfo.PodmanSocket.Path` value (never re-derive it) and `PHLEX_PODMAN_SOCKET_KIND=vm-rootless`, then perform the integrity check `podman machine ssh test -S /run/user/<uid>/podman/podman.sock` and abort before Compose if the recorded machine, the recorded socket source, the VM-side integrity check, or a required listener-detection tool is missing. Do not warn and continue with a missing socket. On Linux, keep the existing proxy path and behavior entirely unchanged, and set `PHLEX_PODMAN_SOCKET_KIND=linux-proxy` alongside the existing `PHLEX_PODMAN_SOCKET_SOURCE` value.

Acceptance: Linux retains its existing relay behavior and existing socket-source path; Darwin requires no `systemctl`, Linux runtime socket path, root privilege, host-interface bind, or TCP Podman API; invalid mappings fail closed; only explicitly listed ready listening source ports are relayed; maps are deterministic and generated even when empty; stale cleanup is limited to owned relay processes; and the Darwin VM-socket integrity guard aborts before Compose when invalid.

Prohibited changes: Do not modify `.devcontainer/kilo-env.sh` or any other prohibited path, especially `.devcontainer/post-create.sh`; do not stage, commit, push, mutate the machine, expose TCP Podman, scan ports, relay wildcard traffic, or write under `~/.phlex-devcontainer-tmp` during automated verification.

Verification: Run `bash -n` and `shellcheck` on the file and a Darwin/Linux listener-tool capability check. Exercise valid, duplicate, malformed, privileged, source-equals-relay, unavailable, and unlisted mappings without external services, using `PHLEX_RELAY_STATE_DIR` pointed at the step's approved external directory; assert invalid sets return nonzero and leave deterministic empty maps; run twice with a controlled loopback listener to prove cleanup is limited to owned relays; inspect sorted populated and empty maps; verify a failed relay is absent from both maps; exercise the Darwin socket guard with valid, missing, and non-socket sources. Run the baseline scope assertion. Do not edit `.devcontainer/kilo-env.sh` or `.devcontainer/post-create.sh`.

Gate: automatic.

Retry policy: `max_attempts: 2`; `strategy: resume_then_narrow`.

Idempotent: true.

### 3b. Implement in-memory Kilo configuration rewriting in `kilo-env.sh`

Executor: `coder-qwen`. This is a narrow bounded subagent task; the primary must enforce the allowed-file boundary and the baseline scope assertion.

Depends on: step 1 and step 3a.

Allowed files: `.devcontainer/kilo-env.sh`.

Task: Keep Kilo rewriting write-free. Discover `/root/.config/kilo/kilo.json` or `/root/.config/kilo/kilo.jsonc` (the read-only host Kilo mount defined in step 4), and separately read the relay map at `/run/phlex-host-relays/relay-map.json` (or `relay-map.env`) produced by step 3a. Parse the discovered Kilo configuration with a named in-memory JSONC helper that removes comments and trailing commas only outside quoted strings and fails closed on malformed input. Rewrite only approved `http` or `https` URLs whose host is `127.0.0.1`, `localhost`, or `[::1]` and whose source port appears in the generated map, preserving the original URL scheme and all non-loopback URLs unchanged. Apply the source-to-relay map in memory and export `KILO_CONFIG_CONTENT`; the Kilo CLI consumes this environment value in preference to the mounted file when both are present. Never write or back up the host mount and never edit `.devcontainer/post-create.sh`. Generic HTTP MCP, local-model, indexer, and TCP-LSP clients receive the gateway and maps for manual configuration. Stdio MCP, stdio LSP, and host Unix sockets are not tunneled.

This script's own delegated verification in this step must read its relay-map fixtures from `/private/tmp/1787581717167-native-arm64-podman-devcontainers-step-3b-relaytest` (an approved external-directory path) rather than `~/.phlex-devcontainer-tmp` or `/run/phlex-host-relays`, since neither is populated outside a running devcontainer.

Acceptance: Kilo rewriting is in-memory, scheme-preserving, and map-driven; non-loopback URLs and URLs whose port is not in the map are unchanged; malformed host configuration fails closed without exporting a partial `KILO_CONFIG_CONTENT`; and the host mount is never written or backed up.

Prohibited changes: Do not modify `.devcontainer/ensure-repos.sh` or any other prohibited path, especially `.devcontainer/post-create.sh`; do not stage, commit, push, mutate the machine, or write host Kilo configuration.

Verification: Run `bash -n` and `shellcheck` on the file. Test JSON and JSONC discovery against fixture copies at the step's approved external directory; verify loopback-only, scheme-preserving rewrites and unchanged non-loopback URLs; verify fail-closed behavior on malformed JSON/JSONC input. Run the baseline scope assertion. Do not edit `.devcontainer/ensure-repos.sh` or `.devcontainer/post-create.sh`.

Gate: automatic.

Retry policy: `max_attempts: 2`; `strategy: resume_then_narrow`.

Idempotent: true.

### 4. Wire Compose, socket access, and local image selection

Executor: `coder-qwen`. This is a narrow bounded subagent task; the primary must enforce the allowed-file boundary and the baseline scope assertion.

Depends on: step 1, step 3a, and step 3b.

Allowed files: `.devcontainer/Dockerfile`, `.devcontainer/docker-compose.yml`, `.devcontainer/devcontainer.json`.

Task: Add the Compose-selectable `PHLEX_DEV_NETWORK_MODE`, with the Mac workflow resolving to `pasta` and the existing provider-compatible default retained elsewhere. Verify that the chosen Compose provider emits the equivalent Podman network mode. Keep the existing, already-unconditional `${HOME}/.podman-proxy/podman.sock:/tmp/podman.sock` mount and its existing dummy-socket fallback in `ensure-repos.sh` as the single nested-Podman mechanism on both platforms; do not add a new conditional mount mechanism. Change the existing `${HOME}/.config/kilo:/root/.config/kilo:Z` mount from read-write to read-only (`:ro,Z`), keeping the same in-container path so mounted agents, skills, and commands remain readable. Add `${HOME}/.phlex-devcontainer-tmp/relays:/run/phlex-host-relays:ro` and expose `PHLEX_HOST_GATEWAY`, `PHLEX_HOST_RELAY_FILE`, `PHLEX_HOST_RELAYS_ENV`, `PHLEX_PODMAN_SOCKET_SOURCE`, and `PHLEX_PODMAN_SOCKET_KIND` as Compose environment values. Do not mount a Mac home-share API socket, derive a Mac socket from `XDG_RUNTIME_DIR`, call `systemctl`, expose TCP Podman, or change machine rootfulness.

Change the devcontainer Dockerfile and Compose variables for the pinned GHCR base, local base image, and distinct final image tag. Keep credential mounts and the existing rootless volume layout otherwise unchanged. Do not make `post-create.sh` part of the solution.

Acceptance: the local arm64 variables resolve to `localhost/phlex-dev:arm64-local` and `localhost/phlex-devcontainer:arm64-local` without overwriting GHCR references; the Mac Compose service resolves `pasta`; Linux and clean-machine defaults remain usable; the Kilo mount at `/root/.config/kilo` is read-only; the relay-map mount resolves exactly to `/run/phlex-host-relays:ro`; and the Compose environment exposes distinct socket-source and socket-kind values.

Prohibited changes: Do not modify any prohibited path, stage, commit, push, mutate the machine, expose TCP Podman, write host Kilo configuration, or change the relay contract implemented in steps 3a and 3b.

Verification: Parse `devcontainer.json` with `python3 -m json.tool`, resolve Compose with local and default variables, inspect the resolved network mode and socket-source/socket-kind values, validate that the Kilo mount is read-only at its existing path and the relay-map mount is read-only at `/run/phlex-host-relays`, assert that no Mac home-share socket or XDG-derived socket is mounted, and run the baseline scope assertion. Container-side reachability is deferred to step 7.

Gate: automatic.

Retry policy: `max_attempts: 2`; `strategy: resume_then_narrow`.

Idempotent: true.

### 5. Add an architecture-detecting local image builder and Mac instructions

Executor: `coder-qwen`. This is a bounded subagent implementation task; the primary must enforce the allowed-file boundary and the baseline scope assertion.

Depends on: step 4.

Allowed files: `scripts/build-container-images.sh`, `docs/dev/podman-macos.md`, `scripts/README.md`.

Task: Add an executable repeatable builder that queries the Podman backend architecture, maps amd64 to `x86_64_v3` and arm64 to `aarch64`, refuses unsupported or emulated native-arm64 requests, builds `ci` and `dev` from the correct context with Docker format, and tags only `localhost/phlex-ci:<arch>-local`, `localhost/phlex-dev:<arch>-local`, and `localhost/phlex-devcontainer:<arch>-local`. Build the final layer from the local dev base with the explicit architecture and network variables. Select verified arm64 act and tracebox artifacts or pass explicit opt-outs. Separate an optional arm64 Spack cache from any amd64 cache and preserve optional GPG signing without keys in the repository or output. Never push images or mutate the machine.

Document the complete Mac workflow: native rootless machine and numeric resource gate; Compose, `socat`, VM socket, and preferred gateway checks; the exact `--net=pasta` Compose path; the controlled relay test services; `PHLEX_HOST_RELAY_PORTS='11434=21434,3000=13000'` as a user configuration example; `PHLEX_DEV_NETWORK_MODE=pasta`; local image and socket variables; terminal-launched VS Code with `dev.containers.dockerPath=podman`; devcontainer reopen; generated map contract; how an ordinary user stops relays and test services; stdio limitations; first-listener firewall prompt handling without claiming firewall restriction is required; separate cache and compiler-directory guidance; optional amd64-emulation act; and local-only image policy. Link the guide from `scripts/README.md`.

Acceptance: the builder produces all three local arm64 tags on a native arm64 backend and corresponding amd64 tags on an amd64 backend without workflow changes. The instructions are sufficient without reconstructing omitted values, and all relay state remains under `~/.phlex-devcontainer-tmp` in production use.

Prohibited changes: Do not modify any prohibited path, stage, commit, push, mutate the machine, reuse an amd64 cache for arm64, place GPG keys in the repository or output, or generate repository-local relay state.

Verification: Run shell syntax checks, relay-only dry-run checks, builder architecture and dry-run checks, documentation link checks, and the baseline scope assertion. Do not start a full package build in this step.

Gate: automatic.

Retry policy: `max_attempts: 2`; `strategy: resume_then_narrow`.

Idempotent: true.

### 6. Validate static configuration and implementation scope

Executor: `orchestrator`. This step performs deterministic checks only and does not edit source files.

Depends on: step 2, step 3a, step 3b, step 4, and step 5.

Allowed files: none

Task: Validate the combined implementation locally without requiring an unidentified remote amd64 node or manual patch transfer. Parse `.devcontainer/devcontainer.json` as JSON, resolve Compose with local and default image variables, verify the Mac `pasta` network selection and socket-source/socket-kind contract, assert the read-only Kilo mount at its existing path and the read-only `/run/phlex-host-relays` mount, run separate shell syntax and listener-tool capability checks for every changed shell file, and run builder and relay dry-run checks using `PHLEX_RELAY_STATE_DIR=/private/tmp/1787581717167-native-arm64-podman-devcontainers-step-6-relaytest` (an approved external-directory path, never `~/.phlex-devcontainer-tmp`). Run repository hooks only in `/private/tmp/1787581717167-native-arm64-podman-devcontainers-step-6-hookcheck/`, using a disposable copy; no fixer may target the workspace. Assert that the original workspace has no hash or path change caused by the hooks.

Assert that all implementation changes relative to the pre-dispatch baseline are limited to the union of step 2, step 3a, step 3b, step 4, and step 5 allowed files. Tolerate only the two explicitly recorded unrelated script changes at their pre-dispatch hashes; a further change to either introduced during execution is a stop, not a tolerated diff. Assert that the primary's post-create disposition is clean and that prohibited workflow, preset, `.actrc`, config, and credential paths remain unchanged. Existing GitHub CI is the amd64 regression mechanism; do not create a branch, commit, push, transfer a patch, or build an amd64 image on an unavailable reference node.

Acceptance: static checks pass, the local image tags and Compose base/network arguments resolve deterministically, hook execution leaves the workspace unchanged, scope and prohibited-path assertions pass, and unrelated worktree changes remain untouched at their recorded hashes.

Verification: Run `python3 -m json.tool .devcontainer/devcontainer.json`, both Compose config resolutions, `bash -n` and `shellcheck` on `ci/entrypoint.sh`, `.devcontainer/ensure-repos.sh`, `.devcontainer/kilo-env.sh`, and `scripts/build-container-images.sh`, assert the exact read-only Kilo/relay mounts and Darwin socket guard, run non-mutating hook validation in `/private/tmp/1787581717167-native-arm64-podman-devcontainers-step-6-hookcheck/`, execute the complete relay negative/positive dry-run matrix (using the step's approved external relay-state directory) including deterministic map hashes and owned-PID cleanup, run builder dry-run checks, and perform the path/hash manifest assertion. Any hook or check that modifies the workspace fails this step; do not restore an entire directory. Record before/after manifests and hook output without copying temporary evidence into the repository.

Gate: automatic.

Retry policy: `max_attempts: 1`; `strategy: abort`.

Idempotent: true.

### 7. Perform the native arm64 end-to-end acceptance run

Executor: `human`

Depends on: step 6.

Allowed files: none during validation; ignored local image, VM, cache, and build directories may change. Evidence is written only under `~/.phlex-devcontainer-tmp/plan-evidence/`.

Runner script: `native-arm64-devcontainer.sh`; phases: `preflight`, `verify-recorded-host`, `build-images`, `create-devcontainer`, `verify-runtime`, `verify-relay`, `verify`; log policy: `allowlisted`.

Task: Run the named runner phases in order. `preflight` checks the values file, native Podman tools, builder, Compose provider, VS Code/Dev Containers prerequisites, and writable evidence paths. `verify-recorded-host` sources the step-1 values, checks the machine, the recorded local Podman-machine socket path, the VM-side socket integrity check, provider/version, preferred gateway, and `--net=pasta` probe without changing any selected value. If any recorded host value is stale, stop and rerun step 1; do not choose a replacement address, gateway, or socket path in step 7.

`build-images` runs the architecture-detecting builder with a separate arm64 cache and verifies the three local arm64 tags and architecture labels. Between `build-images` and `create-devcontainer`, start only the two named controlled test services if their reserved ports are free: `phlex-relay-test-model` listens on `127.0.0.1:25114` and returns a fixed non-secret HTTP JSON health response, and `phlex-relay-test-mcp` listens on `127.0.0.1:25300` and returns a fixed non-secret HTTP JSON health response. The runner owns their temporary process IDs and refuses to start over an occupied port; it never stops an existing process. The test mappings are `25114=25115,25300=25301`; export both `PHLEX_TEST_RELAY_PORTS` and `PHLEX_HOST_RELAY_PORTS` to this exact value before Compose starts so `ensure-repos.sh` (step 3a) consumes the recorded mappings. Actual user services such as a local model on `11434` and HTTP MCP on `3000` are documented examples, not hidden prerequisites.

Export the recorded values plus `PHLEX_DEV_NETWORK_MODE=pasta`, `PHLEX_DEV_BASE_IMAGE=localhost/phlex-dev:arm64-local`, and `PHLEX_DEV_CONTAINER_IMAGE=localhost/phlex-devcontainer:arm64-local`, set the host VS Code `dev.containers.dockerPath` to `podman`, launch VS Code from that environment, and reopen the folder between named phases. `create-devcontainer` verifies the Compose service, the read-only Kilo/relay mounts, and the nested-Podman socket mount. `verify-runtime` checks the compiler environment, native arm64 image, unchanged compiler-neutral default preset, trivial C++23 compile including `__GLIBCXX__` and `-print-file-name=libstdc++.so` resolution under the Spack view, configure/build, CTest, GCC override configure, and required developer tools. Network-backed AI availability is reported separately from image correctness.

`verify-relay` verifies the complete `127.0.0.1` to `socat` to the selected gateway path from inside the final pasta container, checks both controlled test services and the generated source-to-relay map, checks Kilo JSON and JSONC in-memory rewriting, makes non-secret health requests, and verifies that an unlisted host port is absent and unreachable. It verifies that stdio MCP and stdio LSP are treated as in-container processes. Optional nested act testing uses only the existing nested-Podman socket mechanism. Between `verify-relay` and final `verify`, stop only the two test services and relay processes owned by this gate.

Acceptance: `phlex-ci`, `phlex-dev`, and the final VS Code devcontainer run as native arm64 images; ordinary development uses Spack LLVM/Clang without changing `CMakePresets.json`; the GCC 15 headers, libraries, and runtime are selected; configure, build, CTest, and explicit `coverage-gcc` GCC-override configure succeed; the Podman machine remains rootless; optional API access uses only the existing nested-Podman socket mechanism; no amd64 image is selected; required developer tools are usable; approved host TCP services are reachable only through explicit relay mappings; unlisted ports are not forwarded; and stdio MCP/LSP remain in-container.

Verification: The final `verify` phase reasserts every acceptance condition as individually labelled checks, captures image inspection, machine/socket state, compiler paths, CMake compiler identification, the `__GLIBCXX__` and Spack `libstdc++.so` probe, CTest results, relay maps (with a recorded SHA-256), listener checks, negative unlisted-port checks, cleanup ownership, the read-only Kilo/relay mounts, and final `git status --short`, writes the step receipt, and waits for explicit operator approval. Any failed build, relay, native-architecture, rootless, or socket check stops the gate and records the owning recovery action; approval requires a successful configure, build, and test run.

Gate: human approval after the runner's `verify` phase and evidence review.

Retry policy: `max_attempts: 1`; `strategy: abort`.

Idempotent: true.

## Outcome evidence

The goal is achieved only when evidence shows a native rootless arm64 Podman machine and exact pasta probes at both `podman run` and Compose levels, architecture-aware Spack concretization and local image construction, explicit local image selection, Spack LLVM/Clang as the developer default with unchanged CMake presets, a successful native arm64 devcontainer configure/build/test run, safe optional nested-Podman access, unchanged CI/workflow paths, no GHCR push or required act emulation, and an explicit auditable host-service allowlist with negative unlisted-port evidence. Actual amd64 build regression evidence remains residual work for the eventual PR GitHub CI run.

## Failure recovery and post-execution review

A delegated-step failure is checkpointed with the changed-path manifest, attempt, verification output, and failure reason. A manually aborted step is recorded as failed or stopped with its partial output hashes; it is never treated as successful. Resume only the first incomplete dependency-ready step after narrowing its task, and only against a source-matching compiled artifact. For an out-of-scope change, stop immediately, preserve pre-existing changes, identify the exact new path and hash, and require a targeted correction before resuming. For an invalid or externally changed artifact, stop, preserve the worktree, compile a reconciled source revision, archive the mismatched state, and do not reset the old artifact. For a human-gate failure, stop with the runner hash, failed phase receipt, log hash, exact operator action, and rerun phase sequence; never infer approval. After execution, classify the goal as `achieved`, `partially_achieved`, `not_achieved`, or `inconclusive`, record evidence and residual work, and recommend one of `none`, `revise_and_reexecute`, `create_supplemental_plan`, or `human_review`.

## Cost Estimate

This estimate is advisory. No defensible USD pricing input is available in the repository or current execution context, so dollar amounts are intentionally omitted rather than invented. The principal cost drivers are delegated implementation/review dispatches and native arm64 Spack source builds; configured retry limits bound dispatch count, while local build time and host resources are not converted into a fabricated dollar amount. Compilation must emit no `cost_estimate` object rather than inventing schema values.

## Architect review record

**First review** (prior artifact) reported 8 blocking findings: oversized step-3 delegation; mutating hooks under an orchestrator with no allowed files; no disposition for the stale `post-create.sh` edit; undefined concretize environments and forbidden amd64 emulation; no Compose-level pasta probe; unnamed step-7 test services; contradictory step-1 remediation phases; and missing explicit numeric resource acceptance checks. The first revision resolved all 8 by splitting relay/map work from Compose wiring, isolating hooks in a disposable copy, adding a guarded per-file post-create disposition, replacing amd64 concretization with a static branch assertion, adding a Compose pasta probe, naming controlled test services and ports with ownership cleanup, removing the contradictory remediation phase, and promoting all resource thresholds to individually verified acceptance conditions.

**Second review** (first revision) reported 9 further blocking findings, all resolved in this Plan-finalization revision without a further architect dispatch:

1. Duplicate human-gate phase IDs — resolved by listing each step-1 phase exactly once (`preflight`, `remediate-tools`, `inspect-host`, `remediate-host`, `probe-pasta`, `probe-compose-pasta`, `teardown-probes`, `record-values`, `verify`) with reruns described in prose only.
2. Unreachable remediation phases — resolved by explicit failure routing: a tool-only `preflight` failure routes to `remediate-tools` then reruns `preflight`; a machine-only `inspect-host` failure routes to `remediate-host` then reruns `inspect-host`.
3. Loss of Kilo agent/skill/command assets — resolved by mounting the existing host Kilo directory read-only at its existing in-container path (`/root/.config/kilo`) instead of a separate, narrower `/run/phlex-host-kilo` path; all previously-readable assets remain readable.
4. Undefined nested-Podman socket mechanism — resolved by keeping the existing, already-unconditional `${HOME}/.podman-proxy/podman.sock` Compose mount (with its existing dummy-socket fallback) as the single mechanism on both platforms, and by correctly defining `PHLEX_PODMAN_SOCKET_SOURCE` on Darwin as the Podman machine's own local `ConnectionInfo.PodmanSocket.Path` rather than an unreachable VM-internal path.
5. Ambiguous socket-source semantics — resolved by the same correction: `PHLEX_PODMAN_SOCKET_SOURCE` is always an absolute local path; `PHLEX_PODMAN_SOCKET_KIND` is the separate descriptive identifier; Linux's existing path is explicitly preserved unchanged.
6. Delegated writes to an unapproved directory — resolved by a `PHLEX_RELAY_STATE_DIR` override consumed by `ensure-repos.sh`, with every delegated verification step directed to its own `/private/tmp/1787581717167-native-arm64-podman-devcontainers-step-<id>-relaytest` directory instead of `~/.phlex-devcontainer-tmp`.
7. Unremediated `shellcheck` requirement — resolved by adding `shellcheck` to step 1's `preflight` checks, `remediate-tools` install list, and acceptance conditions, and to every step's shell verification.
8. Oversized step 3 after its own abort — resolved by splitting it into step 3a (`ensure-repos.sh`, relay contract) and step 3b (`kilo-env.sh`, Kilo rewrite), with 3b depending on 3a and consuming its map contract; step 4's and step 6's dependencies are updated accordingly.
9. Undefined attempt accounting for the aborted dispatch — resolved by the explicit statement in the Recovery posture section that the consumed attempt is charged against step 3a.

New or changed surfaces introduced by this finalization revision: the `PHLEX_RELAY_STATE_DIR` override (test-only, defaulting to the unchanged production path, values confined to already-approved `/private/tmp/*` directories); the corrected, non-VM-internal definition of `PHLEX_PODMAN_SOCKET_SOURCE` on Darwin (reusing Podman's own existing local connection socket, not a new bridging mechanism); the single read-only Kilo mount at its existing path (a reduction in mount count, not an addition); the split of step 3 into step 3a and step 3b (a plan-structure change, not a new technical mechanism); and `max_total_dispatches` increased from 16 to 18 to absorb the added step. None of these introduces a new host bind, socket source, provider mechanism, non-loopback listener, or protected path beyond what the first and second reviews already examined; each is either a narrowing of an existing mechanism or a corrected definition of a value that was already being recorded and consumed by the previously-executed step-1 runner. This Plan self-review therefore records 0 blocking findings for this revision and does not identify a new mechanism requiring a further targeted Architect or human-triggered review before compilation.
