---
format: kilo-plan/v4
plan_id: native-arm64-podman-devcontainers
workspace_root: /Users/greenc/work/cet-is/sources/phlex/phlex
design_review_status: passed
design_review_blocking_findings: 0
max_total_dispatches: 18
max_consecutive_failures: 3
---
# Native arm64 Podman images and VS Code devcontainers

## Goal

Make `phlex-ci`, `phlex-dev`, and the VS Code devcontainer workflow usable on an Apple-Silicon Mac with a rootless native `linux/arm64` Podman machine and Spack LLVM/Clang as the developer default, while preserving amd64 GitHub CI, compiler-neutral CMake presets, Linux relay behavior, and local-only arm64 images. Produce receipt-grade evidence for an M8 representative-task case study without claiming an unperformed matched workflow benchmark.

## Scope

Update architecture-aware CI and developer image definitions, Darwin pasta networking and allowlisted TCP relays, write-free Kilo configuration rewriting, Compose mounts and image selection, the local image builder, and Mac documentation. Validate the combined implementation statically and through a native arm64 devcontainer run. Preserve and archive the retired plan source, compiled artifact, and stale state artifacts before direct execution state is created.

## Exclusions

Do not modify `.github/workflows/*`, `.actrc`, `CMakePresets.json`, `scripts/git-ai-commit`, `scripts/test/test_git_ai_commit.py`, `.devcontainer/post-create.sh`, `.kilo/*` except the named legacy archive and run-evidence outputs, `kilo.json`, or `AGENTS.md`. Do not create a rootful machine, expose a TCP Podman API, add a host interface or wildcard relay, push arm64 images, reuse amd64 caches or binaries for arm64, or invoke `genplan`, `plan-compiler`, `/compile-plan`, `.exec.json`, or legacy state as execution input. Actual amd64 image-build evidence is deferred to existing GitHub CI.

## Steps

### archive-legacy. Archive retired plan artifacts and establish provenance

#### Depends on

none

#### Executor

human

#### Capabilities

none

#### Allowed files

- .kilo/plans/legacy-archive/

#### Prohibited changes

- .kilo/plans/1787581717167-native-arm64-podman-devcontainers.md
- all repository source, configuration, workflow, preset, and credential paths

#### Task

Run the named runner from the workspace root. The pre-finalization legacy Markdown source is preserved byte-for-byte at `.kilo/plans/1787581717167-native-arm64-podman-devcontainers.legacy-source.md`. Copy the retired `.exec.json` and all four `state.stale-*.json` artifacts into `.kilo/plans/legacy-archive/`, retain originals, and write a SHA-256 receipt. This archive is evidence only and must never create or resume direct state.

#### Context

The old source is an untyped pre-v4 plan, so `archive_legacy_source.py` is not applicable. The adjacent legacy-source file is the raw archive. Copy-only archival prevents data loss and preserves the old execution evidence for comparison.

#### Acceptance criteria

- The legacy Markdown archive exists unchanged.
- The compiled artifact and all four stale states are copied under the legacy archive.
- The receipt lists every copied file and hash, and every copy matches its source.
- No unrelated path changes.

#### Verification

- Run `verify`; require exit 0, zero failures, and explicit approval.
- Compare all five JSON copies with their originals and record runner, receipt, manifest, and hash evidence.

#### Retry

max_attempts: 1
strategy: abort

#### Idempotency

true

#### Gate

human

#### Human runner

{
  "script": "archive-native-arm64-legacy.sh",
  "log_policy": "suppressed",
  "cwd": "/Users/greenc/work/cet-is/sources/phlex/phlex",
  "environment": [],
  "owned_paths": [".kilo/plans/legacy-archive"],
  "cleanup_paths": [],
  "phases": [
    {"id": "preflight", "command": "test -f .kilo/plans/1787581717167-native-arm64-podman-devcontainers.legacy-source.md && test -f .kilo/plans/1787581717167-native-arm64-podman-devcontainers.exec.json && test -f .kilo/plans/1787581717167-native-arm64-podman-devcontainers.state.stale-20260824T171324Z.json && test -f .kilo/plans/1787581717167-native-arm64-podman-devcontainers.state.stale-20260824T200753Z.json && test -f .kilo/plans/1787581717167-native-arm64-podman-devcontainers.state.stale-20260901T210616Z.json && test -f .kilo/plans/1787581717167-native-arm64-podman-devcontainers.state.stale-20260901T214254Z.json", "expected_exit": 0, "expected_output": "none", "remediation": "Stop and restore missing inputs without changing their bytes.", "abort": "abort"},
    {"id": "archive-json", "command": "mkdir -p .kilo/plans/legacy-archive && cp -p .kilo/plans/1787581717167-native-arm64-podman-devcontainers.exec.json .kilo/plans/1787581717167-native-arm64-podman-devcontainers.state.stale-20260824T171324Z.json .kilo/plans/1787581717167-native-arm64-podman-devcontainers.state.stale-20260824T200753Z.json .kilo/plans/1787581717167-native-arm64-podman-devcontainers.state.stale-20260901T210616Z.json .kilo/plans/1787581717167-native-arm64-podman-devcontainers.state.stale-20260901T214254Z.json .kilo/plans/legacy-archive/ && shasum -a 256 .kilo/plans/legacy-archive/*.json > .kilo/plans/legacy-archive/1787581717167-legacy-archive.sha256", "expected_exit": 0, "expected_output": "none", "remediation": "Stop; do not delete or overwrite originals.", "abort": "abort"},
    {"id": "verify", "command": "test -s .kilo/plans/legacy-archive/1787581717167-legacy-archive.sha256 && cmp .kilo/plans/1787581717167-native-arm64-podman-devcontainers.exec.json .kilo/plans/legacy-archive/1787581717167-native-arm64-podman-devcontainers.exec.json && cmp .kilo/plans/1787581717167-native-arm64-podman-devcontainers.state.stale-20260824T171324Z.json .kilo/plans/legacy-archive/1787581717167-native-arm64-podman-devcontainers.state.stale-20260824T171324Z.json && cmp .kilo/plans/1787581717167-native-arm64-podman-devcontainers.state.stale-20260824T200753Z.json .kilo/plans/legacy-archive/1787581717167-native-arm64-podman-devcontainers.state.stale-20260824T200753Z.json && cmp .kilo/plans/1787581717167-native-arm64-podman-devcontainers.state.stale-20260901T210616Z.json .kilo/plans/legacy-archive/1787581717167-native-arm64-podman-devcontainers.state.stale-20260901T210616Z.json && cmp .kilo/plans/1787581717167-native-arm64-podman-devcontainers.state.stale-20260901T214254Z.json .kilo/plans/legacy-archive/1787581717167-native-arm64-podman-devcontainers.state.stale-20260901T214254Z.json", "expected_exit": 0, "expected_output": "none", "remediation": "Stop at the first mismatch and do not approve.", "abort": "abort"}
  ]
}

### baseline. Capture worktree provenance and verification baseline

#### Depends on

- archive-legacy

#### Executor

execute

#### Capabilities

none

#### Allowed files

- .kilo/plans/evidence/

#### Prohibited changes

- all repository files except the bound `.kilo/plans/evidence/<run-id>/` directory
- all paths in Exclusions

#### Task

Capture an invocation-unique baseline before implementation dispatch. Set `PHLEX_RUN_ID` and `PHLEX_EVIDENCE_DIR` to a new run-specific directory under the declared durable evidence root and record both in the direct checkpoint. Record `git status --short`, SHA-256 for every currently modified or implementation-relevant path, exact pre-existing hashes for `scripts/git-ai-commit`, `scripts/test/test_git_ai_commit.py`, `.devcontainer/post-create.sh`, `CMakePresets.json`, all four CI files, both relay/Kilo files, the three Compose files, and `scripts/README.md`. Record `scripts/build-container-images.sh` and `docs/dev/podman-macos.md` as `{path, state: absent}` when absent. Preserve the baseline and partial diffs under that run directory and do not restore or modify any path.

#### Context

The prior execution left partial changes in the relay/Kilo scripts and unrelated changes in the two git-ai-commit paths. Provenance must be established before ownership is assigned. The baseline is external evidence and not a repository change.

#### Acceptance criteria

- A baseline report, run ID, absolute evidence directory, and manifest exist under the durable evidence root; the direct checkpoint binds them before step 1.
- The report includes all named paths, `.devcontainer/post-create.sh`, `CMakePresets.json`, complete pre-existing status, and `{path, state: absent}` entries for `scripts/build-container-images.sh` and `docs/dev/podman-macos.md` when absent.
- A second capture proves the worktree was unchanged.

#### Verification

- Recompute status and hashes immediately after capture and require identical results.
- Record owner-only evidence files and their hashes in the checkpoint.

#### Retry

max_attempts: 1
strategy: abort

#### Idempotency

true

#### Gate

automatic

### 1. Verify the native rootless Podman host and pasta path

#### Depends on

- baseline

#### Executor

human

#### Capabilities

none

#### Allowed files

- .kilo/plans/evidence/

#### Prohibited changes

- all repository files except the bound `.kilo/plans/evidence/<run-id>/` directory
- Podman rootful configuration, TCP API configuration, host interfaces, and unrelated processes

#### Task

From the workspace root, confirm Podman, the pinned `podman-compose` provider, `socat`, `shellcheck`, and Darwin `lsof`. Separately label machine state, native arm64/aarch64 architecture, rootless mode, at least 8 vCPUs, 16 GiB memory, 100 GiB free VM disk, and VM-side rootless socket integrity. Only an explicitly recorded absent or stopped machine may be initialized or started; initialization is rootless with 8 CPUs, 16384 MiB memory, and 120 GiB disk. Tool remediation is per-missing-tool and a guarded no-op when already installed. Run the native `docker.io/library/alpine:3.22` probe and an invocation-unique generated Compose probe at `$PHLEX_EVIDENCE_DIR/compose-probe.yml` with `network_mode: pasta`; do not start the repository `.devcontainer/docker-compose.yml` in this gate. Both probes reach owned loopback listeners, prefer resolving `host.docker.internal` over `host.containers.internal`, and print the required token. Tear down only owned probes and write the gateway, provider/version, local Podman-machine socket, resources, and exact fixtures to the run evidence directory.

**Resource preconditions**: The VM must be provisioned with a 120 GiB virtual disk. If the pre-existing machine's guest root filesystem does not have at least 100 GiB free (measured as `104857600` KiB), the machine must be recreated externally with 120 GiB provisioned. This plan does not resize or recreate an existing machine with insufficient free disk. Evidence captures provisioned disk size from `podman machine inspect --format '{{.Resources.DiskSize}}'` and measured root free-disk separately.

#### Context

This is a Darwin host gate. No manually assigned address, bridge, amd64 image, emulated probe, or host-interface bind is equivalent evidence. `PHLEX_PODMAN_SOCKET_SOURCE` is the local `ConnectionInfo.PodmanSocket.Path`; `PHLEX_PODMAN_SOCKET_KIND=vm-rootless`. Set `PHLEX_HOST_REMEDIATION_REASON` only to the inspected `absent` or `stopped` result before remediation.

#### Acceptance criteria

- State, architecture, rootless mode, every resource threshold, and socket integrity pass as separately labelled checks.
- Compose, `socat`, `shellcheck`, and `lsof` are available.
- Direct and Compose pasta probes reach owned listeners and print `phlex-probe-ok`.
- The gateway, provider/version, local socket, VM socket result, and `25114=25115,25300=25301` mappings are recorded with no TCP API or non-loopback exposure.

#### Verification

- Run the declared phases in order; remediation runs only for the stated missing-tool or absent/stopped condition.
- Every command exits 0; `verify` reports zero failures and records explicit `approve` or `stop`.

#### Retry

max_attempts: 1
strategy: abort

#### Idempotency

true

#### Gate

human

#### Human runner

{
  "script": "native-arm64-podman-host.sh",
  "log_policy": "suppressed",
  "cwd": "/Users/greenc/work/cet-is/sources/phlex/phlex",
   "environment": ["PHLEX_RUN_ID", "PHLEX_EVIDENCE_DIR", "PHLEX_HOST_REMEDIATION_REASON", "PHLEX_PODMAN_SOCKET_KIND", "PHLEX_MACHINE_CPUS", "PHLEX_MACHINE_MEMORY", "PHLEX_MACHINE_DISK", "PHLEX_TEST_RELAY_PORTS", "PHLEX_HOST_RELAY_PORTS"],
   "owned_paths": [".kilo/plans/evidence"],
   "cleanup_paths": [],
      "phases": [
       {"id": "preflight", "command": "test -n \"$PHLEX_RUN_ID\" && test -n \"$PHLEX_EVIDENCE_DIR\" && test \"$PHLEX_EVIDENCE_DIR\" = \"$PWD/.kilo/plans/evidence/$PHLEX_RUN_ID\" && test \"$(pwd)\" = \"/Users/greenc/work/cet-is/sources/phlex/phlex\" && mkdir -p \"$PHLEX_EVIDENCE_DIR\" && printf 'env-binding>>\\n' >> \"$PHLEX_EVIDENCE_DIR/step-1.md\"", "expected_exit": 0, "expected_output": "none", "remediation": "Stop and verify env binding configuration; PHLEX_RUN_ID and PHLEX_EVIDENCE_DIR must be set correctly.", "abort": "abort"},
       {"id": "preflight-cwd", "command": "test \"$(pwd)\" = \"/Users/greenc/work/cet-is/sources/phlex/phlex\" && printf 'cwd>>\\n' >> \"$PHLEX_EVIDENCE_DIR/step-1.md\"", "expected_exit": 0, "expected_output": "none", "remediation": "Stop; execution must occur from the workspace root.", "abort": "abort"},
       {"id": "preflight-podman", "command": "command -v podman && printf 'podman>>\\n' >> \"$PHLEX_EVIDENCE_DIR/step-1.md\"", "expected_exit": 0, "expected_output": "none", "remediation": "Stop and install Podman; it is required for machine and container operations.", "abort": "abort"},
       {"id": "preflight-lsof", "command": "command -v lsof && printf 'lsof>>\\n' >> \"$PHLEX_EVIDENCE_DIR/step-1.md\"", "expected_exit": 0, "expected_output": "none", "remediation": "Stop and install lsof; it is required for Darwin port-listening detection.", "abort": "abort"},
        {"id": "preflight-brew", "command": "command -v brew && printf 'brew>>\\n' >> \"$PHLEX_EVIDENCE_DIR/step-1.md\"", "expected_exit": 0, "expected_output": "none", "remediation": "Stop and install Homebrew; tool remediation requires brew.", "abort": "abort"},
        {"id": "preflight-port-25110", "command": "test -z \"$(lsof -tiTCP:25110 -sTCP:LISTEN)\" && printf 'port-25110-free>>\\n' >> \"$PHLEX_EVIDENCE_DIR/step-1.md\"", "expected_exit": 0, "expected_output": "none", "remediation": "Stop and kill the listener using port 25110; this port must be free for probe listeners.", "abort": "abort"},
        {"id": "preflight-port-25112", "command": "test -z \"$(lsof -tiTCP:25112 -sTCP:LISTEN)\" && printf 'port-25112-free>>\\n' >> \"$PHLEX_EVIDENCE_DIR/step-1.md\"", "expected_exit": 0, "expected_output": "none", "remediation": "Stop and kill the listener using port 25112; this port must be free for probe listeners.", "abort": "abort"},
       {"id": "preflight-resources", "command": "disk_provisioned_gib=$(podman machine inspect podman-machine-default --format '{{.Resources.DiskSize}}' 2>/dev/null || { printf 'machine-not-found\\n' > \"$PHLEX_EVIDENCE_DIR/step-1-resources-fail\"; exit 1; }); cpus=$(podman machine inspect podman-machine-default --format '{{.Resources.CPUs}}' 2>/dev/null || { printf 'machine-not-found\\n' > \"$PHLEX_EVIDENCE_DIR/step-1-resources-fail\"; exit 1; }); memory=$(podman machine inspect podman-machine-default --format '{{.Resources.Memory}}' 2>/dev/null || { printf 'machine-not-found\\n' > \"$PHLEX_EVIDENCE_DIR/step-1-resources-fail\"; exit 1; }); disk_free_kb=$(podman machine ssh podman-machine-default df -Pk / | awk 'END {print $4}' 2>/dev/null || { printf 'machine-not-running\\n' > \"$PHLEX_EVIDENCE_DIR/step-1-resources-fail\"; exit 1; }); printf 'disk_provisioned_gib=%s\\ncpus=%s\\nmemory=%s\\ndisk_free_kb=%s\\n' \"$disk_provisioned_gib\" \"$cpus\" \"$memory\" \"$disk_free_kb\" > \"$PHLEX_EVIDENCE_DIR/step-1-measured-resources\"; test \"$disk_provisioned_gib\" -ge 120 || { printf 'disk-provisioned-%s-gib-less-than-120\\n' \"$disk_provisioned_gib\" > \"$PHLEX_EVIDENCE_DIR/step-1-resources-fail\"; exit 1; }; test \"$disk_free_kb\" -ge 104857600 || { printf 'disk-free-%s-kb-less-than-104857600\\n' \"$disk_free_kb\" > \"$PHLEX_EVIDENCE_DIR/step-1-resources-fail\"; exit 1; }; test \"$cpus\" -ge 8 || { printf 'cpus-%s-less-than-8\\n' \"$cpus\" > \"$PHLEX_EVIDENCE_DIR/step-1-resources-fail\"; exit 1; }; test \"$memory\" -ge 16384 || { printf 'memory-%s-less-than-16384\\n' \"$memory\" > \"$PHLEX_EVIDENCE_DIR/step-1-resources-fail\"; exit 1; }; printf 'resources>>\\n' >> \"$PHLEX_EVIDENCE_DIR/step-1.md\"", "expected_exit": 0, "expected_output": "none", "remediation": "Stop; the machine must be provisioned with at least 120 GiB disk and 100 GiB free space (104857600 KiB), 8 CPUs, and 16384 MiB memory. External recreation/start with podman machine init --rootful=false --cpus=8 --memory=16384 --disk-size=120 is required before this gate.", "abort": "abort"},
        {"id": "remediate-tools", "command": "command -v brew >/dev/null || { printf 'brew-not-found>>\\n' >> \"$PHLEX_EVIDENCE_DIR/step-1.md\"; exit 1; }; command -v podman-compose >/dev/null || { brew_out=$(brew install podman-compose 2>&1 || printf ''); }; command -v socat >/dev/null || { brew_out=$(brew install socat 2>&1 || printf ''); }; command -v shellcheck >/dev/null || { brew_out=$(brew install shellcheck 2>&1 || printf ''); }; command -v podman && command -v podman-compose && command -v socat && command -v shellcheck && command -v lsof && printf 'tools-installed>>\\n' >> \"$PHLEX_EVIDENCE_DIR/step-1.md\"", "expected_exit": 0, "expected_output": "none", "remediation": "Stop and record the per-tool package failure or missing-tool failure after installation.", "abort": "abort"},
      {"id": "inspect-state", "command": "state=$(podman machine inspect podman-machine-default --format '{{.State}}' 2>/dev/null || printf absent); printf '%s\\n' \"$state\" > \"$PHLEX_EVIDENCE_DIR/step-1-machine-state\"; printf 'state>>\\n' >> \"$PHLEX_EVIDENCE_DIR/step-1.md\"; test \"$state\" = running -o \"$state\" = stopped -o \"$state\" = absent", "expected_exit": 0, "expected_output": "none", "remediation": "Use the recorded state file for the guarded remediation phase; any other result aborts.", "abort": "remediate_then_abort"},
       {"id": "remediate-host", "command": "state=$(cat \"$PHLEX_EVIDENCE_DIR/step-1-machine-state\"); if test \"$state\" = running; then :; elif test \"$state\" = stopped; then podman machine start podman-machine-default; elif test \"$state\" = absent; then podman machine init --rootful=false --cpus=8 --memory=16384 --disk-size=120 --name=podman-machine-default && podman machine start podman-machine-default; else false; fi", "expected_exit": 0, "expected_output": "none", "remediation": "Stop unless the recorded state is running, absent, or stopped; never recreate a mismatched machine.", "abort": "abort"},
      {"id": "inspect-architecture", "command": "podman info --format '{{.Host.Arch}}' | grep -E '^(arm64|aarch64)$' && printf 'architecture>>\\n' >> \"$PHLEX_EVIDENCE_DIR/step-1.md\"", "expected_exit": 0, "expected_output": "none", "remediation": "Stop on an architecture mismatch; never emulate or recreate.", "abort": "abort"},
      {"id": "inspect-rootless", "command": "podman info --format '{{.Host.Security.Rootless}}' | grep -Fx true && printf 'rootless>>\\n' >> \"$PHLEX_EVIDENCE_DIR/step-1.md\"", "expected_exit": 0, "expected_output": "none", "remediation": "Stop on a rootful result; do not change rootfulness.", "abort": "abort"},
      {"id": "inspect-resources", "command": "set -- $(podman machine inspect podman-machine-default --format '{{.Resources.CPUs}} {{.Resources.Memory}} {{.Resources.DiskSize}}'); cpus=\"$1\"; memory=\"$2\"; disk_provisioned=\"$3\"; disk_free_kb=$(podman machine ssh podman-machine-default df -Pk / | awk 'END {print $4}'); printf 'cpus=%s\\nmemory=%s\\ndisk_provisioned_gib=%s\\ndisk_free_kb=%s\\n' \"$cpus\" \"$memory\" \"$disk_provisioned\" \"$disk_free_kb\" > \"$PHLEX_EVIDENCE_DIR/step-1-measured-resources\" && test \"$cpus\" -ge 8 && test \"$memory\" -ge 16384 && test \"$disk_free_kb\" -ge 104857600 && printf 'cpu-memory-disk>>\\n' >> \"$PHLEX_EVIDENCE_DIR/step-1.md\"", "expected_exit": 0, "expected_output": "none", "remediation": "Stop on a CPU, memory, or free-disk mismatch; the 100 GiB free-space threshold requires 120 GiB provisioned.", "abort": "abort"},
      {"id": "inspect-socket", "command": "podman machine ssh podman-machine-default test -S /run/user/$(podman machine ssh podman-machine-default id -u)/podman/podman.sock && printf 'socket>>\\n' >> \"$PHLEX_EVIDENCE_DIR/step-1.md\"", "expected_exit": 0, "expected_output": "none", "remediation": "Stop on socket-integrity failure; do not enter machine creation.", "abort": "abort"},
      {"id": "derive-gateway", "command": "gateway=$(podman run --rm --network pasta docker.io/library/alpine:3.22 sh -c 'getent hosts host.docker.internal >/dev/null && printf host.docker.internal || printf host.containers.internal'); test -n \"$gateway\" && printf '%s\\n' \"$gateway\" > \"$PHLEX_EVIDENCE_DIR/step-1-gateway\" && printf 'gateway>>\\n' >> \"$PHLEX_EVIDENCE_DIR/step-1.md\"", "expected_exit": 0, "expected_output": "none", "remediation": "Stop if neither documented gateway resolves; do not choose an address.", "abort": "abort"},
        {"id": "probe-pasta", "command": "gateway=$(cat \"$PHLEX_EVIDENCE_DIR/step-1-gateway\"); socat TCP-LISTEN:25110,reuseaddr,fork,bind=127.0.0.1 SYSTEM:'printf \"phlex-probe-ok\"' >/dev/null 2>&1 & pid=$!; iteration=0; verified_pid=; while test $iteration -lt 10 && test -z \"$verified_pid\"; do verified_pid=$(lsof -tiTCP:25110 -sTCP:LISTEN | head -n1); sleep 0.2; iteration=$((iteration + 1)); done; test -n \"$verified_pid\" || { printf 'probe-pasta-no-pid\\n' > \"$PHLEX_EVIDENCE_DIR/step-1-probe-failure\"; exit 1; }; lsof -nP -a -p \"$verified_pid\" -iTCP:25110 -sTCP:LISTEN > \"$PHLEX_EVIDENCE_DIR/step-1-probe-pasta-evidence\"; grep -Fq '127.0.0.1:25110' \"$PHLEX_EVIDENCE_DIR/step-1-probe-pasta-evidence\" && grep -Fq 'socat' \"$PHLEX_EVIDENCE_DIR/step-1-probe-pasta-evidence\" || { printf 'probe-pasta-ownership\\n' > \"$PHLEX_EVIDENCE_DIR/step-1-probe-failure\"; exit 1; }; printf '25110|%s\\n' \"$verified_pid\" > \"$PHLEX_EVIDENCE_DIR/step-1-probe-pids\" && podman run --rm --network pasta docker.io/library/alpine:3.22 sh -c \"printf x | nc $gateway 25110 | grep -Fx phlex-probe-ok\" && printf 'probe-pasta-token>>\\n' >> \"$PHLEX_EVIDENCE_DIR/step-1.md\"", "expected_exit": 0, "expected_output": "none", "remediation": "Run teardown-probes for owned listener cleanup, then stop and record the exact probe failure.", "abort": "abort"},
        {"id": "probe-compose-pasta", "command": "gateway=$(cat \"$PHLEX_EVIDENCE_DIR/step-1-gateway\"); printf 'services:\\n  probe:\\n    image: docker.io/library/alpine:3.22\\n    network_mode: pasta\\n    command: [sh, -c, \"printf x | nc %s 25112 | grep -Fx phlex-probe-ok\"]\\n' \"$gateway\" > \"$PHLEX_EVIDENCE_DIR/compose-probe.yml\"; socat TCP-LISTEN:25112,reuseaddr,fork,bind=127.0.0.1 SYSTEM:'printf \"phlex-probe-ok\"' >/dev/null 2>&1 & pid=$!; iteration=0; verified_pid=; while test $iteration -lt 10 && test -z \"$verified_pid\"; do verified_pid=$(lsof -tiTCP:25112 -sTCP:LISTEN | head -n1); sleep 0.2; iteration=$((iteration + 1)); done; test -n \"$verified_pid\" || { printf 'probe-compose-no-pid\\n' > \"$PHLEX_EVIDENCE_DIR/step-1-probe-failure\"; exit 1; }; lsof -nP -a -p \"$verified_pid\" -iTCP:25112 -sTCP:LISTEN > \"$PHLEX_EVIDENCE_DIR/step-1-probe-compose-evidence\"; grep -Fq '127.0.0.1:25112' \"$PHLEX_EVIDENCE_DIR/step-1-probe-compose-evidence\" && grep -Fq 'socat' \"$PHLEX_EVIDENCE_DIR/step-1-probe-compose-evidence\" || { printf 'probe-compose-ownership\\n' > \"$PHLEX_EVIDENCE_DIR/step-1-probe-failure\"; exit 1; }; printf '25112|%s\\n' \"$verified_pid\" >> \"$PHLEX_EVIDENCE_DIR/step-1-probe-pids\" && podman-compose -f \"$PHLEX_EVIDENCE_DIR/compose-probe.yml\" -p phlex-pasta-probe up --abort-on-container-exit --exit-code-from probe && printf 'probe-compose-token>>\\n' >> \"$PHLEX_EVIDENCE_DIR/step-1.md\"", "expected_exit": 0, "expected_output": "none", "remediation": "Run teardown-probes for owned listener cleanup, then stop and record the Compose failure.", "abort": "abort"},
      {"id": "check-loopback-bindings", "command": "lsof -nP -iTCP:25110 -sTCP:LISTEN > \"$PHLEX_EVIDENCE_DIR/step-1-loopback-evidence\" && lsof -nP -iTCP:25112 -sTCP:LISTEN >> \"$PHLEX_EVIDENCE_DIR/step-1-loopback-evidence\" && grep -qF '127.0.0.1:25110' \"$PHLEX_EVIDENCE_DIR/step-1-loopback-evidence\" && grep -qF '127.0.0.1:25112' \"$PHLEX_EVIDENCE_DIR/step-1-loopback-evidence\" && printf 'loopback-bindings>>\\n' >> \"$PHLEX_EVIDENCE_DIR/step-1.md\"", "expected_exit": 0, "expected_output": "none", "remediation": "Stop if probe listeners are not bound to 127.0.0.1; require loopback-only exposure and save evidence before teardown.", "abort": "abort"},
      {"id": "check-no-tcp-api-uri", "command": "podman system connection list --format '{{.URI}}' > \"$PHLEX_EVIDENCE_DIR/step-1-tcp-uri-evidence\" && tcp_uris=$(grep '^tcp://' \"$PHLEX_EVIDENCE_DIR/step-1-tcp-uri-evidence\" 2>/dev/null || printf ''); test -z \"$tcp_uris\" && printf 'no-tcp-api-uri>>\\n' >> \"$PHLEX_EVIDENCE_DIR/step-1.md\"", "expected_exit": 0, "expected_output": "none", "remediation": "Stop if podman system connection list contains tcp:// URIs; save evidence before teardown.", "abort": "abort"},
      {"id": "teardown-probes", "command": "compose_file=\"$PHLEX_EVIDENCE_DIR/compose-probe.yml\"; rm_file=\"$PHLEX_EVIDENCE_DIR/step-1-probe-pids\"; cleanup_failed=0; if test -s \"$rm_file\"; then while IFS='|' read -r port pid; do test -n \"$port\" && test -n \"$pid\" || continue; listener_pid=$(lsof -tiTCP:\"$port\" -sTCP:LISTEN 2>/dev/null | head -n1); if test -n \"$listener_pid\"; then test \"$listener_pid\" = \"$pid\" || { printf 'teardown-unowned-port-%s\\n' \"$port\"; cleanup_failed=1; break; }; lsof -nP -a -p \"$listener_pid\" -iTCP:\"$port\" -sTCP:LISTEN > \"$PHLEX_EVIDENCE_DIR/step-1-teardown-evidence\" && grep -Fq 'socat' \"$PHLEX_EVIDENCE_DIR/step-1-teardown-evidence\" || { printf 'teardown-no-ownership-proof-%s\\n' \"$port\"; cleanup_failed=1; break; }; kill \"$listener_pid\"; iteration=0; port_clear=0; while test $iteration -lt 10 && test $port_clear -eq 0; do sleep 0.5; test -z \"$(lsof -tiTCP:\"$port\" -sTCP:LISTEN 2>/dev/null)\" && port_clear=1; iteration=$((iteration + 1)); done; test $port_clear -eq 0 && { printf 'teardown-port-%s-not-cleared\\n' \"$port\"; cleanup_failed=1; break; }; fi; done < \"$rm_file\"; fi; if test $cleanup_failed -eq 0 && test -f \"$compose_file\"; then if ! podman-compose -f \"$compose_file\" -p phlex-pasta-probe down 2>/dev/null; then printf 'teardown-compose-down-failed\\n'; exit 1; fi; fi; test -z \"$(lsof -tiTCP:25110 -sTCP:LISTEN 2>/dev/null)\" || { printf 'port-25110-not-cleared\\n'; exit 1; }; test -z \"$(lsof -tiTCP:25112 -sTCP:LISTEN 2>/dev/null)\" || { printf 'port-25112-not-cleared\\n'; exit 1; }; : > \"$PHLEX_EVIDENCE_DIR/step-1-probe-pids\"", "expected_exit": 0, "expected_output": "none", "remediation": "Stop if ownership cannot be proven for any PID or either listener remains after attempted cleanup; each port must clear within 5 seconds of signal.", "abort": "abort"},
      {"id": "record-values", "command": ". \"$PHLEX_EVIDENCE_DIR/step-1-measured-resources\"; live_socket=$(podman machine inspect podman-machine-default --format '{{.ConnectionInfo.PodmanSocket.Path}}' 2>/dev/null); live_provider=$(command -v podman-compose); live_version=$(podman-compose --version 2>&1 | head -n1); live_gateway=$(cat \"$PHLEX_EVIDENCE_DIR/step-1-gateway\" 2>/dev/null || printf ''); test -n \"$live_socket\" && printf \"%s\\n\" \"$live_socket\" > \"$PHLEX_EVIDENCE_DIR/step-1-podman-socket\"; test -n \"$live_provider\" && printf \"%s\\n\" \"$live_provider\" > \"$PHLEX_EVIDENCE_DIR/step-1-compose-provider\"; test -n \"$live_version\" && printf \"%s\\n\" \"$live_version\" > \"$PHLEX_EVIDENCE_DIR/step-1-compose-version\"; test -n \"$live_gateway\" && printf \"relay-fixtures>>\\ncompose-provider-version>>\\n\" >> \"$PHLEX_EVIDENCE_DIR/step-1.md\" && printf \"PHLEX_HOST_GATEWAY=%s\\nPHLEX_PODMAN_SOCKET_SOURCE=%s\\nPHLEX_PODMAN_SOCKET_KIND=%s\\nPHLEX_COMPOSE_PROVIDER=%s\\nPHLEX_COMPOSE_VERSION=%s\\nPHLEX_MACHINE_CPUS=%s\\nPHLEX_MACHINE_MEMORY=%s\\nPHLEX_MACHINE_DISK=%s\\nPHLEX_TEST_RELAY_PORTS=%s\\nPHLEX_HOST_RELAY_PORTS=%s\\n\" \"$live_gateway\" \"$live_socket\" \"$PHLEX_PODMAN_SOCKET_KIND\" \"$live_provider\" \"$live_version\" \"$cpus\" \"$memory\" \"$disk_free_kb\" \"$PHLEX_TEST_RELAY_PORTS\" \"$PHLEX_HOST_RELAY_PORTS\" > \"$PHLEX_EVIDENCE_DIR/step-1-values.env\"", "expected_exit": 0, "expected_output": "none", "remediation": "Stop and record missing gateway, socket, provider, measured resources, or version mismatch.", "abort": "abort"},
      {"id": "verify", "command": "test -s \"$PHLEX_EVIDENCE_DIR/step-1-values.env\" && test -s \"$PHLEX_EVIDENCE_DIR/step-1.md\" && for label in \"env-binding>>\" \"cwd>>\" \"podman>>\" \"lsof>>\" \"brew>>\" \"port-25110-free>>\" \"port-25112-free>>\" \"tools-installed>>\" \"state>>\" \"architecture>>\" \"rootless>>\" \"cpu-memory-disk>>\" \"socket>>\" \"gateway>>\" \"probe-pasta-token>>\" \"probe-compose-token>>\" \"compose-provider-version>>\" \"relay-fixtures>>\" \"loopback-bindings>>\" \"no-tcp-api-uri>>\"; do grep -Fx \"$label\" \"$PHLEX_EVIDENCE_DIR/step-1.md\"; done && test ! -s \"$PHLEX_EVIDENCE_DIR/step-1-probe-pids\" && test -s \"$PHLEX_EVIDENCE_DIR/step-1-measured-resources\" && test -s \"$PHLEX_EVIDENCE_DIR/step-1-podman-socket\" && test -s \"$PHLEX_EVIDENCE_DIR/step-1-compose-provider\" && test -s \"$PHLEX_EVIDENCE_DIR/step-1-compose-version\" && test -s \"$PHLEX_EVIDENCE_DIR/step-1-loopback-evidence\" && test -s \"$PHLEX_EVIDENCE_DIR/step-1-tcp-uri-evidence\" && grep -Fq '127.0.0.1:25110' \"$PHLEX_EVIDENCE_DIR/step-1-loopback-evidence\" && grep -Fq '127.0.0.1:25112' \"$PHLEX_EVIDENCE_DIR/step-1-loopback-evidence\" && ! grep -q '^tcp://' \"$PHLEX_EVIDENCE_DIR/step-1-tcp-uri-evidence\"", "expected_exit": 0, "expected_output": "none", "remediation": "Stop without approval and record the missing receipt, evidence file, or failed assertion.", "abort": "abort"}
    ]
}

### 2. Make Spack images architecture-aware and set compiler defaults

#### Depends on

- baseline

#### Executor

coder-qwen

#### Capabilities

- filesystem
- shell
- tool:bash

#### Allowed files

- ci/Dockerfile
- ci/spack.yaml
- ci/packages.yaml
- ci/entrypoint.sh

#### Prohibited changes

- all paths in Exclusions
- amd64 caches or binaries in arm64 definitions
- image pushes, machine mutation, and commits

#### Task

Add validated architecture selection: `x86_64_v3` for amd64 and `aarch64` for arm64. Propagate it through every Spack target constraint, GCC bootstrap, package defaults, CMake, LLVM, and package requirements; include AArch64 in arm64 LLVM backends; reject host/target mismatches; prevent cross-architecture cache reuse; preserve GCC 15 ABI and Fermilab mirror. Make act and tracebox architecture-aware or explicit opt-out. Keep CI GCC-default and make developer images default to Spack LLVM/Clang with a reproducible GCC 15 `--gcc-toolchain` binding and explicit GCC override.

#### Context

The generic CMake default preset remains compiler-neutral. The prior execution changed these four files; verify the baseline hashes before editing and preserve unrelated changes. Linux reference nodes are amd64-only, so use static amd64 rendering and native arm64 concretization only.

#### Acceptance criteria

- amd64 renders only `x86_64_v3`; arm64 renders `aarch64` with no x86 microarchitecture requirement.
- arm64 LLVM includes AArch64 and compiler defaults match the CI/developer split.
- A trivial C++23 compile resolves Spack GCC 15 libstdc++ under developer Clang.
- `CMakePresets.json` is byte-identical to the baseline manifest.

#### Verification

- Verify the baseline hashes, then run shell syntax, effective Spack configuration, and bounded native arm64 concretize-only checks.
- Assert the amd64 branch statically without starting an amd64 container.
- Record changed paths, output hashes, and scope results.

#### Retry

max_attempts: 2
strategy: resume_then_narrow

#### Idempotency

true

#### Gate

automatic

### 3a. Implement the allowlisted host relay contract

#### Depends on

- baseline

#### Executor

coder-qwen

#### Capabilities

- filesystem
- shell
- tool:bash

#### Allowed files

- .devcontainer/ensure-repos.sh

#### Prohibited changes

- .devcontainer/kilo-env.sh
- .devcontainer/post-create.sh
- all paths in Exclusions
- wildcard listeners, port scans, TCP APIs, and production evidence paths during tests

#### Task

Preserve Linux behavior and its existing Headroom variables and socket source while adding Darwin pasta. Parse the complete numeric `source=relay` allowlist before starting anything; reject malformed, duplicate, privileged, equal, unavailable, and unlisted mappings; write deterministic empty maps on failure; detect exact listeners with Darwin `lsof` or Linux `ss`; start `socat` listener-first; bind Darwin relays to `127.0.0.1`; record only ready relays; and clean only stale processes whose PID-file command identity proves ownership. Add `PHLEX_RELAY_STATE_DIR`, defaulting to `$HOME/.phlex-devcontainer-tmp/relays`, and exact `relay-map.json` and `relay-map.env` outputs. Implement `--start-test-services`, `--self-test`, and `--cleanup-owned` so later gates can start named loopback fixtures, run the same ownership-safe checks, and clean only proven-owned processes. On Darwin, source step-1 values and fail closed before Compose when the recorded local socket or VM rootless check is invalid. Set Linux `PHLEX_PODMAN_SOCKET_KIND=linux-proxy` without changing the Linux path.

#### Context

The prior combined relay/Kilo dispatch was aborted after partial changes. The pre-dispatch hash from `baseline` is mandatory and the prior aborted attempt is charged to this step, leaving one allowed implementation attempt. Successful relays remain alive for the devcontainer; only `--cleanup-owned` or the final human gate may stop proven-owned relays. No process-wide kill pattern is permitted.

#### Acceptance criteria

- Valid mappings produce sorted deterministic maps only for ready explicitly listed services.
- Every invalid mapping class fails closed with empty maps and no relay process.
- Darwin uses loopback plus pasta and the recorded rootless socket; Linux behavior remains intact.
- Stale cleanup is ownership-proven and no unrelated process is stopped.

#### Verification

- Verify the baseline hash, then run `bash -n` and `shellcheck`.
- Exercise positive, negative, unavailable, duplicate, privileged, equal-port, deterministic-map, owned-PID, socket-guard, named test-service, `--self-test`, and `--cleanup-owned` cases in a unique external directory.
- Record pre/post hashes, changed paths, and all test output hashes.

#### Retry

max_attempts: 1
strategy: resume_then_narrow

#### Idempotency

true

#### Gate

automatic

### 3b. Implement write-free Kilo configuration rewriting

#### Depends on

- baseline
- 3a

#### Executor

coder-qwen

#### Capabilities

- filesystem
- shell
- tool:bash

#### Allowed files

- .devcontainer/kilo-env.sh

#### Prohibited changes

- .devcontainer/ensure-repos.sh
- .devcontainer/post-create.sh
- all paths in Exclusions
- host Kilo mounts, credentials, and repository-local relay fixtures

#### Task

Discover `/root/.config/kilo/kilo.json` or `kilo.jsonc` from the read-only mount and read `/run/phlex-host-relays/relay-map.json` or `relay-map.env`. Parse JSONC in memory, removing comments and trailing commas only outside quoted strings and failing closed on malformed input. Rewrite only HTTP(S) URLs with loopback hosts and source ports in the map, preserve schemes and all other values, and export `KILO_CONFIG_CONTENT` without writing or backing up the mounted host configuration. Implement `--self-test` to exercise JSON and JSONC fixtures, loopback-only rewriting, malformed-input failure, and a no-write assertion. Fixtures live only in the unique external test directory.

#### Context

The relay map interface is exact: host `$HOME/.phlex-devcontainer-tmp/relays`, container `/run/phlex-host-relays`, files `relay-map.json` and `relay-map.env`. Stdio MCP, stdio LSP, and host Unix sockets are not tunneled.

#### Acceptance criteria

- JSON and JSONC rewriting is in-memory, scheme-preserving, loopback-only, and map-driven.
- Non-loopback URLs and unmapped ports remain unchanged.
- Malformed input fails closed without partial export.
- No host Kilo file is written or backed up.

#### Verification

- Verify the baseline hash, then run `bash -n` and `shellcheck`.
- Test discovery, valid rewrites, unchanged values, malformed input, and no-write behavior with external fixtures.
- Record fixture/output hashes and the allowed-file assertion.

#### Retry

max_attempts: 2
strategy: resume_then_narrow

#### Idempotency

true

#### Gate

automatic

### 4. Wire Compose, socket access, and local image selection

#### Depends on

- baseline
- 1
- 3a
- 3b

#### Executor

coder-qwen

#### Capabilities

- filesystem
- shell
- tool:bash

#### Allowed files

- .devcontainer/Dockerfile
- .devcontainer/docker-compose.yml
- .devcontainer/devcontainer.json

#### Prohibited changes

- all paths in Exclusions
- new nested-Podman mechanisms, host-interface mounts, TCP APIs, and host Kilo writes

#### Task

Add `PHLEX_DEV_NETWORK_MODE` with `pasta` for Mac while retaining clean-machine and Linux defaults. Retain the existing unconditional `${HOME}/.podman-proxy/podman.sock:/tmp/podman.sock` mount and dummy fallback as the sole nested-Podman mechanism; ordinary development does not require it. Change the host Kilo mount to read-only at `/root/.config/kilo`, add host `$HOME/.phlex-devcontainer-tmp/relays:/run/phlex-host-relays:ro`, and expose gateway, map, socket-source, and socket-kind values. Add distinct local arm64 image variables without overwriting GHCR defaults.

#### Context

The Darwin socket source is the local Podman machine connection socket, not a VM-internal or home-share socket. When the optional existing proxy socket is used, it grants rootless container-runtime control of the VM and is trusted host access; no new bridge is allowed.

#### Acceptance criteria

- Local selection resolves to `localhost/phlex-dev:arm64-local` and `localhost/phlex-devcontainer:arm64-local`.
- Defaults remain pinned GHCR references.
- `pasta`, read-only Kilo, read-only relay-map, exact source directory and filenames, and distinct socket values resolve.
- Optional nested Podman is explicitly optional and uses only the existing proxy mount.

#### Verification

- Verify Compose with `podman-compose -f .devcontainer/docker-compose.yml config` for local and default variables.
- Parse `devcontainer.json`; assert exact mounts, network, environment, and absence of home-share/XDG socket mounts.
- Run the scope assertion and record resolved-config hashes.

#### Retry

max_attempts: 2
strategy: resume_then_narrow

#### Idempotency

true

#### Gate

automatic

### 5. Add the architecture-detecting local image builder and Mac guide

#### Depends on

- 4

#### Executor

coder-qwen

#### Capabilities

- filesystem
- shell
- tool:bash

#### Allowed files

- scripts/build-container-images.sh
- scripts/README.md
- docs/dev/podman-macos.md

#### Prohibited changes

- all paths in Exclusions
- image pushes, repository-local relay state, shared amd64 caches, and repository/output GPG keys

#### Task

Implement a repeatable builder that detects Podman backend architecture, maps amd64 to `x86_64_v3` and arm64 to `aarch64`, rejects unsupported or emulated native-arm64 requests, builds CI/dev/final layers with Docker format, and tags only `localhost/phlex-ci:<arch>-local`, `localhost/phlex-dev:<arch>-local`, and `localhost/phlex-devcontainer:<arch>-local`. Use separate arm64 cache state, architecture-appropriate act/tracebox artifacts or explicit opt-outs, and optional signing without keys. Document prerequisites, exact pasta and relay contracts, local image selection, read-only mounts, compiler behavior, cleanup ownership, stdio limits, optional act, and local-only image policy; link the guide from `scripts/README.md`.

#### Context

The guide must be executable without reconstructing omitted values and must distinguish ordinary development, optional nested Podman, and GitHub amd64 regression.

#### Acceptance criteria

- Architecture selection, dry-run arguments, and tags are deterministic for both architectures.
- `scripts/build-container-images.sh` is executable with mode `0755`.
- The guide contains all required commands, variables, failure boundaries, cleanup, and limitations.
- No image is pushed and no production relay state is created by tests.

#### Verification

- Run shell syntax, `test -x scripts/build-container-images.sh`, builder dry-run, relay-only dry-run, documentation-link, and scope checks; record the executable mode.
- Record the required documentation-section checklist and output hashes.

#### Retry

max_attempts: 2
strategy: resume_then_narrow

#### Idempotency

true

#### Gate

automatic

### 6. Validate static configuration and implementation scope

#### Depends on

- baseline
- 2
- 3a
- 3b
- 4
- 5

#### Executor

execute

#### Capabilities

none

#### Allowed files

- .kilo/plans/evidence/

#### Prohibited changes

- every source path named by implementation steps except the bound `.kilo/plans/evidence/<run-id>/` directory
- all paths in Exclusions
- hook mutations, commits, pushes, and patch transfers

#### Task

Run JSON and both explicit Compose resolutions with the pinned `podman-compose` provider, pasta and socket checks, shell syntax and `shellcheck` for every changed shell file, the full relay matrix, builder dry-run, documentation checklist, and non-mutating hooks in an invocation-unique disposable copy. Set `PHLEX_RELAY_STATE_DIR` to an invocation-unique external test directory, never `$HOME/.phlex-devcontainer-tmp/relays`, and run the complete relay matrix there. Compare the final manifest with `baseline`, tolerate only the two recorded unrelated script changes at their original hashes, and require `.devcontainer/post-create.sh` and `CMakePresets.json` to retain their baseline hashes. Write the exact final `git status --short` to `PHLEX_EVIDENCE_DIR/step-6-final-status` only after all scope checks pass. Preserve pre-existing changes; never restore whole directories.

#### Context

This is the last automatic gate before end-to-end validation. Existing GitHub CI remains the amd64 regression path; no unavailable remote node or emulated build is substituted.

#### Acceptance criteria

- All static, dry-run, hook, documentation, scope, and prohibited-path checks pass.
- Only the allowed implementation union plus unchanged pre-existing paths remains.
- The relay matrix leaves no production `relay-map.json` or `relay-map.env` and no test relay listener under the default relay state directory.
- No hook changed the workspace and all evidence artifacts have verified hashes.

#### Verification

- Assert the baseline-bound `PHLEX_RUN_ID` and `PHLEX_EVIDENCE_DIR` are unchanged, then recompute before/after status and SHA-256 manifests and fail on any unexpected path or hash.
- Record the deterministic verification report, artifact manifest, and final-status receipt consumed by step 7.

#### Retry

max_attempts: 1
strategy: abort

#### Idempotency

true

#### Gate

automatic

### 7. Perform native arm64 end-to-end acceptance and outcome review

#### Depends on

- 6

#### Executor

human

#### Capabilities

none

#### Allowed files

- .kilo/plans/evidence/

#### Prohibited changes

- tracked repository files during validation except the bound `.kilo/plans/evidence/<run-id>/` directory
- unowned processes, host interfaces, rootful configuration, and TCP Podman APIs

#### Task

From the workspace root, source only the immutable step-1 values, export `PHLEX_DEV_NETWORK_MODE=pasta`, local arm64 image tags, and `25114=25115,25300=25301`. Run the typed phases in order: build and inspect all three native arm64 images; create the devcontainer; verify exact read-only mounts, the optional existing nested-Podman mechanism, Clang/GCC environment, unchanged presets, actual C++23 configure/build/CTest, explicit GCC coverage configure, developer tools, named controlled services, owned relays, deterministic maps, JSON/JSONC rewriting, negative unlisted-port behavior, and in-container stdio MCP/LSP boundaries; then launch VS Code. Between `verify-runtime` and `launch-vscode`, record the SHA-256 of `~/Library/Application Support/Code/User/settings.json` or record `state: absent`, add `"dev.containers.dockerPath": "podman"` through the VS Code settings UI or JSONC-safe editor, and record the post-change hash. After `verify-relay`, run `cleanup`; it stops only PID-file and command-identity-proven owned services and relays, tears down the devcontainer, and verifies no owned listener remains. Restore the settings file to its recorded pre-change bytes or absent state after `launch-vscode` and before `record-metrics`, then write the measured metric table to `$PHLEX_EVIDENCE_DIR/metrics.tsv`, deviations to `deviations.md`, missing measurements to `missing-measurements.md`, source/state/catalog bindings to `bindings.md`, evidence-mode and M8 claim limits to `evidence-mode.md` with literal `mode=evidence_only_case_study`, residual work to `residual-work.md`, and risks to `risks.md`; use `unavailable` rather than inventing values. The `record-metrics` phase then writes the M8 report and artifact manifest under the same run directory with exactly one recommendation from `none`, `revise_and_reexecute`, `create_supplemental_plan`, or `human_review`.

#### Context

Recorded host values are immutable inputs. A stale value routes back to step 1; step 7 may not choose a replacement gateway, socket, or address. Network-backed AI availability is reported separately from image correctness. The existing proxy socket grants rootless container-runtime control of the VM and is optional trusted host access; ordinary development acceptance does not require it. This remains an evidence-only M8 case study unless a separately retained matched native conversational Plan-to-implementation baseline is executed.

#### Acceptance criteria

- Images, devcontainer, compiler environment, actual build/tests, relay path, and cleanup pass natively on rootless arm64.
- No amd64 selection, prohibited path change, wildcard relay, unlisted-port reachability, or undeclared host configuration write occurs; the declared VS Code setting edit is hash-recorded and restored byte-for-byte or to its recorded absent state.
- Evidence covers every acceptance condition and terminal outcome is one of the four allowed classifications.
- `metrics.tsv` contains every key in the `metric-schema` Record as a tab-delimited nonempty value, using literal `unavailable` when a measurement cannot be obtained.
- The M8 report names supported questions and does not claim criteria 14–15 or global criterion 17 without matched comparison and independent alignment review.

#### Verification

- Run `preflight`, recorded-host comparison against the step-1 values, real image build/inspection, devcontainer creation/mount inspection, runtime build/test, relay/Kilo positive and negative checks, `cleanup`, and final `verify` in order.
- Require exit 0 for every command, zero final failures, explicit approval, complete `{path, sha256}` evidence, and clean prohibited-path assertions.
- Record task/run identity, conditions, planning/execution times, dispatches, attempts, retries, reviews, human intervention, verification failures, escaped defects, recovery, available cost, cost per verified step and achieved goal, deviations, missing measurements, residual work, and one recommendation.

#### Retry

max_attempts: 1
strategy: abort

#### Idempotency

false

#### Gate

human

#### Recovery

If the settings file or relay maps differ from their recorded post-change or pre-change hashes, stop and require conflict-specific human review. Invoke `rollback-vscode-settings` only after VS Code is exited; it restores the recorded settings bytes or absent state and refuses an unrecognized current hash. Relay cleanup refuses pre-existing production maps and removes only maps created by this gate.

#### Human runner

{
  "script": "native-arm64-devcontainer.sh",
  "log_policy": "suppressed",
  "cwd": "/Users/greenc/work/cet-is/sources/phlex/phlex",
  "environment": ["PHLEX_RUN_ID", "PHLEX_EVIDENCE_DIR", "PHLEX_HOST_GATEWAY", "PHLEX_PODMAN_SOCKET_SOURCE", "PHLEX_PODMAN_SOCKET_KIND", "PHLEX_VSCODE_SETTINGS", "PHLEX_VSCODE_EXITED", "PHLEX_DEV_NETWORK_MODE", "PHLEX_DEV_BASE_IMAGE", "PHLEX_DEV_CONTAINER_IMAGE", "PHLEX_TEST_RELAY_PORTS", "PHLEX_HOST_RELAY_PORTS", "PHLEX_OUTCOME", "PHLEX_RECOMMENDATION"],
  "owned_paths": [".kilo/plans/evidence"],
  "cleanup_paths": [],
  "phases": [
    {"id": "preflight", "command": "test -n \"$PHLEX_RUN_ID\" && test -n \"$PHLEX_EVIDENCE_DIR\" && test \"$PHLEX_EVIDENCE_DIR\" = \"$PWD/.kilo/plans/evidence/$PHLEX_RUN_ID\" && test -s \"$PHLEX_EVIDENCE_DIR/step-1-values.env\" && test ! -e \"$HOME/.phlex-devcontainer-tmp/relays/relay-map.json\" && test ! -e \"$HOME/.phlex-devcontainer-tmp/relays/relay-map.env\" && printf 'state: absent\\n' > \"$PHLEX_EVIDENCE_DIR/step-7-relay-map-pre\" && command -v podman && command -v podman-compose && command -v devcontainer && command -v jq && command -v code && test -x scripts/build-container-images.sh", "expected_exit": 0, "expected_output": "none", "remediation": "Stop if a pre-existing relay map exists; do not overwrite an active session's state.", "abort": "abort"},
    {"id": "verify-recorded-host", "command": "set -a && . \"$PHLEX_EVIDENCE_DIR/step-1-values.env\" && set +a && podman info --format '{{.Host.Arch}} {{.Host.Security.Rootless}}' | grep -E '^(arm64|aarch64) true$' && live_socket=$(podman machine inspect --format '{{.ConnectionInfo.PodmanSocket.Path}}') && test \"$live_socket\" = \"$PHLEX_PODMAN_SOCKET_SOURCE\" && live_gateway=$(podman run --rm --network pasta docker.io/library/alpine:3.22 sh -c 'getent hosts host.docker.internal >/dev/null && printf host.docker.internal || printf host.containers.internal') && test \"$live_gateway\" = \"$PHLEX_HOST_GATEWAY\"", "expected_exit": 0, "expected_output": "none", "remediation": "Stop and rerun step 1; compare live socket and gateway values rather than selecting replacements.", "abort": "abort"},
    {"id": "build-images", "command": "scripts/build-container-images.sh && test \"$(podman image inspect --format '{{.Architecture}}' localhost/phlex-ci:arm64-local)\" = arm64 && test \"$(podman image inspect --format '{{.Architecture}}' localhost/phlex-dev:arm64-local)\" = arm64 && test \"$(podman image inspect --format '{{.Architecture}}' localhost/phlex-devcontainer:arm64-local)\" = arm64 && printf 'images-arm64-local\\n' > \"$PHLEX_EVIDENCE_DIR/step-7-receipts\"", "expected_exit": 0, "expected_output": "none", "remediation": "Stop and record architecture, cache, tag, or package failure; never use amd64 fallback.", "abort": "abort"},
    {"id": "create-devcontainer", "command": "devcontainer up --workspace-folder \"$PWD\" --config .devcontainer/devcontainer.json > \"$PHLEX_EVIDENCE_DIR/container-result.json\" && jq -r '.containerId' \"$PHLEX_EVIDENCE_DIR/container-result.json\" > \"$PHLEX_EVIDENCE_DIR/container-id\" && test -s \"$PHLEX_EVIDENCE_DIR/container-id\"", "expected_exit": 0, "expected_output": "none", "remediation": "Stop and record the Dev Containers, Compose, or mount failure.", "abort": "abort"},
    {"id": "verify-runtime", "command": "container=$(cat \"$PHLEX_EVIDENCE_DIR/container-id\"); test \"$(podman inspect \"$container\" --format '{{range .Mounts}}{{if eq .Destination \"/root/.config/kilo\"}}{{.RW}}{{end}}{{end}}')\" = false && test \"$(podman inspect \"$container\" --format '{{range .Mounts}}{{if eq .Destination \"/run/phlex-host-relays\"}}{{.RW}}{{end}}{{end}}')\" = false && podman exec \"$container\" sh -lc 'test \"$CC\" = clang && test \"$CXX\" = clang++ && printf \"\" | c++ -x c++ -std=c++23 -fsyntax-only - && cmake --preset default -B build && cmake --build build && ctest --test-dir build --output-on-failure && cmake --preset coverage-gcc -B build-coverage-gcc' && printf 'mounts-rw-false\\ncompiler-clang\\ncxx23\\nconfigure-build-ctest\\ncoverage-gcc\\n' >> \"$PHLEX_EVIDENCE_DIR/step-7-receipts\"", "expected_exit": 0, "expected_output": "none", "remediation": "Stop and record mount, compiler, configure, build, test, or preset failure.", "abort": "abort"},
    {"id": "record-vscode-settings", "command": "settings=\"${PHLEX_VSCODE_SETTINGS:-$HOME/Library/Application Support/Code/User/settings.json}\"; test ! -e \"$PHLEX_EVIDENCE_DIR/step-7-vscode-settings-pre.sha256\" && test ! -e \"$PHLEX_EVIDENCE_DIR/step-7-vscode-settings-pre.bytes\" && if test -f \"$settings\"; then shasum -a 256 \"$settings\" > \"$PHLEX_EVIDENCE_DIR/step-7-vscode-settings-pre.sha256\" && cp -p \"$settings\" \"$PHLEX_EVIDENCE_DIR/step-7-vscode-settings-pre.bytes\"; else printf 'state: absent\\n' > \"$PHLEX_EVIDENCE_DIR/step-7-vscode-settings-pre.sha256\"; fi", "expected_exit": 0, "expected_output": "none", "remediation": "Stop and refuse a rerun that would overwrite the pre-change settings evidence.", "abort": "abort"},
    {"id": "verify-vscode-setting", "command": "settings=\"${PHLEX_VSCODE_SETTINGS:-$HOME/Library/Application Support/Code/User/settings.json}\"; test -f \"$settings\" && grep -E 'dev\\.containers\\.dockerPath.*podman' \"$settings\" && if grep -Fx 'state: absent' \"$PHLEX_EVIDENCE_DIR/step-7-vscode-settings-pre.sha256\"; then :; else test \"$(shasum -a 256 \"$settings\" | cut -d' ' -f1)\" != \"$(cut -d' ' -f1 \"$PHLEX_EVIDENCE_DIR/step-7-vscode-settings-pre.sha256\")\"; fi && shasum -a 256 \"$settings\" > \"$PHLEX_EVIDENCE_DIR/step-7-vscode-settings-post.sha256\"", "expected_exit": 0, "expected_output": "none", "remediation": "Apply the declared setting interactively and record a changed post-edit hash before launching VS Code.", "abort": "abort"},
    {"id": "launch-vscode", "command": "code --reuse-window .", "expected_exit": 0, "expected_output": "none", "remediation": "Stop and record the VS Code Dev Containers launch failure.", "abort": "abort"},
    {"id": "await-vscode-exit", "command": "test \"$PHLEX_VSCODE_EXITED\" = true && printf 'vscode-exit-confirmed>>\\n' >> \"$PHLEX_EVIDENCE_DIR/step-7.md\"", "expected_exit": 0, "expected_output": "none", "remediation": "Close VS Code and set PHLEX_VSCODE_EXITED=true before invoking this phase. The operator must manually close VS Code and export this value to proceed.", "abort": "abort"},
    {"id": "rollback-vscode-settings", "command": "settings=\"${PHLEX_VSCODE_SETTINGS:-$HOME/Library/Application Support/Code/User/settings.json}\"; test \"$PHLEX_VSCODE_EXITED\" = true && if test -f \"$settings\"; then test \"$(shasum -a 256 \"$settings\" | cut -d' ' -f1)\" = \"$(cut -d' ' -f1 \"$PHLEX_EVIDENCE_DIR/step-7-vscode-settings-post.sha256\")\"; fi && if grep -Fx 'state: absent' \"$PHLEX_EVIDENCE_DIR/step-7-vscode-settings-pre.sha256\"; then rm -f \"$settings\"; else cp -p \"$PHLEX_EVIDENCE_DIR/step-7-vscode-settings-pre.bytes\" \"$settings\"; fi", "expected_exit": 0, "expected_output": "none", "remediation": "Exit VS Code and refuse restoration when the current hash is not the recorded post-edit hash.", "abort": "abort"},
    {"id": "start-services", "command": "PHLEX_TEST_RELAY_PORTS=25114=25115,25300=25301 PHLEX_HOST_RELAY_PORTS=25114=25115,25300=25301 PHLEX_RELAY_STATE_DIR=\"$HOME/.phlex-devcontainer-tmp/relays\" .devcontainer/ensure-repos.sh --start-test-services > \"$PHLEX_EVIDENCE_DIR/step-7-owned-services\" && test -s \"$PHLEX_EVIDENCE_DIR/step-7-owned-services\"", "expected_exit": 0, "expected_output": "none", "remediation": "Stop if either named service or its command-identity proof is missing.", "abort": "abort"},
    {"id": "verify-relay", "command": "PHLEX_TEST_RELAY_PORTS=25114=25115,25300=25301 PHLEX_HOST_RELAY_PORTS=25114=25115,25300=25301 PHLEX_RELAY_STATE_DIR=\"$HOME/.phlex-devcontainer-tmp/relays\" .devcontainer/ensure-repos.sh --self-test && .devcontainer/kilo-env.sh --self-test && test -s \"$HOME/.phlex-devcontainer-tmp/relays/relay-map.json\" && test -s \"$HOME/.phlex-devcontainer-tmp/relays/relay-map.env\" && printf 'relay-map-deterministic\\nkilo-json-jsonc\\nunlisted-port-refused\\nstdio-boundary\\n' >> \"$PHLEX_EVIDENCE_DIR/step-7-receipts\"", "expected_exit": 0, "expected_output": "none", "remediation": "Stop and record relay, map, Kilo, negative-port, or process-boundary failure.", "abort": "abort"},
    {"id": "cleanup", "command": "PHLEX_RELAY_STATE_DIR=\"$HOME/.phlex-devcontainer-tmp/relays\" .devcontainer/ensure-repos.sh --cleanup-owned && test -s \"$PHLEX_EVIDENCE_DIR/step-7-owned-services\" && while IFS='|' read -r pid service args; do test -n \"$pid\" && test -n \"$service\" && test -n \"$args\" && { if ps -p \"$pid\" -o args= >/dev/null 2>&1; then ps -p \"$pid\" -o args= | grep -F \"$args\" && kill \"$pid\"; fi; }; done < \"$PHLEX_EVIDENCE_DIR/step-7-owned-services\" && devcontainer down --workspace-folder \"$PWD\" && test -z \"$(podman ps -aq --filter label=devcontainer.local_folder=\"$PWD\")\" && ! lsof -nP -iTCP:25114 -sTCP:LISTEN && ! lsof -nP -iTCP:25300 -sTCP:LISTEN && rm -f \"$HOME/.phlex-devcontainer-tmp/relays/relay-map.json\" \"$HOME/.phlex-devcontainer-tmp/relays/relay-map.env\" && test ! -e \"$HOME/.phlex-devcontainer-tmp/relays/relay-map.json\" && test ! -e \"$HOME/.phlex-devcontainer-tmp/relays/relay-map.env\" && printf 'cleanup\\n' >> \"$PHLEX_EVIDENCE_DIR/step-7-receipts\" && : > \"$PHLEX_EVIDENCE_DIR/step-7-owned-services\"", "expected_exit": 0, "expected_output": "none", "remediation": "Stop if any PID, service, or command identity is malformed or not owned; an already-exited owned PID is a pass, but unowned processes are never killed.", "abort": "abort"},
    {"id": "record-metrics", "command": "test -s \"$PHLEX_EVIDENCE_DIR/step-1-values.env\" && test -s \"$PHLEX_EVIDENCE_DIR/step-6-final-status\" && test -s \"$PHLEX_EVIDENCE_DIR/metrics.tsv\" && test -s \"$PHLEX_EVIDENCE_DIR/deviations.md\" && test -s \"$PHLEX_EVIDENCE_DIR/missing-measurements.md\" && test -s \"$PHLEX_EVIDENCE_DIR/bindings.md\" && test -s \"$PHLEX_EVIDENCE_DIR/evidence-mode.md\" && test -s \"$PHLEX_EVIDENCE_DIR/residual-work.md\" && test -s \"$PHLEX_EVIDENCE_DIR/risks.md\" && for key in task_identity run_identity environment_model_conditions planning_elapsed execution_elapsed planning_questions dispatches substantive_attempts retries revisions review_passes human_intervention verification_failures escaped_defects recovery_time available_cost cost_per_verified_step cost_per_achieved_goal deviations missing_measurements terminal_outcome; do awk -F '\\t' -v key=\"$key\" '$1 == key && $2 != \"\" {found=1} END {exit !found}' \"$PHLEX_EVIDENCE_DIR/metrics.tsv\"; done && test \"$PHLEX_OUTCOME\" = achieved -o \"$PHLEX_OUTCOME\" = partially_achieved -o \"$PHLEX_OUTCOME\" = not_achieved -o \"$PHLEX_OUTCOME\" = inconclusive && test \"$PHLEX_RECOMMENDATION\" = none -o \"$PHLEX_RECOMMENDATION\" = revise_and_reexecute -o \"$PHLEX_RECOMMENDATION\" = create_supplemental_plan -o \"$PHLEX_RECOMMENDATION\" = human_review && { cat \"$PHLEX_EVIDENCE_DIR/evidence-mode.md\"; printf 'task/run identity\\nconditions\\n'; cat \"$PHLEX_EVIDENCE_DIR/metrics.tsv\"; printf 'deviations\\n'; cat \"$PHLEX_EVIDENCE_DIR/deviations.md\"; printf 'missing measurements\\n'; cat \"$PHLEX_EVIDENCE_DIR/missing-measurements.md\"; printf 'residual work\\n'; cat \"$PHLEX_EVIDENCE_DIR/residual-work.md\"; printf 'risks\\n'; cat \"$PHLEX_EVIDENCE_DIR/risks.md\"; printf 'bindings\\n'; cat \"$PHLEX_EVIDENCE_DIR/bindings.md\"; printf 'outcome=%s\\nrecommendation=%s\\n' \"$PHLEX_OUTCOME\" \"$PHLEX_RECOMMENDATION\"; } > \"$PHLEX_EVIDENCE_DIR/1787581717167-m8-case-study.md\" && find \"$PHLEX_EVIDENCE_DIR\" -type f ! -name artifact-manifest.sha256 -print0 | LC_ALL=C sort -z | xargs -0 shasum -a 256 > \"$PHLEX_EVIDENCE_DIR/artifact-manifest.sha256\" && printf 'metrics-recorded\\n' >> \"$PHLEX_EVIDENCE_DIR/step-7-receipts\"", "expected_exit": 0, "expected_output": "none", "remediation": "Stop and complete every metric key, evidence mode, deviations, bindings, residual work, risks, and terminal review before verification.", "abort": "abort"},
    {"id": "verify", "command": "settings=\"${PHLEX_VSCODE_SETTINGS:-$HOME/Library/Application Support/Code/User/settings.json}\"; test -s \"$PHLEX_EVIDENCE_DIR/step-1-values.env\" && test -s \"$PHLEX_EVIDENCE_DIR/step-6-final-status\" && test \"$(git status --short)\" = \"$(cat \"$PHLEX_EVIDENCE_DIR/step-6-final-status\")\" && test ! -s \"$PHLEX_EVIDENCE_DIR/step-7-owned-services\" && for label in images-arm64-local mounts-rw-false compiler-clang cxx23 configure-build-ctest coverage-gcc relay-map-deterministic kilo-json-jsonc unlisted-port-refused stdio-boundary cleanup metrics-recorded; do grep -Fx \"$label\" \"$PHLEX_EVIDENCE_DIR/step-7-receipts\"; done && for key in task_identity run_identity environment_model_conditions planning_elapsed execution_elapsed planning_questions dispatches substantive_attempts retries revisions review_passes human_intervention verification_failures escaped_defects recovery_time available_cost cost_per_verified_step cost_per_achieved_goal deviations missing_measurements terminal_outcome; do awk -F '\\t' -v key=\"$key\" '$1 == key && $2 != \"\" {found=1} END {exit !found}' \"$PHLEX_EVIDENCE_DIR/metrics.tsv\"; done && if grep -Fx 'state: absent' \"$PHLEX_EVIDENCE_DIR/step-7-vscode-settings-pre.sha256\"; then test ! -e \"$settings\"; else test \"$(shasum -a 256 \"$settings\" | cut -d' ' -f1)\" = \"$(cut -d' ' -f1 \"$PHLEX_EVIDENCE_DIR/step-7-vscode-settings-pre.sha256\")\"; fi && test -s \"$PHLEX_EVIDENCE_DIR/1787581717167-m8-case-study.md\" && test -s \"$PHLEX_EVIDENCE_DIR/artifact-manifest.sha256\" && grep -Fx 'mode=evidence_only_case_study' \"$PHLEX_EVIDENCE_DIR/evidence-mode.md\" && grep -E '^(outcome|recommendation)=' \"$PHLEX_EVIDENCE_DIR/1787581717167-m8-case-study.md\"", "expected_exit": 0, "expected_output": "none", "remediation": "Stop without approval and record the missing receipt, metric key, manifest, baseline match, restored settings, or prohibited-path result.", "abort": "abort"}
  ]
}

## Records

### legacy-artifacts. Legacy artifact inventory and archive boundary

#### Kind

archive_boundary

#### Applies to

- archive-legacy

#### Content

The exact pre-v4 Markdown source is archived at `.kilo/plans/1787581717167-native-arm64-podman-devcontainers.legacy-source.md`. The retired JSON artifacts are the `.exec.json` and four `state.stale-*.json` files; they are copied, not deleted, into `.kilo/plans/legacy-archive/` with a SHA-256 receipt. They are non-dispatchable evidence. No `.exec.json` or legacy state is parsed, compiled, migrated, resumed, or reset.

### recovery-provenance. Prior partial execution and safe recovery

#### Kind

recovery_posture

#### Applies to

- baseline
- 2
- 3a
- 3b
- 6

#### Content

The prior execution approved the host gate, completed the four CI files, and aborted the combined relay/Kilo dispatch after partial changes in `.devcontainer/ensure-repos.sh` and `.devcontainer/kilo-env.sh`. Baseline captures those hashes and unrelated changes in `scripts/git-ai-commit` and `scripts/test/test_git_ai_commit.py`; no whole-directory restore is permitted. The aborted combined dispatch consumed the one remaining attempt charged to step 3a, so this source sets step 3a `max_attempts: 1`; step 3b retains its full budget. An out-of-scope path is a blocking failure. Invalid old artifacts are archived, not reset.

### design-boundaries. Architecture, security, and compatibility decisions

#### Kind

decision_boundary

#### Applies to

- 1
- 2
- 3a
- 3b
- 4
- 5
- 6
- 7

#### Content

Darwin uses native arm64, rootless Podman, exact pasta, loopback listeners, the documented Podman gateway, and the machine's local connection socket. Relay state is `$HOME/.phlex-devcontainer-tmp/relays/relay-map.json` and `relay-map.env`, mounted read-only at `/run/phlex-host-relays` with those exact filenames. Linux preserves existing relay/proxy behavior. The optional existing `${HOME}/.podman-proxy/podman.sock:/tmp/podman.sock` mount is trusted host access that grants rootless container-runtime control of the VM; ordinary development does not require it and no new bridge or TCP API is allowed. Relays are explicit numeric maps, deterministic, ownership-cleaned, and never wildcard. Kilo is mounted read-only and rewritten in memory; stdio MCP/LSP and host Unix sockets are not tunneled. CMake presets, workflows, `.actrc`, rootfulness, and host interfaces remain outside scope.

### test-fixtures. Deterministic probe and relay fixtures

#### Kind

test_fixture

#### Applies to

- 1
- 3a
- 7

#### Content

The direct probe reserves `127.0.0.1:25110`, the Compose probe reserves `127.0.0.1:25112`, and the required token is `phlex-probe-ok`. Step 7 starts only gate-owned `phlex-relay-test-model` at `127.0.0.1:25114` and `phlex-relay-test-mcp` at `127.0.0.1:25300`; both return fixed non-secret health responses. `--start-test-services` writes one line per service as `<pid>|<service-name>|<exact-command-identity>`, with all three fields nonempty and the identity matching `ps -o args=` for that PID. `--cleanup-owned` cleans relay PIDs only and leaves these two service PIDs for the runner cleanup phase. Exact mappings are `25114=25115,25300=25301` and are written to both relay environment variables. The runner performs the explicit interactive VS Code settings action described by step 7, launches `code --reuse-window .`, records service/relay PIDs and ownership proofs, and records final `git status --short`.

### m8-evidence. M8 evidence mode and recording protocol

#### Kind

m8_evidence_protocol

#### Applies to

- archive-legacy
- baseline
- 1
- 2
- 3a
- 3b
- 4
- 5
- 6
- 7

#### Content

This is an evidence-only representative-task case study, not a superiority claim or completed matched benchmark. The durable evidence root is `.kilo/plans/evidence/<run-id>/`; retain it through independent M8 review and do not delete it as ordinary temporary state. Preserve source bytes and hash, canonical digest, direct-state and worker-catalog bindings, runner hashes, receipts, output manifests, sanitized logs, task/run identity, conditions, timing, available cost, cost per verified step and achieved goal, dispatches, attempts, retries, revisions, reviews, human intervention, failures, escaped defects, recovery, deviations, missing measurements, and terminal outcome together. Criteria 1–13 and 16 may be supported; criteria 14–15 require a future equivalent native conversational Plan-to-implementation comparison; global criterion 17 requires independent alignment review. Missing matched evidence is `inconclusive`.

### metric-schema. Required M8 metric keys

#### Kind

metric_schema

#### Applies to

- 7

#### Content

The first column of `metrics.tsv` must contain each key exactly once, followed by a tab and a nonempty value. Permitted values include the literal `unavailable` when measurement is not possible. Required keys are `task_identity`, `run_identity`, `environment_model_conditions`, `planning_elapsed`, `execution_elapsed`, `planning_questions`, `dispatches`, `substantive_attempts`, `retries`, `revisions`, `review_passes`, `human_intervention`, `verification_failures`, `escaped_defects`, `recovery_time`, `available_cost`, `cost_per_verified_step`, `cost_per_achieved_goal`, `deviations`, `missing_measurements`, and `terminal_outcome`.

### evidence-storage. Run-unique external evidence retention

#### Kind

evidence_storage

#### Applies to

- baseline
- 1
- 6
- 7

#### Content

The `baseline` step owns creation of `PHLEX_RUN_ID` and `PHLEX_EVIDENCE_DIR` as `.kilo/plans/evidence/<run-id>/` using an invocation-unique run ID and records both in direct state before step 1. Later steps must consume that exact binding and fail if their environment differs. The directory is retained through independent M8 review. Every step receipt, values file, manifest, report, and evidence artifact is written inside that run directory and recorded in direct state as `{path, sha256}`. Fixed step-1/step-7 filenames are permitted only inside this unique run directory.

### validation-binding. Deterministic validation and execution handoff

#### Kind

validation_protocol

#### Applies to

- archive-legacy
- baseline
- 6
- 7

#### Content

Before execution and every resume, run `python3 ~/.config/kilo/skill/execution-plans/scripts/direct_state.py preflight` with the current worker catalog and config. After every dispatch, gate, result, failure, or review, checkpoint through `direct_state.py checkpoint` with a unique candidate. The raw SHA-256 binds exact approved UTF-8 bytes; the canonical normalized digest binds ordered parsed semantics excluding source-line metadata and presentation-only placement. Keep them separate. Never create a compiled JSON twin; any source, parser, catalog, workspace, or normalized-plan change requires a new approved source.

## Review checkpoints

### implementation-alignment. Independent implementation alignment review

#### After steps

- 2
- 3a
- 3b
- 4
- 5

#### Gates steps

- 6
- 7

#### Reviewer

implementation-reviewer

#### Trigger

After implementation steps pass and before static validation or the end-to-end human gate, dispatch the catalog-authorized read-only reviewer. Preserve every finding and hash. A blocking finding gates only steps 6 and 7 and is resolved only by approved repair with fresh verification or a new approved source; findings are never downgraded or deleted.

## Outcome criteria

- The legacy source and retired JSON artifacts are preserved with verified hashes, and direct execution uses only this v4 source and source-suffixed state.
- Native rootless arm64 Podman, exact pasta probes, architecture-aware images, local selection, Clang defaults, unchanged presets, and native build/test are evidenced.
- Relay allowlisting, deterministic maps, ownership-safe cleanup, read-only mounts, write-free Kilo rewriting, negative unlisted-port checks, and stdio boundaries are evidenced.
- The final report has receipt-grade hashes, complete metrics, deviations, missing measurements, terminal outcome, residual work, risks, and exactly one closed-set recommendation.
- The report is labelled an evidence-only M8 case study unless a retained matched baseline and independent alignment review justify a comparative benchmark claim.
