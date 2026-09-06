# Structured Plan Execution Recovery Improvement Plan

## Status

This is a human-readable engineering roadmap, not a canonical `kilo-plan` source and not an instruction to invoke the structured execution workflow. Implementation is paused pending a model change.

## Purpose

Improve structured plan execution so that recording, consistency checks, and fail-closed validation protect evidence while actively facilitating diagnosis, repair, and recovery. A consistency check should prevent unsafe continuation, but it must also preserve completed work, explain the precise incompatibility, and provide the narrowest safe recovery action. It must not turn irrelevant drift or a correctable bookkeeping defect into unnecessary loss of progress.

## Guiding principles

1. Preserve verified work by default. Never discard or repeat a completed step merely because unrelated configuration, catalog, or source content changed.
2. Fail closed only on execution-relevant incompatibility. Record broader environment changes for audit without treating all of them as admission failures.
3. Make rejection actionable. Every failed consistency check must report expected and actual values, classify the mismatch, identify preserved evidence, and name the next safe recovery operation.
4. Separate repair from restart. Rebinding compatible evidence, repairing bookkeeping, retrying a failed phase, superseding a source, and restarting a run are distinct operations.
5. Keep state authoritative without requiring humans or models to hand-author full state documents.
6. Record before side effects and durably after results. A crash between dispatch and result must leave enough information to verify or recover without blindly redispatching.
7. Treat human-runner phases as typed operations, not opaque shell strings.
8. Make recovery idempotent, bounded, reversible, and evidence-preserving.
9. Never weaken source, permission, model, non-idempotency, or evidence checks merely to make a run continue.
10. Do not modify the execution workflow from within an active execution run. Pause the run, repair the control plane separately, validate it, and reconcile the paused run explicitly.

## Time-and-motion baseline

The execution transcript recorded the following control-plane activity before the first substantive plan worker completed its dispatch:

| Measure | Observed count |
| --- | ---: |
| Canonical source versions | 3 |
| Visible preflights | 20 |
| Successful resets | 2 |
| Rejected checkpoint candidates | At least 8 |
| Catalog or configuration drift episodes | 2 |
| Human-runner materializations | 6 |
| Step-1 runner executions | At least 5 |
| Successful archive-gate executions | 3 |
| Baseline captures | 3 |
| `coder-qwen` delegations | 17 |
| Actual plan-step worker dispatches | 1 |
| Architect reviews of revised sources | 5 |
| Execution-time implementation-reviewer dispatches | 0 |

The safety checks frequently detected real inconsistencies, but recovery required repeated admission, source revision, baseline capture, gate execution, manual state construction, and catalog reconciliation. The desired workflow should retain the same safety guarantees with far less repeated motion.

## Defects already corrected

The following transcript-triggered defects are fixed in the current implementation and require permanent regression coverage:

- Empty human-runner environment arrays are guarded before Bash nounset expansion.
- Built-in `execute`, `orchestrator`, and `human` attempts are distinct from subagent dispatch accounting.
- Reset provenance remains immutable across later checkpoints.
- Planning-ledger re-admission and integrity-chain behavior have focused coverage.

These fixes should not be redesigned unless a replacement preserves their tested behavior.

## Remaining problem areas

### Effective configuration and model identity

The workflow does not materialize Kilo's merged effective configuration. A schema-only project overlay can hide valid global provider definitions, and model identity is represented ambiguously across provider key, model key, configured ID, display name, runtime ID, and variant.

### Overly coarse worker-catalog binding

The full configuration, provider listings, dispatcher, and all eligible workers contribute to the binding. An unrelated global change can therefore block a checkpoint after successful work. The rejection is safe, but its scope and recovery cost are disproportionate.

### Manual state candidate construction

Operators and orchestrators currently assemble full candidate state documents. Redundant fields such as step status, completed IDs, pending IDs, counts, dispatch records, attempts, gate decisions, and failure context can disagree and produce repeated checkpoint failures.

### Weak dispatch relationships

Dispatch validation does not fully prove that a receipt's agent, model identity, step, attempt, and executor agree. Error messages do not expose the expected and actual canonical identities.

### Incomplete state schema and reset integrity

Important structures remain weakly typed, including failure context, portions of reset provenance, step extensions, and goal review. Reset backups are referenced by path without binding their content hash.

### Human-runner observability and portability

Preflight can fail before normal diagnostic recording. Usage output omits valid phases. Reruns overwrite receipts. Phase ordering is not enforced. Validation does not execute a safe self-test. Bash and zsh boundaries are unclear, and macOS-specific process behavior is under-tested.

### Opaque shell commands in plan source

Long JSON strings containing shell programs produced escaping errors, hidden redirections, malformed commands, and review difficulty. Structural lint succeeded where runtime behavior was invalid.

### Evidence durability and compatibility

Planning artifacts and runner receipts can point into temporary directories. Compatible evidence has no supported import path after source amendment. Consequently, a verify-only correction can force repeated archive, baseline, and gate work.

### Planning admission gaps

Plan review did not deterministically detect impossible resource assumptions, phase-order contradictions, undeclared evidence production, shell portability defects, or weak cleanup ownership before execution.

### Operator experience

The operator was required to discover environment values, distinguish Bash from zsh syntax, avoid terminal-closing `exit`, interpret nearly empty logs, and copy long phase loops manually. The workflow exposed its implementation mechanics instead of presenting one safe next action.

## Supplemental execution-log findings

The supplemental session `session-ses_f886.md` was analyzed as an additive continuation of the original study. No structured-workflow implementation changes occurred between the sessions. Its findings strengthen the recovery-first conclusions rather than replacing the original baseline.

The supplemental session added 11 direct-state preflights, 11 checkpoint attempts, 4 rejected checkpoint candidates, 1 successful reset, 2 worker dispatches, 2 substantive attempts, 2 human-gate approvals, 2 runner materializations, 3 successful archive phases, 14 successful Podman-gate phases, 1 runner launch failure before any phase, 28 explicit reads, and 56 explicit shell calls. Its final state had only three completed steps and seven pending steps, with no alignment-review dispatch.

The most important additional finding is that fail-closed validation could prevent durable recording of the fact that recovery was blocked: the canonical state rejected both its worker-catalog mismatch and missing temporary receipt evidence, and the attempted failure checkpoint was rejected by the same recovery validation. Preflight therefore needs an independent recovery-diagnostic envelope that does not require a valid state candidate.

The reset operation also discarded compatible completed archive, baseline, and human-gate work after a catalog-only correction. The normal repair path must be `reconcile` or `rebind`, not `reset`, when source identity, plan behavior, workspace, and relevant step semantics are unchanged.

Authoritative state evidence must never depend on disposable `/tmp`, `/var/tmp`, or `/var/folders` artifacts. Receipts must be copied or emitted into the durable invocation evidence directory before approval, with the temporary runner identity retained separately.

The materializer returned an authoritative runner path that was manually mistyped before launch. The launcher must emit one copy-safe command derived from the exact path and hash, and the self-test must invoke that command verbatim. The operator must not reconstruct paths from memory.

Phase-produced values were incorrectly required as initial operator environment, including the host-remediation reason produced by `inspect-state`. Inputs must distinguish operator-supplied values from values produced by prior phases and carry the latter through typed dependencies.

The supplemental session also confirmed that worker success is not step success. A worker reported acceptance, but independent verification could not establish native arm64 concretization or the required C++23 compile because `spack` was unavailable and evidence paths and hashes were inconsistent. Verification must therefore produce `passed`, `inconclusive`, or `blocked`; a successful worker return cannot complete a step.

Finally, the supplemental session exposed effective compiler-default ambiguity between shared entrypoint defaults and per-image Docker defaults, zsh failures caused by reserved variable names and wildcard cleanup, and misleading hidden-file searches. Recovery diagnostics must use canonical absolute paths and state commands rather than generic hidden-file globbing.

These findings add the following non-negotiable rule: recording and consistency checks exist to preserve, explain, and repair progress. They may stop unsafe continuation, but they must not make a correctable mismatch unrecoverable or force repetition of compatible successful work.

## Target operating model

A run should use an immutable source, an immutable execution-engine snapshot, a narrowly scoped execution contract, durable evidence, and event-oriented state transitions.

The state engine should maintain two catalog identities:

- A full catalog snapshot retained for audit and diagnostics.
- A plan-relevant execution binding containing only referenced executors and reviewers, their resolved model identities, effective permissions, protected paths, required capabilities, and dispatcher rules.

Unrelated drift should be recorded and safely reconciled. Relevant drift should pause execution with an exact semantic diff and an explicit repair path.

## Implementation work packages

### 1. Effective configuration and canonical identities

Implement a read-only effective-config materializer using Kilo's actual global and project precedence rules, including JSONC parsing. Represent model identity as separate provider key, model key, configured ID, display name, runtime ID, and variant fields.

Retain raw input hashes for provenance, but bind execution to the canonical effective subset.

Acceptance criteria:

- A schema-only project overlay does not hide valid global models.
- `fnal-anthropic.models."opus-5"` resolves to its configured `azure/claude-opus-5` identity.
- Diagnostics distinguish missing configuration, unavailable runtime model, and identity mismatch.
- Repeated materialization of unchanged inputs is deterministic.

### 2. Plan-relevant catalog projection and drift classification

Retain the complete worker catalog as evidence but derive a smaller binding containing only workers and reviewers referenced by the source and the permission rules that affect their required operations.

Classify changes as:

- irrelevant observation drift;
- relevant but behavior-equivalent correction;
- relevant incompatible change;
- unavailable runtime dependency.

Acceptance criteria:

- Unrelated agent or provider changes do not invalidate state.
- Relevant executor, reviewer, model, capability, permission, or protected-path changes fail closed.
- The mismatch report lists changed fields and the permitted recovery operations.
- A successful gate result can still be durably recorded in quarantine when a binding mismatch is discovered, then reconciled without rerunning the gate.

### 3. Transactional state operations

Replace operator-authored full candidates with typed operations such as dispatch start, dispatch result, gate result, verification result, failure, pause, alignment review, reconcile, and reset.

The engine should derive completed and pending IDs, counts, step status, and consecutive failures. Raw candidate replacement may remain an internal testing interface but should not be part of normal operation.

Record a dispatch-start event before invoking a worker and complete it after the result. If the orchestrator stops between those points, resumption must classify the dispatch as in progress or uncertain and verify effects before retrying.

Acceptance criteria:

- No documented recovery requires editing complete state JSON.
- Repeating an event is idempotent or rejected with the existing event identity.
- A crash between dispatch and result is recoverable without automatic redispatch.
- Rejection diagnostics show expected and actual values for every mismatched field.
- State summaries are derived rather than independently authored.
- If canonical state cannot be loaded or reconciled, preflight writes a durable, invocation-unique recovery-diagnostic envelope containing source/state identity, expected and actual bindings, missing evidence, preserved evidence, and the exact permitted recovery operation without mutating canonical state.
- A worker receipt, human-gate receipt, or transport success never completes a step without independent orchestrator verification bound to the current source, run, baseline, and evidence hashes.

### 4. Tight state schema and reset provenance

Introduce a deliberately versioned state schema update if required rather than silently changing v9 semantics. Strongly type failure context, step records, reset provenance, alignment resolution, and goal review. Reject unknown fields where auditability matters.

A reset must retain the prior state hash, backup path, backup hash, correction class, reason, timestamp, and new binding. Validate the complete prior state before replacing it.

Separate operations clearly:

- `pause`: retain all progress without changing bindings;
- `reconcile`: accept a compatible binding correction;
- `rebind-evidence`: attach still-valid evidence after reconciliation;
- `supersede-source`: create a new run from a revised source and import compatible evidence;
- `restart`: intentionally discard progress after preserving a backup.

Acceptance criteria:

- Deleted or modified reset backups are detected.
- Top-level and per-step status cannot disagree.
- Terminal state requires a valid outcome review.
- Reset provenance cannot be removed or rewritten.
- Repair and rebind do not zero compatible completed work.
- A compatible catalog-only correction preserves completed steps, attempts, gate decisions, counters, and evidence through `reconcile` or `rebind` rather than forcing `reset`.
- Reset records and validates the prior-state hash and backup-content hash.

### 5. Source lineage and evidence reuse

Compute semantic digests for each step and each human-runner phase. A revised source should declare which prior source it supersedes. The engine should compare step and phase digests, workspace identity, relevant binding, output hashes, and dependency evidence.

Reuse is allowed only when all compatibility checks pass and the import is recorded. A behavior-changing revision must invalidate the changed step and its affected dependents.

Archive and baseline evidence should be content-addressed independently of presentation-only source changes.

Acceptance criteria:

- A verify-only amendment imports unchanged archive and baseline evidence.
- Unchanged human phases remain complete; only changed phases and dependent verification rerun.
- Old state remains immutable and linked from the new state.
- Imported evidence identifies its original source, state, step, phase, and hashes.
- Authoritative state evidence is stored under the durable invocation evidence directory; temporary runner paths are provenance only and are never the sole referenced evidence.

### 6. Typed human-runner contract

Replace opaque one-line shell commands with a readable, parser-addressable representation inside the canonical Markdown source. Preserve Markdown as the single authoritative source, but use fenced command blocks or another multiline grammar that can be reviewed without escaping layers.

Each phase should declare prerequisites, required tools, required environment, produced evidence, cleanup, expected outcome, phase dependencies, and rerun policy.

The materializer should write the runner and every phase receipt directly into a durable invocation-unique evidence directory. Temporary launchers may be disposable, but authoritative receipts must not be.

Acceptance criteria:

- Missing CWD, tool, environment, or prerequisite produces a structured failed receipt with expected and observed values.
- Valid phase names appear in usage output.
- Phase order is enforced or explicitly represented as a dependency graph.
- Rerunning a phase never overwrites prior receipts.
- A failed phase can resume from the earliest safe phase without rerunning unrelated successful phases.
- Cleanup verifies ownership before mutation and reports uncertain cleanup rather than suppressing it.
- The materializer emits one authoritative launcher command containing the exact runner path and hash, and the self-test invokes that command verbatim.
- Phase-produced environment values are carried by declared phase dependencies rather than being required as initial operator exports.

### 7. Runner validation and portability matrix

Extend validation beyond textual markers. Materialize every runner during planning preflight and run:

- JSON and grammar validation;
- `bash -n`;
- ShellCheck;
- safe `--self-test` or dry-run fixtures;
- phase graph validation;
- required-input/produced-output validation;
- macOS Bash and modern Linux Bash regression tests;
- zsh-safe operator-launcher tests.

Add deterministic checks for the transcript defects: empty arrays under nounset, preflight requiring remediation-installed tools, non-portable process discovery, unquoted redirection tokens, terminal-closing instructions, shell-precedence mistakes, and malformed command escaping.

Acceptance criteria:

- The original, v2, and uncorrected v3 runner defects are rejected before semantic approval.
- Runtime validation does not mutate host resources.
- Every generated runner supports one safe launch command that returns status without exiting the parent terminal.
- Missing prerequisites, missing tools, and missing environment values always produce a durable diagnostic receipt before returning failure.
- Rerun receipts and logs are unique and retain prior attempts.

### 8. Planning-admission satisfiability checks

Add deterministic checks before model review for resource feasibility, evidence-producer coverage, phase ordering, cleanup ownership, and verification reachability.

The planning packet must contain deterministic artifact identities. Either persist artifacts in a caller-owned evidence directory or include only stable relative identities and hashes; do not return deleted temporary paths.

Acceptance criteria:

- Provisioned resources are sufficient for required measured resources, including filesystem overhead.
- Every required evidence item has a preceding producer.
- A verification check cannot be satisfied by merely writing its own success label.
- Repeated planning preflight on unchanged inputs yields the same packet.
- Reviewer context contains readable commands and complete deterministic findings.
- Resource checks occur before mutations that cannot repair the resource condition, and provisioned capacity is proven sufficient for measured guest capacity including filesystem overhead.
- Every verification assertion has an independent evidence producer; a phase cannot satisfy its own verification merely by writing a success label.

### 9. Operator commands and recovery UX

Provide concise commands that hide state internals, for example:

```text
state status SOURCE
state explain SOURCE
state reconcile SOURCE --dry-run
state supersede OLD_SOURCE NEW_SOURCE --dry-run
human-gate diagnose SOURCE STEP
human-gate run SOURCE STEP --resume
```

Every blocked operation should report:

- what was preserved;
- what changed;
- whether the difference is relevant;
- what evidence remains reusable;
- the exact safe next command;
- what that command will modify.

Acceptance criteria:

- Human gates use one copy-safe command block with no surrounding prose that can be interpreted as shell input.
- The launcher is compatible with the user's interactive shell while invoking the required runner shell explicitly.
- A diagnostic never requires manually discovering hidden environment variables or inspecting raw state.
- Recovery commands use exact durable paths and tolerate absent files without unsafe wildcard expansion or shell-specific reserved variable names.
- Diagnostics distinguish missing, hidden, stale, temporary, and durable evidence paths.

### 10. M8 and cross-ledger binding integrity

Reject conflicting source, state, planning-ledger, or M8 metadata bindings rather than merging them with first-value-wins behavior. Preserve evidence-only M8 classification unless all required measurements and independent review exist.

Acceptance criteria:

- Conflicting ledger/state/metadata bindings fail with a field-level report.
- Missing telemetry remains explicit and non-blocking unless a plan requires it.
- No recovery path can manufacture an M8 completion or superiority claim.

## Recovery scenarios that must be demonstrated

The implementation is not complete until tests cover these recoveries:

1. Worker transport fails before action: retain dispatch evidence, consume no substantive attempt, allow bounded retry.
2. Worker result is lost after possible mutation: mark uncertain, inspect effects, and forbid blind retry.
3. Orchestrator stops after dispatch-start: resume from the open dispatch record.
4. Irrelevant configuration changes: record drift and continue without reset.
5. Relevant behavior-equivalent correction: reconcile with explicit provenance.
6. Relevant incompatible correction: pause and require a new approved execution contract.
7. Human phase succeeds but checkpoint fails: preserve durable receipts and ingest them after repair without rerunning the phase.
8. Runner crashes during cleanup: retain owned-resource records and resume cleanup safely.
9. Source amendment changes only verification: import prior compatible phases and rerun verification.
10. Evidence file is missing or altered: invalidate only dependent work and report the narrow rerun boundary.
11. State file is interrupted or corrupted: recover from the validated backup or transaction journal.
12. Model becomes unavailable: pause before dispatch and retain all completed work.
13. Control-plane code changes during a run: keep the run paused until an explicit engine-compatibility check succeeds.
14. User intentionally pauses: persist a first-class paused status with one resume action.
15. Canonical state or evidence binding cannot be reconciled: write a recovery-diagnostic envelope outside canonical state, preserve the original state, and provide a narrow repair or rebind action.
16. Catalog-only correction occurs after successful gates: reconcile the binding and preserve compatible archive, baseline, gate, attempt, and receipt records without rerunning them.
17. A temporary runner receipt exists but durable evidence is missing: import only after copying and hashing the durable evidence, or report the exact missing boundary without invalidating unrelated work.
18. The materialized launcher path is unavailable or mistyped: use the emitted path/hash command, diagnose the mismatch, and do not manually reconstruct a path.
19. A worker reports success while independent verification is unavailable: mark the step inconclusive or blocked and retain the worker result for later verification instead of retrying automatically.
20. A phase requires a value produced by a prior phase: carry it through typed phase dependency state rather than requiring a manual environment export.

## Regression suite

Create focused tests for:

- layered JSON/JSONC effective configuration;
- nested model keys and configured/runtime identity mismatch;
- irrelevant and relevant catalog drift;
- wrong executor, model, attempt, duplicate, and built-in dispatch receipts;
- typed failure context and terminal goal review;
- reset backup deletion, tampering, and malformed prior state;
- transactional event idempotency and crash recovery;
- human-runner diagnostic receipt creation;
- dry-run, phase ordering, usage, and rerun behavior;
- macOS and Linux shell behavior;
- durable and deterministic planning packets;
- compatible source supersession and phase-level evidence import;
- conflicting planning-ledger, state, and M8 bindings;
- a replay fixture derived from `execution-session-01_2026-09-04.md`;
- a replay fixture derived from `session-ses_f886.md`;
- recovery-diagnostic quarantine when canonical state or temporary evidence cannot be reconciled;
- compatible catalog correction preserving completed work;
- durable import of temporary runner receipts before checkpoint approval;
- exact materializer-emitted launcher-path execution;
- phase-produced environment dependency handling;
- zsh-safe failure and cleanup launchers;
- worker transport success versus independent verification failure;
- missing required verifier tool producing `inconclusive` rather than completion;
- effective per-image compiler-default verification.

## Execution approach after model change

Use ordinary skill-maintenance sessions, not the structured plan executor being repaired.

Primary role:

- `skill-author-high`, resolving to GPT-5.6 Luna at `high` effort.

Delegation:

- `explore-high` for difficult read-only investigation and independent evidence gathering.
- Use `coder-qwen` only for tightly bounded mechanical changes outside Kilo skill policy when explicitly permitted. Kilo skill design and integration remain with the Luna primary.
- Use a fresh Luna-high review-only session after each major work package.

Work in invocation-unique temporary directories, run targeted tests before applying final changes, preserve content-addressed backups, and obtain approval before changing canonical global skill files. Divide implementation into bounded sessions so no single Luna-high session exceeds its practical step budget.

Recommended session order:

1. Effective configuration and canonical identity.
2. Plan-relevant catalog and receipt relationships.
3. State schema, transactions, and reset integrity.
4. Source lineage and evidence reuse.
5. Human-runner contract and diagnostics.
6. Planning admission and durable packets.
7. Operator UX, M8 consistency, documentation, and full regression.
8. Independent review and canary recovery exercise.

## Success measures

The improved workflow should demonstrate:

- one stable initial admission for unchanged inputs;
- no hand-authored full state candidate during normal operation;
- no repeated archive or baseline work for a compatible amendment;
- changed human phases rerun without repeating unrelated phases;
- irrelevant catalog changes do not block progress;
- relevant changes block with a semantic diff and a recovery command;
- successful gate evidence survives checkpoint failure and temporary-directory loss;
- every preflight or checkpoint failure includes actionable diagnostics;
- reset and backup provenance are hash-verified;
- dispatch receipts bind executor, step, attempt, model, variant, and evidence;
- operator instructions use one safe launcher and never close the interactive terminal;
- transcript-derived incidents pass in regression tests;
- a paused execution resumes without redispatching successful work;
- all consistency checks improve repairability and recovery rather than creating avoidable dead ends.
