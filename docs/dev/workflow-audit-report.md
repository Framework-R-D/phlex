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

> **Model size note** – The analysis required only a few hundred lines of YAML parsing and pattern matching. The current model (120 B) provides sufficient context; a larger model would not materially improve the outcome.

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

**Impact:** Callers need to remember workflow-specific additions. Recommendation: move any extra input into a **dedicated “advanced” section** and document it in the shared README (`.github/WORKFLOW_CALLS.md`).

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
    contains(fromJSON('["OWNER","COLLABORATOR","MEMBER"]'), github.event.comment.author_association) &&
    startsWith(github.event.comment.body, format('@{0}bot <name>', github.event.repository.name))
  )
```

*All workflows use the same block (modulo the `<name>` part). No mismatches detected.*

---

## 3. Fork-PR Compatibility

### 3.1 Current pattern

* Workflows that need **write** permission on a PR (e.g., posting comments, uploading artifacts) **split** the operation into two separate workflows:
  * **Primary check** (runs with read-only `GITHUB_TOKEN`) – runs on the PR event.
  * **Secondary “-report” workflow** (`clang-tidy-report.yaml`, `coverage-report.yaml`, etc.) – triggered via `workflow_run` when the primary succeeds, and uses `secrets.WORKFLOW_PAT` (a PAT with `repo` scope) to write back.

### 3.2 Workflows that **lack** this split

| Workflow | Write-needed step | Current handling |
| ---------- | ------------------- | ------------------ |
| `markdown-fix.yaml` | Posts comment with `thollander/actions-comment-pull-request` | Uses `secrets.WORKFLOW_PAT` directly inside the same workflow (no split). **Fork-PR**: `GITHUB_TOKEN` is read-only, so the step will fail on forks. |
| `python-fix.yaml` | Commits formatted files back to PR | Uses `secrets.WORKFLOW_PAT` directly. No separate report workflow. |
| `clang-format-fix.yaml` | Same as above | Direct PAT usage – will fail on forks. |
| `jsonnet-format-fix.yaml` | Same pattern | Direct PAT usage. |
| `yaml-fix.yaml` | Same pattern | Direct PAT usage. |

**Conclusion:** The “split-workflow” strategy is **inconsistent**; only the clang-tidy and coverage pipelines use it. All other *-fix* workflows will break on PRs from forks.

### 3.3 Recommendations for fork handling

* Adopt the split-workflow pattern for **all** workflows that need PR write access.
* Create a generic **`post-comment.yaml`** workflow that can be invoked via `workflow_run` with a matrix of **`comment-type`** (e.g., `format-fix`, `markdown-fix`).
* Centralise the condition that detects a fork (`github.event.pull_request.head.repo.full_name != github.repository`) in a reusable step (`action-workflow-setup`) to expose `is_fork` output.

---

## 4. Permissions & `WORKFLOW_PAT` Usage

| Workflow | Permissions block | `WORKFLOW_PAT` usage | Minimality assessment |
| ---------- | ------------------- | ---------------------- | ----------------------- |
| **Check-only** (clang-tidy-check, clang-format-check, yaml-check, markdown-check, python-check, codeql-analysis) | `contents: read`, `pull-requests: read` (plus `security-events: write` for codeql) | **None** | ✅ Minimal |
| **Fix / Report** (clang-tidy-fix, clang-tidy-report, coverage, markdown-fix, python-fix, clang-format-fix, jsonnet-format-fix, yaml-fix) | `contents: write` (often `issues`/`pull-requests: write`) | **Sees `secrets.WORKFLOW_PAT`** | ⚠️ Over-perm in many cases |
| **CI for actions** (actions/* CI workflows) | `contents: read`, `pull-requests: read`, `security-events: write` for codeql | Never uses PAT | ✅ Good |
| **Dependabot auto-merge** | `contents: write`, `pull-requests: write` | Uses `secrets.WORKFLOW_PAT` for merging | Acceptable (needs push). |

### 4.1 Over-use of `WORKFLOW_PAT`

* **`markdown-fix.yaml`**, **`python-fix.yaml`**, **`clang-format-fix.yaml`**, **`jsonnet-format-fix.yaml`**, **`yaml-fix.yaml`** all request a full PAT **even though they only need `contents: write` and `pull-requests: write`** on the *forked* PR. The standard `GITHUB_TOKEN` (with `permissions: {contents: write, pull-requests: write}`) would be sufficient **if we adopt the split-workflow model**.
* **`codeql-comment.yaml`** uses `WORKFLOW_PAT` to download artifacts across runs (required because `GITHUB_TOKEN` cannot access artifacts from other private repositories). This is **justified**.

### 4.2 Minimal-permission recommendation

* Create a **repo-level secret** `WRITE_TOKEN` that is a scoped PAT with only `contents` & `pull-requests` scopes (no `admin:repo`).
* Switch all *-fix* workflows to use `WRITE_TOKEN` **only** in the secondary report workflow.
* For workflows that genuinely need `security-events` (codeql) keep the current permission set but remove PAT usage – `GITHUB_TOKEN` already has `security-events: write` when `permissions` includes it.

---

## 5. Action-Repo Workflow Parity

### 5.1 Current state

* Each `action/*` repository contains **CI workflows** (`ci.yaml`, `dependabot-auto-merge.yaml`, `guardrail-audit-alert.yaml`) that **lint**, **test**, and **security-scan** the action itself.
* **No copy of the *phlex* operational workflows** (e.g., `cmake-build.yaml`, `clang-tidy-check.yaml`) exists in the action repos – they are **not intended to run there**.

### 5.2 Desired “identical copy” policy

The user’s note #3 requests that workflows in the action repos **should be identical copies** of the equivalents in the primary repo, with differences handled elsewhere (e.g., path changes). In practice:

| Gap | Example | Why it matters |
| ----- | --------- | ---------------- |
| Missing `workflow_call` inputs for actions that are *reusable* (e.g., `action-workflow-setup`) | `action-workflow-setup` has its own `action.yaml` but no dedicated workflow files. | Consumers expect the same interface across all repos. |
| Permission drift | Some action CI workflows request `contents: write` (unnecessary for static analysis). | Over-privilege increases risk. |
| Duplicate `guardrail-audit-alert.yaml` variation | Slightly different description text, version caps. | Inconsistent documentation for auditors. |

### 5.3 Recommended approach (tiered)

1. **Source-of-truth folder** – Keep a `ci/` directory **inside `phlex/.github/`** that contains the *canonical* CI workflows for actions (e.g., `action-ci.yaml`).
2. **Copy script** – A lightweight script (`scripts/sync-action-ci.sh`) runs in CI after any change to `phlex/.github/ci/*` and:
   * Copies each file to the corresponding `action/*/.github/workflows/` directory.
   * Substitutes placeholders (`{{REPO}}`) with the target repo name where needed.
3. **Version-gate** – Add a `workflow_dispatch` to the sync script itself so maintainers can manually trigger a sync if required.

*Result*: No manual edit of each action repo; the `phlex` repo remains the **single source of truth** for CI policy.

---

## Prioritized Recommendations

| Priority | Area | Recommendation | Rationale |
| ---------- | ------ | ---------------- | ----------- |
| **P1** | **Fork-PR handling** | Refactor **all** `*-fix.yaml` and any workflow that writes to the PR to use the **split-workflow (report) pattern**. Create a generic `post-comment.yaml` reusable workflow for all comment-posting steps. | Guarantees success on forks, eliminates broken runs, aligns with the proven clang-tidy pattern. |
| **P1** | **`workflow_call` input consistency** | Define a **shared YAML fragment** (e.g., `.github/workflows/inputs-common.yaml`) and `<<: *common-inputs` in each workflow. Include all base inputs (`checkout-path`, `build-path`, `skip-relevance-check`, `ref`, `repo`, `pr-base-sha`, `pr-head-sha`). | Reduces duplication, prevents drift, ensures new reusable workflows start with the same contract. |
| **P2** | **Permissions minimisation** | Replace all direct `secrets.WORKFLOW_PAT` usage in fix/report workflows with a **scoped `WRITE_TOKEN`** (contents & pull-requests only). Require PAT only in `codeql-comment.yaml` where artifact download across repos is needed. | Least-privilege principle; reduces blast-radius if token leaks. |
| **P2** | **Variable naming alignment** | Standardise all boolean inputs to **snake_case** (e.g., `skip-relevance-check`) and avoid lower-case env vars (`perfetto-heap-profile`). Use the same naming scheme for `workflow_dispatch` inputs. | Improves readability and reduces hidden bugs caused by case-sensitivity. |
| **P3** | **Action-repo CI parity** | Implement the **tiered sync script** (see 5.3). Keep a central `ci/` folder in the main repo and automatically propagate to each action repo via a scheduled CI job. | Guarantees identical CI policies across all reusable actions, eliminates manual effort. |
| **P3** | **Documentation** | Add a top-level `WORKFLOW_CALLS.md` that lists all reusable workflow names, required inputs, and example invocations. Include a note on the split-workflow pattern for forks. | Onboards new contributors quickly, prevents misuse. |
| **P4** | **`workflow_dispatch` UX** | Ensure every `ref` input has a helpful description and a default (e.g., “empty → default branch”). Add a **`description`** field for any extra inputs (`build-combinations`, `perfetto-…`). | Improves self-service usage from the UI; less confusion for ad-hoc runs. |
| **P4** | **Test coverage of triggers** | Add a matrix test job (run in CI) that **simulates each trigger** (`push`, `pull_request`, `workflow_dispatch`, `schedule`, `workflow_call`, `issue_comment`). Verify that jobs execute as expected. | Detects future regression when changing the shared `if:` gating block. |

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
| `clang-tidy-fix` | write | write | — | — | ✅ (direct) |
| `markdown-fix` | write | write | — | — | ✅ (direct) |
| `codeql-analysis` | read | read | — | write | — |
| `codeql-comment` | read | read | write | — | ✅ (artifact download) |
| Action CI (`ci.yaml`) | read | read | — | write (codeql) | — |

### A.3 Fork-PR Detection Pattern (current)

```yaml
if: >
  github.event_name == 'workflow_dispatch' ||
  github.event_name == 'pull_request' ||
  github.event_name == 'push' ||
  (github.event_name == 'issue_comment' &&
   github.event.issue.pull_request &&
   contains(fromJSON('["OWNER","COLLABORATOR","MEMBER"]'), github.event.comment.author_association) &&
   startsWith(github.event.comment.body, format('@{0}bot <name>', github.event.repository.name)))
```

*All check/fix workflows use this block; only the `*-report` workflows add an extra condition `github.event_name != 'pull_request'` to avoid double-posting.*

---

End of Report
