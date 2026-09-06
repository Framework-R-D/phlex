---
format: kilo-plan/v4
plan_id: native-arm64-podman-continuation
workspace_root: /Users/greenc/work/cet-is/sources/phlex/phlex
design_review_status: pending
design_review_blocking_findings: 0
max_total_dispatches: 5
max_consecutive_failures: 2
max_alignment_review_dispatches: 1
---
# Native arm64 Podman continuation

## Goal

Repair the exhausted step-2 architecture and compiler configuration under a new approved direct-execution source, independently verify the static contract, implement the allowlisted Darwin/Linux relay and write-free Kilo configuration changes, and pass the declared implementation-alignment review. Native Spack concretization, C++23 compilation, and end-to-end devcontainer acceptance remain explicitly deferred to the later native arm64 acceptance gate and are not claimed by this continuation.

## Scope

The continuation uses the current worktree and preserved v3 evidence as provenance, repairs `ci/Dockerfile`, `ci/spack.yaml`, `ci/packages.yaml`, and `ci/entrypoint.sh`, implements `.devcontainer/ensure-repos.sh` and `.devcontainer/kilo-env.sh`, records receipt-grade evidence under `.kilo/plans/evidence/<run-id>/`, and independently reviews the resulting implementation before the final static continuation verification.

## Exclusions

Do not resume or modify the v3 direct state, legacy state, `.exec.json` artifacts, or the Plan/Compile/Execute workflow. Do not modify Kilo configuration, `.github/workflows/*`, `.actrc`, `CMakePresets.json`, `scripts/git-ai-commit`, `scripts/test/test_git_ai_commit.py`, `.devcontainer/post-create.sh`, any host credential or socket path, or any path outside the step boundary. Do not initialize or mutate Podman machines, build or push images, use wildcard or TCP relays, create production relay fixtures during tests, commit, or push. The prior v3 source, state, and evidence are provenance only; this source creates its own direct state.

## Steps

### provenance. Bind preserved evidence without resuming exhausted state

#### Depends on

none

#### Executor

execute

#### Capabilities

- tool:bash

#### Allowed files

- .kilo/plans/evidence/

#### Prohibited changes

- all repository source and configuration paths
- all Kilo plan and state paths except the new evidence directory
- commits, pushes, machine mutation, and process-wide cleanup

#### Task

Create an invocation-unique continuation evidence directory and record the current source hash, worker-catalog binding, current worktree status, the v3 state status and counters, the v3 baseline manifest, and hashes for every implementation and protected path relevant to this continuation. Treat the v3 source, v3 state, and prior receipts as read-only provenance and reject any mismatch or missing receipt before dispatch. Verify the exact prior host-value artifact `.kilo/plans/evidence/20260904T151641Z-v3-545de7544a/step-1-values.env` and record its hash and freshness status; consume it only as prior evidence, never as a replacement for live host validation.

#### Context

The v3 direct state exhausted step 2 after two attempts and is not resumable for this repair. The preserved baseline is `.kilo/plans/evidence/20260904T151641Z-v3-545de7544a/`; the prior independent findings are under `.kilo/plans/evidence/step-2-orchestrator-verify.VW9TxVAU/`. A new source and state are required.

#### Acceptance criteria

- A unique evidence directory, provenance report, and hash manifest exist under `.kilo/plans/evidence/`.
- The report proves the old v3 state and evidence were read without being used as execution state.
- The current worktree status and all named baseline hashes are recorded before implementation dispatch.
- The exact prior host-value artifact and its hash are recorded, with live-host freshness marked `unavailable` unless independently revalidated.
- No repository path outside the new evidence directory changes.

#### Verification

- Recompute the provenance hashes and status immediately after capture.
- Require identical repeated results, valid receipt hashes, and an explicit matching result.

#### Retry

max_attempts: 1
strategy: abort

#### Idempotency

true

#### Gate

automatic

### remediate-2. Repair build-time architecture and compiler defaults

#### Depends on

- provenance

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
- CMakePresets.json and protected baseline paths
- image builds, pushes, machine mutation, and cross-architecture cache or binary reuse

#### Task

Repair the four CI files so the requested architecture is normalized before any architecture-dependent Spack operation. Declare and consume `PHLEX_SPACK_TARGET` in valid Docker build-argument scope, reject host/target mismatches before Spack cloning, installation, or concretization, map amd64 to `x86_64_v3` and arm64 to `aarch64`, and establish `PHLEX_LLVM_TARGET` as x86 or AArch64 before `spack env create` and concretization. Preserve the Fermilab mirror, GCC 15 ABI, act and tracebox architecture behavior, CI GCC default, developer Clang default, explicit GCC override, and reproducible Clang `--gcc-toolchain` binding. Keep existing valid constraints unless a minimal change is required to make the build-time values effective.

#### Context

The previous remediation added target normalization too late in the Dockerfile and did not set the LLVM target before concretization. The previous static review also found that the architecture guard ran after concretization. The current step must fix those specific defects without restoring unrelated historical content. The repair is convergent: it must establish declarative final values and may not append duplicate blocks on a rerun.

#### Acceptance criteria

- Both architecture inputs render only their required Spack target and LLVM target before all Spack constraints are evaluated.
- A mismatched native host and requested target fails before any architecture-dependent Spack operation.
- CI defaults to GCC and developer images default to Clang with GCC 15 toolchain binding; explicit GCC override remains functional.
- GCC 15 ABI, Fermilab mirror behavior, arm64 AArch64 LLVM support, and architecture-specific act/tracebox behavior remain present.
- `CMakePresets.json` and every protected baseline path remain byte-identical.

#### Verification

- Run `bash -n` and `shellcheck -S warning` on every changed shell file.
- Run a static Dockerfile-order and two-input rendering test proving argument scope, early guard, target normalization, LLVM normalization, compiler defaults, and mismatch rejection.
- Recompute protected baseline hashes and `git diff --check`.
- Record native Spack concretization and C++23 compile as `unavailable` on hosts lacking Spack; do not mark those runtime checks complete here.

#### Retry

max_attempts: 1
strategy: abort

#### Idempotency

true

#### Gate

automatic

### verify-2. Independently verify the repaired architecture contract

#### Depends on

- remediate-2

#### Executor

execute

#### Capabilities

- tool:bash

#### Allowed files

- .kilo/plans/evidence/

#### Prohibited changes

- every source path named by implementation steps
- CMakePresets.json, protected baseline paths, Kilo configuration, and machine state

#### Task

Independently inspect the repaired files rather than relying on the worker report. Produce receipt-grade evidence for Docker build-argument scope, early architecture validation, amd64 and arm64 normalized values, LLVM backend selection, compiler defaults, protected hashes, shell checks, and remaining unavailable runtime checks. Complete this step only when the static contract is proven and the deferred-runtime limitation is explicitly recorded.

#### Context

This step deliberately separates static implementation correctness from native runtime acceptance. The later native arm64 gate owns Spack concretization, C++23 compilation, image inspection, and devcontainer runtime evidence.

#### Acceptance criteria

- The static architecture/compiler contract passes with zero blocking findings.
- Protected baseline hashes and `CMakePresets.json` match the provenance manifest.
- All evidence files exist, are workspace-contained, and have matching SHA-256 hashes.
- Deferred native checks are listed as unavailable rather than represented as passed evidence.

#### Verification

- Run the complete declared static command matrix and require zero unexpected path changes.
- Validate every evidence hash and write an orchestrator verification record.

#### Retry

max_attempts: 1
strategy: abort

#### Idempotency

true

#### Gate

automatic

### 3a. Implement the allowlisted host relay contract

#### Depends on

- verify-2

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
- all paths in Exclusions
- wildcard listeners, port scans, TCP APIs, host interfaces, and production relay fixtures

#### Task

Preserve Linux behavior and existing Headroom variables while adding Darwin pasta support. Parse the complete numeric `source=relay` allowlist before starting anything; reject malformed, duplicate, privileged, equal, unavailable, and unlisted mappings; write the exact sorted JSON object and env lines from the `relay-interface` record; detect exact listeners with Darwin `lsof` or Linux `ss`; start socat listener-first; bind Darwin relays to loopback; record only ready relays; and clean only stale processes whose PID-file command identity proves ownership. Add `PHLEX_RELAY_STATE_DIR` with the declared default and implement `--start-test-services`, `--self-test`, and `--cleanup-owned`. Only when provenance marks the prior host-value artifact current may Darwin source those values and enforce the live socket or VM rootless guard; otherwise report the host-dependent result as `unavailable` and continue only with fixture-level checks.

#### Context

The relay interface is loopback-only pasta with exact numeric mappings from the `relay-interface` record. Prior partial changes in this file are preserved; provenance and baseline hashes establish ownership, so no whole-file restoration is allowed. The exact prior host-value artifact is usable only when provenance marks it current; otherwise host-dependent socket and rootless checks are recorded as `unavailable` and deferred. The implementation is convergent: repeated self-tests must converge on the same map and ownership state without duplicate listeners.

#### Acceptance criteria

- Valid mappings produce sorted deterministic maps only for ready explicitly listed services.
- Every invalid mapping class fails closed with empty maps and no relay process.
- Darwin uses loopback plus pasta and the recorded rootless socket; Linux behavior remains intact.
- Cleanup proves PID-file command identity and never stops an unrelated process.
- When current host values are unavailable, fixture-level behavior is evidenced without claiming live host or VM acceptance.

#### Verification

- Verify the provenance hash, run `bash -n` and `shellcheck -S warning`.
- Exercise positive, negative, unavailable, duplicate, privileged, equal-port, deterministic-map, owned-PID, socket-guard, named-service, self-test, and cleanup cases in an invocation-unique external test directory.
- Record pre/post hashes and all test output hashes.
- The worker records command outputs; the orchestrator writes the durable evidence manifest and direct-state receipt under the bound evidence directory.

#### Retry

max_attempts: 1
strategy: abort

#### Idempotency

true

#### Gate

automatic

### 3b. Implement write-free Kilo configuration rewriting

#### Depends on

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
- all paths in Exclusions
- host Kilo mounts, credentials, repository-local relay fixtures, and configuration writes

#### Task

Discover the read-only mounted Kilo JSON or JSONC file and the exact relay map interface. Parse JSONC in memory by removing comments and trailing commas only outside quoted strings, fail closed on malformed input, rewrite only HTTP(S) loopback URLs whose source ports are in the allowlist, preserve schemes and unrelated values, and export `KILO_CONFIG_CONTENT` without writing or backing up the host file. Implement `--self-test` with JSON, JSONC, malformed-input, loopback-only, map-driven, and no-write fixtures in a unique external directory.

#### Context

The relay map is mounted at `/run/phlex-host-relays` with exact filenames from the `relay-interface` record. The ordered read-only Kilo candidates are `/run/phlex-host-relays/kilo.json` and `/run/phlex-host-relays/kilo.jsonc`; relay maps are `/run/phlex-host-relays/relay-map.json` and `/run/phlex-host-relays/relay-map.env`. Stdio MCP, stdio LSP, and host Unix sockets are not tunneled. The worker verifies the interface contract with fixtures; the orchestrator owns durable receipt creation. The rewrite is convergent: repeated self-tests produce the same export and never write the mounted host file.

#### Acceptance criteria

- JSON and JSONC rewriting is in-memory, scheme-preserving, loopback-only, and map-driven.
- Non-loopback URLs and unmapped ports remain unchanged.
- Malformed input fails closed without partial export.
- No host Kilo file is written or backed up.
- The implementation consumes only the exact relay filenames and mappings from the `relay-interface` record.
- A fixture round-trip consumes a `3a`-produced map and verifies the expected `PHLEX_HOST_RELAY_<source-port>` exports before rewriting configuration.

#### Verification

- Verify the provenance hash, run `bash -n` and `shellcheck -S warning`.
- Run discovery, valid rewrite, unchanged-value, malformed-input, and no-write tests with external fixtures.
- Record fixture, output, and allowed-file assertion hashes.
- The worker records command outputs; the orchestrator writes the durable evidence manifest and direct-state receipt under the bound evidence directory.

#### Retry

max_attempts: 1
strategy: abort

#### Idempotency

true

#### Gate

automatic

### continuation-verify. Verify the implementation boundary before handoff

#### Depends on

- verify-2
- 3a
- 3b

#### Executor

execute

#### Capabilities

- tool:bash

#### Allowed files

- .kilo/plans/evidence/

#### Prohibited changes

- every implementation source path
- all paths in Exclusions
- commits, pushes, hook mutations, image operations, and machine mutation

#### Task

Run the static relay, Kilo, architecture, shell, scope, and protected-hash verification matrix in an invocation-unique disposable test directory. Compare the final status with provenance, preserve unrelated changes, and write a complete handoff receipt for the alignment review and the deferred native acceptance work.

#### Context

This is the final automatic step in this continuation. It does not substitute static evidence for native arm64 runtime acceptance and does not create M8 superiority claims.

#### Acceptance criteria

- All static implementation, scope, prohibited-path, relay, Kilo, and architecture checks pass.
- Only the declared implementation union plus preserved pre-existing paths remains.
- Evidence files have verified hashes and identify every unavailable runtime measurement.
- No production relay state, unowned listener, host Kilo write, commit, or push is created.
- The actual `3a` relay map is consumed by the `3b` fixture round-trip and its result is recorded.
- The bound run directory contains `metrics.tsv`, `evidence-mode.md`, `m8-case-study.md`, `residual-work.md`, and `risks.md`, each recorded with a matching `{path, sha256}` entry.

#### Verification

- Assert the continuation source and evidence binding, recompute all protected hashes, run the full static matrix, and require zero unexpected failures.
- Record the final status, evidence manifest, deferred-runtime handoff receipt, and the M8 artifact manifest from the exact `PHLEX_EVIDENCE_DIR` created by `provenance`.

#### Retry

max_attempts: 1
strategy: abort

#### Idempotency

true

#### Gate

automatic

## Records

### recovery-provenance. Prior execution and new-state boundary

#### Kind

recovery_posture

#### Applies to

- provenance
- remediate-2
- verify-2
- 3a
- 3b
- continuation-verify

#### Content

The v3 source and state are archival provenance only. The v3 step-2 attempts and receipts remain immutable evidence. This new source has a new plan identity and must create a new direct state; it must not import completed IDs, dispatch counters, reviewer evidence, or state JSON from v3. If any implementation step aborts after partial edits, leave those edits in place, record per-file pre/post hashes against the provenance manifest, and never wholesale-restore a file or directory; subsequent recovery requires a new approved source or an explicitly authorized repair.

### deferred-native. Deferred native acceptance boundary

#### Kind

validation_boundary

#### Applies to

- remediate-2
- verify-2
- 3a
- 3b
- continuation-verify

#### Content

Native Spack concretization, C++23 compilation, image architecture inspection, devcontainer creation, and end-to-end rootless arm64 acceptance are deferred to the later native human gate. For `3a` and `3b`, Darwin socket, pasta, mount, and rootless checks are fixture-level only unless the exact prior host-value artifact is independently revalidated in the current invocation. When unavailable, record `unavailable`; never convert missing runtime evidence into a pass or an M8 achieved claim.

### relay-interface. Shared relay and Kilo interface

#### Kind

interface_contract

#### Applies to

- 3a
- 3b
- continuation-verify

#### Content

The default relay state directory is `$HOME/.phlex-devcontainer-tmp/relays`. The producer writes the exact sorted JSON object `{ "<source-port>": "<relay-port>" }` to `relay-map.json` and one `PHLEX_HOST_RELAY_<source-port>=<relay-port>` line per mapping to `relay-map.env`, mounted read-only at `/run/phlex-host-relays`. The exact read-only Kilo configuration candidates are `/run/phlex-host-relays/kilo.json` followed by `/run/phlex-host-relays/kilo.jsonc`; they are distinct from the relay map files. The named test mappings are `25114=25115,25300=25301`; the `source=relay` input must be a complete numeric allowlist and is rejected when malformed, duplicate, privileged, equal, unavailable, or unlisted. Step 3a owns deterministic map production and step 3b consumes only these exact files and mappings.

### m8-evidence. Evidence-only M8 case study

#### Kind

m8_evidence_protocol

#### Applies to

- provenance
- remediate-2
- verify-2
- 3a
- 3b
- continuation-verify

#### Content

This continuation contributes only evidence to the existing representative-task case study. `provenance` creates `PHLEX_RUN_ID` and the absolute `PHLEX_EVIDENCE_DIR` under `.kilo/plans/evidence/<run-id>/`; every later step consumes that exact binding. Preserve source bytes and both bindings, direct-state receipts, worker receipts, verification artifacts, missing measurements, residual work, risks, deviations, environment and model conditions, and one closed-set recommendation. Record `metrics.tsv`, `evidence-mode.md`, `m8-case-study.md`, `residual-work.md`, and `risks.md` under that exact run directory with nonempty values for `task_identity`, `run_identity`, `environment_model_conditions`, `planning_elapsed`, `execution_elapsed`, `planning_questions`, `dispatches`, `substantive_attempts`, `retries`, `revisions`, `review_passes`, `human_intervention`, `verification_failures`, `escaped_defects`, `recovery_time`, `available_cost`, `cost_per_verified_step`, `cost_per_achieved_goal`, `deviations`, `missing_measurements`, and `terminal_outcome`, using literal `unavailable` when necessary. Record every artifact as `{path, sha256}`, write `evidence-mode.md` with literal `mode=evidence_only_case_study`, and produce `m8-case-study.md` with residual work, risks, exactly one terminal outcome classification, and exactly one recommendation. It is not a matched native conversational benchmark and cannot claim M8 criteria 14–15 or global criterion 17 without independent alignment review.

## Review checkpoints

### implementation-alignment. Independent implementation alignment review

#### After steps

- remediate-2
- 3a
- 3b

#### Gates steps

- continuation-verify

#### Reviewer

implementation-reviewer

#### Trigger

After the repaired architecture, relay, and Kilo implementations pass deterministic verification, dispatch the catalog-authorized read-only reviewer. Preserve all findings and evidence. A blocking finding stops continuation-verify and requires an approved repair or new approved source; it must never be downgraded or deleted.

## Outcome criteria

- A new v4 direct state binds the revised source without resuming the exhausted v3 state.
- The architecture/compiler static contract, relay contract, and write-free Kilo contract pass with receipt-grade evidence.
- The implementation-alignment review is recorded as clean or non-blocking before continuation-verify progresses.
- Native arm64 runtime checks remain explicitly deferred and are handed off without unsupported completion claims.
- The terminal outcome classification is exactly one of `achieved`, `partially_achieved`, `not_achieved`, or `inconclusive`; residual work and risks are recorded with the case-study report.
- The final recommendation is exactly one of `none`, `revise_and_reexecute`, `create_supplemental_plan`, or `human_review`.
