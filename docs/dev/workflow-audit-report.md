# Phlex & Action Repositories – Workflow & Reusable-Action Audit

*Prepared for the `phlex` monorepo (primary) and the `action/*` sibling repos.*

---

## Table of Contents

1. [Scope & Methodology](#scope--methodology)
2. [Structural Consistency & Naming](#1-structural-consistency--naming)
3. [Trigger Correctness (`workflow_dispatch` / `workflow_call`)](#2-trigger-correctness)
4. [Fork-PR Compatibility](#3-fork-pr-compatibility)
5. [Permissions & `WORKFLOW_PAT` Usage](#4-permissions--workflow_pat-usage)
6. [Action-Repo Workflow Parity](#5-action-repo-workflow-parity)
7. [Prioritized Recommendations](#prioritized-recommendations)
8. [Appendix – Quick Reference Tables](#appendix--quick-reference-tables)

---

## Scope & Methodology

| Item | What was examined | Tools used |
| ------ | ------------------- | ----------- |
| **Phlex workflows** | All 27 YAML files under `phlex/.github/workflows/` | `grep`, `read`, `bash` to list files |
| **Action reusable-actions** | `action/*/action.yaml` + CI workflows under `action/*/.github/workflows/` | Same tooling |
| **Cross-repo parity** | Comparison of each phlex workflow with the copy-step used for the action repos. | Manual diff of key sections |
| **Triggers, permissions, PATs** | Specific patterns (`workflow_dispatch`, `workflow_call`, `permissions:`, `secrets.WORKFLOW_PAT`). | `grep` across the repo tree |
| **Fork handling** | Workflows that post PR comments or artifacts (clang-tidy, coverage, format-fix, etc.). | Reviewed explicit `if:` conditions and auxiliary “-report” workflows |

> **Model size note** – The structural pattern-matching in this report (naming, input inventory, `if:`-gating comparison) required only a few hundred lines of YAML parsing. However, an earlier revision of this report drew incorrect conclusions about trigger semantics, the `GITHUB_TOKEN` CI-recursion barrier, and cross-file YAML-anchor reuse in GitHub Actions — precisely the Actions-specific reasoning that pattern matching does not capture. Sections 3, 4, and the Prioritized Recommendations have been corrected accordingly. Reviewers should treat Actions-runtime semantics claims as requiring verification against the actual trigger blocks, not inferred from surface structure.

---

## 1. Structural Consistency & Naming

### 1.1 Common “base” inputs (present in most `workflow_call` sections)

| Input | Description | Present in |
| ------- | ------------- | ------------ |
| `checkout-path` | Path to checkout the repo | **All** workflow-call enabled files |
| `skip-relevance-check` | Bypass relevance detection | **All** |
| `pr-base-sha` & `pr-head-sha` | Relevance-check SHAs | **All** |
| `ref` | Branch / SHA to check out | **All** |
| `repo` | Repository to checkout | **All** |

#### Missing / Inconsistent Inputs

| Workflow | Missing input(s) | Impact |
| ---------- | ------------------ | -------- |
| `clang-format-check.yaml`, `clang-format-fix.yaml`, `cmake-format-check.yaml`, `cmake-format-fix.yaml`, `jsonnet-format-*.yaml` | `build-path` (present only in `cmake-build.yaml` and a few others) | Consumers that need a custom build directory cannot reuse these without fork-editing. |
| `python-check.yaml`, `python-fix.yaml` | `build-combinations` (only in `cmake-build.yaml`) | Not applicable – fine; but naming differs (`build-combinations` vs. none). |
| `coverage.yaml` | No `skip-relevance-check` (uses its own gating) | Slightly divergent semantics; may cause confusion in docs. |

### 1.2 Job & Stage Naming

| Semantic group | Standard name in phlex | Outliers |
| ---------------- | ------------------------ | ---------- |
| Setup | `setup` | None |
| Build matrix generation | `generate-matrix` | `generate-matrix` used only in `cmake-build.yaml`; other checks don’t need a matrix. |
| Build / test | `build` | `clang-tidy-check` & `clang-tidy-fix` use `clang-tidy-check` (clear) – fine. |
| Reporting / comment | `build-complete` / `report` | `clang-tidy-report.yaml` uses a separate workflow (`report`) – consistent. |
| Linting | `yaml-check`, `markdown-check`, `clang-format-check`, `clang-tidy-check` | Consistent. |
| Format-fix | `yaml-fix`, `markdown-fix`, `clang-format-fix`, `clang-tidy-fix` | Consistent. |
| Coverage | `coverage` (single job) | No divergence. |

**Variable Naming** – Across all workflows, environment variables are capitalised (e.g., `BUILD_TYPE`, `CICOLOR_FORCE`). The only exception is the `perfetto` flags inside `cmake-build.yaml` that expose lower-case env vars (`perfetto-heap-profile`, `perfetto-cpu-profile`) **only** for `workflow_dispatch`; they are referenced via `${{ github.event.inputs.perfetto-heap-profile }}` – acceptable but broken consistency with the other boolean inputs (`skip-relevance-check`).

---

## 2. Trigger Correctness

### 2.1 `workflow_dispatch`

| Workflow | Inputs defined | Default values | Comments |
| ---------- | ---------------- | ---------------- | ---------- |
| All check/fix workflows (`clang-*`, `cmake-*`, `jsonnet-*`, `markdown-*`, `python-*`, `yaml-*`, `coverage`) | `ref` (string) – **no default** | none | Good – allows explicit ref override. |
| `cmake-build.yaml` | **Additional** inputs `build-combinations`, `perfetto-heap-profile`, `perfetto-cpu-profile` | `build-combinations`: `""` (empty) → defaults to `gcc/none` in script; `perfetto-*`: boolean defaults (false/true) | Consistent with the underlying build script. |
| `coverage.yaml` | Several extra inputs (`phlex-coverage-compiler`, `phlex-enable-form`, etc.) | Mostly empty strings or booleans with defaults | Correct – matched by later `if` statements. |

**Issue:** Some `workflow_dispatch` blocks omit a `description` for the `ref` input (e.g., `jsonnet-format-fix.yaml` – description present, but `markdown-fix.yaml` does not). While not functional, it reduces self-documentation.

### 2.2 `workflow_call`

All 13 workflows that expose `workflow_call` share the same core set of inputs (see 1.1). The only divergences are:

| Workflow | Extra input(s) |
| ---------- | ---------------- |
| `cmake-build.yaml` | `build-combinations` |
| `clang-tidy-fix.yaml` | `tidy-checks` (boolean array) – present only here. |
| `python-fix.yaml` | No extra inputs. |

**Impact:** Callers need to remember workflow-specific additions. Recommendation: document each workflow's inputs in a shared reference (`.github/WORKFLOW_CALLS.md`).

> **Note on de-duplication.** GitHub Actions does **not** support YAML anchors/aliases across files, and does not merge a `<<:`-injected map into `on.workflow_call.inputs`. There is no supported "import a shared inputs fragment" mechanism at the workflow level. De-duplicating input definitions therefore requires either (a) accepting per-workflow duplication (documented in one place), or (b) a generator/templating step that emits the workflow files from a single source. A raw `<<: *common_inputs` in a workflow's `inputs:` block will not work and must not be recommended.

### 2.3 Consistency of `if:` gating

The majority of jobs share an identical pattern:

```yaml
if: >
  github.event_name == 'workflow_dispatch' ||
  github.event_name == 'pull_request' ||
  github.event_name == 'push' ||
  github.event_name == 'schedule' ||
  github.event_name == 'workflow_call' ||
  (
    github.event_name == 'issue_comment' &&
    github.event.issue.pull_request &&
    contains(fromJSON('["OWNER", "COLLABORATOR", "MEMBER"]'), github.event.comment.author_association) &&
    startsWith(github.event.comment.body, format('@{0}bot <name>', github.event.repository.name))
  )
```

*All workflows use the same block (modulo the `<name>` part). No mismatches detected.*

---

## 3. Fork-PR Compatibility

> **Correction (supersedes the earlier draft).** The earlier draft claimed the
> `*-fix` workflows run on `pull_request` and therefore "break on forks." That
> is false. It also conflated two distinct write operations — *posting a
> comment* and *pushing a commit* — into one problem with one fix. Those are
> different problems with different solutions, and only one of them is solvable
> by an extra workflow invocation. This section is rewritten to reflect the
> workflows as actually wired.

### 3.1 How the `*-fix` workflows are actually triggered

Every `*-fix.yaml` workflow (`clang-format-fix`, `cmake-format-fix`,
`header-guards-fix`, `jsonnet-format-fix`, `markdown-fix`, `python-fix`,
`yaml-fix`) is triggered **only** by:

* `issue_comment` (a `@phlexbot <tool>-fix` command on a PR), gated to
  `OWNER`/`COLLABORATOR`/`MEMBER` author-association;
* `workflow_dispatch` (a maintainer manually running it); and
* `workflow_call` (invoked by `format-all.yaml`, which is itself
  `issue_comment`-triggered under the same author-association gate).

**None of them trigger on `pull_request`.** Consequently they never execute in a
fork's PR-event context, and the "read-only fork `GITHUB_TOKEN`" scenario the
earlier draft described **does not arise**. Every invocation runs in the
**base-repo context**, initiated by a trusted maintainer. There is no
fork-triggered code path to break.

### 3.2 The two write operations, disentangled

The `*-fix` workflows perform two different writes; the audit must treat them
separately because they have different credential requirements.

| Operation | Where it happens | Credential requirement |
| ----------- | ------------------ | ------------------------ |
| **Post a PR comment** (status/patch instructions) | `action-handle-fix-commit`, `thollander/actions-comment-pull-request`, `action-complete-pr-comment` | Only needs `pull-requests: write`. In a base-repo context (or a `workflow_run` context) the plain `GITHUB_TOKEN` suffices — see `clang-tidy-report.yaml`, which comments with `github.token`, no PAT. |
| **Push the fix commit back to the PR branch** | `action-handle-fix-commit` → `git push origin HEAD:$PR_REF` | Needs a token that (a) can write to the target branch — possibly a **fork** branch when "Allow edits from maintainers" is enabled — and (b) **re-triggers downstream CI**. The default `GITHUB_TOKEN` cannot satisfy (b): commits pushed with `GITHUB_TOKEN` do **not** trigger further workflow runs (GitHub suppresses this to prevent recursion). This is the real, unavoidable reason `WORKFLOW_PAT` is present. |

The fork case for the *commit* path is already handled gracefully:
`action-handle-fix-commit` pushes directly only when the PR is same-repo **or**
`maintainer_can_modify` is true; otherwise it uploads a `fix.patch` artifact and
comments patch-application instructions (see the action's `Create patch` /
`Comment with patch instructions` steps). It does **not** "fail on forks" — it
degrades cleanly.

### 3.3 What an "extra workflow invocation" can and cannot solve

The repo already uses the extra-invocation (`workflow_run`) pattern for
*comment* writes: `clang-tidy-report.yaml` and `codeql-comment.yaml`.
Instructively:

* `clang-tidy-report.yaml` posts its comment with `github-token: ${{ github.token }}` — the plain `GITHUB_TOKEN` — because a `workflow_run` workflow runs in the base repo with `pull-requests: write`. **No PAT needed to comment.**
* `codeql-comment.yaml` uses `WORKFLOW_PAT` **only** to *download artifacts from another run in a private/internal repo* (see its inline comment on the `github-token` input); its `createComment`/`updateComment` calls could use `GITHUB_TOKEN`. The PAT there is an artifact-access requirement, not a commenting requirement.

Therefore:

* **Commenting → solvable by an extra invocation.** The comment-only steps of
  the `*-fix` workflows *could* be moved to a `workflow_run`-triggered reporter
  that comments with `GITHUB_TOKEN`, eliminating the PAT **for those steps**.
  The benefit is modest, because these workflows already run in a base-repo
  context where `GITHUB_TOKEN` also has `pull-requests: write`.
* **Committing → NOT solvable by an extra invocation.** A `workflow_run`
  reporter changes *where* a write happens, not *what credential* a
  CI-re-triggering `git push` requires. Pushing a fix commit that re-runs the
  checks still needs a PAT or a GitHub App installation token. No amount of
  extra workflow plumbing removes this requirement.

### 3.4 Recommendations for fork handling

* **Do not** refactor the `*-fix` workflows into a split "report" pattern on the
  premise that they break on forks — they do not, and the split does not address
  the operation (commit push) that actually needs the secret.
* **Optionally**, if minimizing PAT exposure in comment-only steps is desired,
  move those specific comment steps behind the existing `workflow_run` pattern
  and have them comment with `GITHUB_TOKEN`. Treat this as low-priority cleanup,
  not a correctness fix.
* If the goal is to also remove the PAT from the **commit** path (e.g., to avoid
  a long-lived user PAT entirely), the correct mechanism is a **GitHub App
  installation token**, which both writes to the branch and re-triggers CI. A
  second scoped user PAT (`WRITE_TOKEN`) does **not** achieve this — see §4.

---

## 4. Permissions & `WORKFLOW_PAT` Usage

| Workflow | Permissions block | `WORKFLOW_PAT` usage | Minimality assessment |
| ---------- | ------------------- | ---------------------- | ----------------------- |
| **Check-only** (clang-tidy-check, clang-format-check, yaml-check, markdown-check, python-check, codeql-analysis) | `contents: read`, `pull-requests: read` (plus `security-events: write` for codeql) | **None** | ✅ Minimal |
| **Fix / Report** (clang-tidy-fix, clang-tidy-report, coverage, markdown-fix, python-fix, clang-format-fix, jsonnet-format-fix, yaml-fix) | `contents: write` (often `issues`/`pull-requests: write`) | **Sees `secrets.WORKFLOW_PAT`** | ✅ Justified — the PAT is required to push CI-re-triggering fix commits (§4.1), not an over-permission artifact. Fine-grained-token provisioning (§4.2) is the appropriate minimization, if any. |
| **CI for actions** (actions/* CI workflows) | `contents: read`, `pull-requests: read`, `security-events: write` for codeql | Never uses PAT | ✅ Good |
| **Dependabot auto-merge** | `contents: write`, `pull-requests: write` | Uses `secrets.WORKFLOW_PAT` for merging | Acceptable (needs push). |

### 4.1 Why `WORKFLOW_PAT` is required (corrected)

The earlier draft asserted that `GITHUB_TOKEN` "would be sufficient if we adopt
the split-workflow model." That is incorrect for the operation that actually
drives the PAT requirement.

* **The dominant reason is the commit push, not commenting.** The `*-fix`
  workflows push fix commits back to the PR branch via `action-handle-fix-commit`
  (`git push origin HEAD:$PR_REF`). Commits pushed with the default
  `GITHUB_TOKEN` **do not trigger downstream workflow runs** — GitHub suppresses
  this by design to prevent infinite CI recursion. Fix commits *must* re-trigger
  the checks, so a credential other than `GITHUB_TOKEN` is required. A
  `workflow_run` "report" split does not change this: the report workflow can
  *comment* with `GITHUB_TOKEN`, but it cannot *push a re-triggering commit* with
  it.
* **A secondary reason is cross-repo push to fork branches.** When a fork PR has
  "Allow edits from maintainers" enabled, pushing to the contributor's branch
  needs a token the base `GITHUB_TOKEN` does not carry. When that permission is
  absent, the action already falls back to a patch artifact + instructions
  (§3.2), so this is not a failure mode.
* **`codeql-comment.yaml`** uses `WORKFLOW_PAT` to download artifacts across
  runs (required because `GITHUB_TOKEN` cannot access artifacts from other
  private/internal repositories). This is **justified**. Its comment-posting
  calls, by contrast, could use `GITHUB_TOKEN`.

### 4.2 On a scoped `WRITE_TOKEN` secret (corrected)

The earlier draft recommended introducing a second repo secret, `WRITE_TOKEN`,
scoped to `contents` + `pull-requests`. This recommendation is **withdrawn** for
the following reasons:

* **A classic PAT cannot be narrowed to "contents + pull-requests only."** The
  minimum classic-PAT scope that permits `git push` to a repo is `repo`, which
  already encompasses contents and pull requests. There is no narrower classic
  scope to gain.
* **If finer scoping is genuinely wanted, provision the *existing*
  `WORKFLOW_PAT` as a fine-grained token** (repository-scoped, with
  Contents: write and Pull requests: write permissions). This achieves the
  minimization goal **without adding a second secret name**.
* **A second secret increases surface and onboarding friction.** Forks that want
  `@phlexbot` auto-fixes must set `WORKFLOW_PAT` today; adding `WRITE_TOKEN`
  would force them to set *two* secrets for no privilege gain.
* **To eliminate a long-lived user PAT entirely**, migrate to a **GitHub App
  installation token** — it can both push (re-triggering CI) and comment, and is
  short-lived and centrally revocable. This is the only option that also removes
  the PAT from the *commit* path.

For workflows that genuinely need `security-events` (codeql), keep the current
permission set and rely on `GITHUB_TOKEN` — no PAT is needed there.

---

## 5. Action-Repo Workflow Parity

### 5.1 Current state

* Each `action/*` repository contains **CI workflows** (`ci.yaml`, `dependabot-auto-merge.yaml`, `guardrail-audit-alert.yaml`) that **lint**, **test**, and **security-scan** the action itself.
* **No copy of the *phlex* operational workflows** (e.g., `cmake-build.yaml`, `clang-tidy-check.yaml`) exists in the action repos – they are **not intended to run there**.

### 5.2 Policy decision: reject "identical copies" of operational workflows

> **Decision.** The "identical copies" framing is **rejected**. It is
> incompatible with §5.1 (the operational phlex workflows are intentionally
> absent from the action repos) and with how the two repo classes are actually
> used. The adopted policy is: *shared logic lives in reusable actions/workflows;
> each repository keeps only the workflow files it actually needs.*

The two repo classes have genuinely different jobs:

* The **phlex** operational workflows (`cmake-build`, `clang-tidy-check`, the
  `*-fix` family) build and check the phlex C++/Python codebase.
* The **action** repos' CI (`ci.yaml`: actionlint, YAML/Markdown lint, CodeQL
  `actions`) validates the *action definitions*. There is no C++ to build there;
  copying `cmake-build.yaml` into, say, `action-handle-fix-commit` would be dead
  weight.

Consequently, "identical copies of operational workflows" is not a defect to
close — the absence is correct by design.

The DRY mechanism GitHub actually provides is `workflow_call` / composite
actions, **which the repo already uses**. Consistency is already centralized
where it matters: `action-workflow-setup`, `action-handle-fix-commit`,
`action-collect-format-results`, etc. are single sources of truth consumed by
every workflow via SHA-pinned `uses:`. That is the supported, low-maintenance
form of "identical," and it is already in place.

The one legitimate residual concern is **CI-policy drift** across the action
repos' near-identical `ci.yaml` files (permission blocks, version caps,
`guardrail-audit-alert.yaml` wording). This is real but small, and a file-copy
sync engine is a disproportionate — and failure-prone — response to it
(placeholder-substitution bugs, drift when a downstream copy is edited directly,
SHA-pin skew across N repos).

### 5.3 If CI-policy consistency becomes a felt pain

Address it with the supported reuse mechanism, not a copy script:

1. **Preferred — reusable CI workflow.** Extract the shared action-repo CI into a
   single reusable workflow (e.g., `action-ci.yaml` in `Framework-R-D/.github` or
   a dedicated repo) that each action repo *calls*:
   `uses: Framework-R-D/<repo>/.github/workflows/action-ci.yaml@<sha>` with a
   couple of inputs. One source of truth; no duplicated files to drift.
2. **Fallback — drift detection, not propagation.** A scheduled job that diffs
   each action repo's `ci.yaml` against a canonical copy and **opens an issue** on
   mismatch. Detect and flag; do not auto-overwrite downstream files.

The file-sync script previously proposed here (`scripts/sync-action-ci.sh` with
`{{REPO}}` substitution) is **not recommended** and should not be built.

---

## Prioritized Recommendations

> **Superseded items.** Three recommendations from the earlier draft are
> **withdrawn**: (1) the P1 "refactor all `*-fix` workflows to the split pattern
> to fix fork breakage" — the `*-fix` workflows are not `pull_request`-triggered
> and do not break on forks (§3.1); (2) the P2 "introduce a scoped `WRITE_TOKEN`
> secret" — `WORKFLOW_PAT` is required for CI-re-triggering commit pushes and
> cannot be replaced by a split workflow or a narrower classic PAT (§3.3, §4); and
> (3) the P3 action-repo **file-sync script** — the "identical copies" policy is
> rejected (§5.2). The revised table below reflects the corrected findings.

| Priority | Area | Recommendation | Rationale |
| ---------- | ------ | ---------------- | ----------- |
| **P2** | **Variable naming alignment** | Standardise all boolean inputs to **snake_case** (e.g., `skip-relevance-check`) and avoid lower-case env vars (`perfetto-heap-profile`). Use the same naming scheme for `workflow_dispatch` inputs. | Improves readability and reduces hidden bugs caused by case-sensitivity. Low-risk, self-contained. |
| **P2** | **Documentation** | Add a `WORKFLOW_CALLS.md` that lists all reusable workflow names, required inputs, and example invocations. Document the *actual* trigger model of the `*-fix` workflows (`issue_comment` / `workflow_dispatch` / `workflow_call`, base-repo context only) and the reason `WORKFLOW_PAT` is required (CI-re-triggering commit push). | Onboards contributors; prevents the same misdiagnosis recurring. |
| **P3** | **`workflow_dispatch` UX** | Ensure every `ref` input has a helpful description and a documented default (e.g., "empty → default branch"). Add a **`description`** field for extra inputs (`build-combinations`, `perfetto-…`). | Improves self-service usage from the UI; less confusion for ad-hoc runs. Independent, low-risk. |
| **P3** | **`workflow_call` input consistency** | Document the canonical base-input contract (`checkout-path`, `build-path`, `skip-relevance-check`, `ref`, `repo`, `pr-base-sha`, `pr-head-sha`) in `WORKFLOW_CALLS.md`. If de-duplication is pursued, use a **generator/templating** step — **not** a cross-file YAML anchor, which Actions does not support (§2.2). | Reduces drift without relying on an unsupported mechanism. |
| **P4** *(optional)* | **PAT minimisation** | If reducing token exposure is desired: (a) provision the existing `WORKFLOW_PAT` as a **fine-grained** repo-scoped token (Contents + Pull requests: write); and/or (b) migrate the commit-push path to a **GitHub App installation token**. Optionally move comment-only steps behind the existing `workflow_run` reporter so they comment with `GITHUB_TOKEN`. | Achieves least-privilege without a second secret; the GitHub App is the only route that also removes the PAT from the commit path. |
| **Closed** | **Action-repo CI parity** | Policy decision made (§5.2): "identical copies" of operational workflows is **rejected**; shared logic already lives in reusable actions consumed via SHA-pinned `uses:`. The file-sync script is **not** to be built. *If* CI-policy drift across action repos becomes a felt pain, use a **reusable `action-ci.yaml` workflow** (call, don't copy) or a **drift-detection** job that opens an issue on mismatch (§5.3). | Uses the supported reuse mechanism; avoids a copy engine's drift and SHA-skew failure modes. |
| **P4** | **Trigger regression guard** | Instead of a matrix job that "simulates" every trigger (`issue_comment`, `workflow_run`, `schedule` cannot be faithfully simulated from within a job), add a lightweight **lint/assertion** that checks the shared `if:` gating block is present and identical across workflows (e.g., a script in the existing `actionlint`/CI job). | Detects drift in the gating block without a mock that validates nothing real. |

---

## Appendix – Quick Reference Tables

### A.1 Workflow Input Summary (core)

| Workflow | `checkout-path` | `build-path` | `skip-relevance-check` | `ref` | `repo` | `pr-base-sha` | `pr-head-sha` |
| ---------- | ----------------- | -------------- | -------- | ------- | -------- | ---------------- | --------------- |
| `cmake-build.yaml` | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| `clang-tidy-check.yaml` | ✅ | ✅ (via setup) | ✅ | ✅ | ✅ | ✅ | ✅ |
| `clang-format-check.yaml` | ✅ | ✅ (via setup) | ✅ | ✅ | ✅ | ✅ | ✅ |
| `python-check.yaml` | ✅ | — | ✅ | ✅ | ✅ | ✅ | ✅ |
| `markdown-check.yaml` | ✅ | — | ✅ | ✅ | ✅ | ✅ | ✅ |
| `coverage.yaml` | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| `yaml-check.yaml` | ✅ | — | ✅ | ✅ | ✅ | ✅ | ✅ |

(“—” indicates the workflow never uses a build directory.)

### A.2 Permission Matrix

| Workflow | `contents` | `pull-requests` | `issues` | `security-events` | `secrets.WORKFLOW_PAT`? |
| ---------- | ------------ | ---------------- | ---------- | ------------------- | -------------------------- |
| `clang-tidy-check` | read | read | — | — | ✅ (only in `report` workflow) |
| `clang-tidy-fix` | write | write | — | — | ✅ (direct — required to push CI-re-triggering commits; §4.1) |
| `markdown-fix` | write | write | — | — | ✅ (direct — required to push CI-re-triggering commits; §4.1) |
| `codeql-analysis` | read | read | — | write | — |
| `codeql-comment` | read | read | write | — | ✅ (artifact download) |
| Action CI (`ci.yaml`) | read | read | — | write (codeql) | — |

### A.3 Author-Association Gating Pattern

The **check** workflows use the broad event list shown in §2.3. The **`*-fix`**
workflows use a *narrower* gate that does **not** include `pull_request` — this
is why they never run in a fork's PR-event context (§3.1):

```yaml
if: >
  inputs.ref != '' ||
  github.event_name == 'workflow_dispatch' ||
  (github.event_name == 'issue_comment' &&
   github.event.issue.pull_request &&
   contains(fromJSON('["OWNER","COLLABORATOR","MEMBER"]'), github.event.comment.author_association) &&
   startsWith(github.event.comment.body, format('@{0}bot <name>-fix', github.event.repository.name)))
```

*The `inputs.ref != ''` clause is the `workflow_call` entry point (used by
`format-all.yaml`). The `*-report` workflows are `workflow_run`-triggered and
gate on `github.event.workflow_run.event == 'pull_request'` instead.*

---

End of Report
