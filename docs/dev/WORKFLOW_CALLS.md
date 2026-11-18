# Reusable GitHub Actions Workflows

This document consolidates all information about reusable GitHub Actions workflows in the **phlex** repository. The workflows are defined under `.github/workflows/` and can be invoked via:

* `workflow_call` from another workflow (the primary reuse model)
* `workflow_dispatch` for manual runs via the **Actions** tab
* Patch-only formatter bot commands in PR comments (e.g. `@phlexbot clang-fix`)

The same workflows are also automatically triggered by CI events such as `pull_request` and `push` where appropriate.

## Table of Contents

* [General Usage Model](#general-usage-model)
* [Manual Runs (`workflow_dispatch`)](#manual-runs-workflow_dispatch)
* [Working on a Fork](#working-on-a-fork)
* [Relevance-Check Bypass (`skip-relevance-check`)](#relevance-check-bypass-skip-relevance-check)
* [Canonical Base-Input Contract](#canonical-base-input-contract)
* [Common Invocation Template](#common-invocation-template)
* [Check Workflows](#check-workflows)
* [Fix Workflows](#fix-workflows)
* [Fix Workflow Outputs](#fix-workflow-outputs)
* [Composite Workflow](#composite-workflow)
* [Non-Reusable Internal Workflows](#non-reusable-internal-workflows)
* [Bot Command Summary](#bot-command-summary)
* [Usage Guidelines](#usage-guidelines)

---

## General Usage Model

1. **Calling a reusable workflow** – In a consumer workflow file you reference a workflow on the phlex repo:

```yaml
jobs:
  my_job:
    uses: Framework-R-D/phlex/.github/workflows/<workflow_file>.yaml@<commit_sha>
    with:
      # workflow-specific inputs
```

   *Prefer pinning to a specific commit SHA* rather than `@main` to avoid unintentionally pulling breaking changes.

1. **Trigger types**
   * **Check workflows** – `pull_request`, `push`, `workflow_dispatch`, `workflow_call`. They only analyze code.
   * **Fix workflows** – `issue_comment` (gated to OWNER/COLLABORATOR/MEMBER), `workflow_dispatch`, `workflow_call`. They format an immutable source snapshot and publish patch-only artifacts; they never push.

2. **Source approval**
   Authorized PR-comment commands resolve the current PR head once at workflow start, then pin and record that immutable SHA. Formatter commands publish artifacts only and never push; review and apply them only if the PR still points to the recorded SHA. Build, coverage, and clang-tidy commands execute the pinned snapshot and skip or reject stale results. Manual dispatches continue to require an explicit full SHA.

---

## Manual Runs (`workflow_dispatch`)

Most workflows expose a `workflow_dispatch` trigger. To run one manually:

1. Open the **Actions** tab of the repository (or your fork).
2. Select the desired workflow on the left.
3. Choose the workflow branch to execute.
4. Fill any workflow-specific inputs (e.g. `build-combinations` for `cmake-build`) and provide `source-sha` where required.
5. Click **Run workflow**.

### Example (cmake-build)

```yaml
uses: ./.github/workflows/cmake-build.yaml
with:
  build-combinations: "gcc/asan clang/tsan"
  ref: main
  source-sha: <full-40-character-SHA-at-main-head>
```

---

## Working on a Fork

If you develop on a fork of `Framework-R-D/phlex`:

* **Enable Actions** on the fork (click the *I understand my workflows…* button on the **Actions** tab).
* Bot commands in the base repository use the base-repository bot name (e.g. `@phlexbot clang-fix`).
* Fix workflows need no write token: they resolve the PR head once and return a patch artifact, which must be reviewed and applied manually to that exact SHA.

### Authorization Model

Comment-triggered workflows perform an explicit authorization check, resolve the current PR head once at workflow start, and record that immutable snapshot. Only comment authors with one of the following association types are allowed to trigger them:

* `OWNER`
* `COLLABORATOR`
* `MEMBER`

This protects against malicious users posting bot commands. See the detailed analysis in `AUTHORIZATION_ANALYSIS.md` for the exact security model.

---

## Relevance-Check Bypass (`skip-relevance-check`)

When a reusable workflow is invoked from another workflow you may want it to run unconditionally (e.g. for a manual `workflow_dispatch`). Pass the following input:

```yaml
with:
  skip-relevance-check: ${{ github.event_name == 'workflow_dispatch' || github.event_name == 'issue_comment' }}
```

---

## Canonical Base-Input Contract

All check and fix workflows share these common inputs, provided by the `Framework-R-D/action-workflow-setup` action:

| Input | Type | Required | Description |
| ------- | ------ | ---------- | ------------- |
| `checkout-path` | string | No | Path to check out code to |
| `skip-relevance-check` | boolean | No | Bypass relevance detection (default `false`) |
| `ref` | string | No | Branch, ref, or SHA to checkout |
| `repo` | string | No | Repository to check out from |
| `pr-base-sha` | string | No | Base SHA of the PR for relevance check |
| `pr-head-sha` | string | No | Head SHA of the PR for relevance check |
| `source-sha` | string | Workflow-specific | Optional immutable source commit; otherwise resolve the event snapshot |

---

## Common Invocation Template

Below is a minimal reusable-workflow job snippet. Fix workflows derive the PR head from the caller event if `source-sha` is omitted. Adjust the `uses:` line and workflow-specific inputs as needed.

```yaml
jobs:
  <job_name>:
    uses: Framework-R-D/phlex/.github/workflows/<workflow_file>.yaml@<commit_sha>
    with:
      checkout-path: .
      # Optional: set skip-relevance-check to true for unconditional runs
      # skip-relevance-check: true
      # Optional source-sha can pin a previously resolved snapshot.
      # Add workflow-specific inputs here (e.g. build-combinations, language-matrix)
```

*Replace `<job_name>`, `<workflow_file>`, and `<commit_sha>` with the appropriate values for the workflow you are invoking.*

All check and fix workflows share these common inputs, provided by the `Framework-R-D/action-workflow-setup` action:

| Input | Type | Required | Description |
| ------- | ------ | ---------- | ------------- |
| `checkout-path` | string | No | Path to check out code to |
| `skip-relevance-check` | boolean | No | Bypass relevance detection (default `false`) |
| `ref` | string | No | Branch, ref, or SHA to checkout |
| `repo` | string | No | Repository to check out from |
| `pr-base-sha` | string | No | Base SHA of the PR for relevance check |
| `pr-head-sha` | string | No | Head SHA of the PR for relevance check |
| `source-sha` | string | Workflow-specific | Optional immutable source commit; otherwise resolve the event snapshot |

---

## Check Workflows

Check workflows analyze code without making changes. They are triggered by `pull_request`, `push`, `workflow_dispatch`, and `workflow_call`.

### Actionlint Check

*File:* `.github/workflows/actionlint-check.yaml`
*Inputs:* `checkout-path`, `skip-relevance-check`, `ref`, `repo`, `pr-base-sha`, `pr-head-sha`

### Clang-Format Check

*File:* `.github/workflows/clang-format-check.yaml`
*Inputs:* same as above.

### CMake Format Check

*File:* `.github/workflows/cmake-format-check.yaml`
*Inputs:* same as above.

### Header-Guards Check

*File:* `.github/workflows/header-guards-check.yaml`
*Inputs:* same as above.

### Jsonnet Format Check

*File:* `.github/workflows/jsonnet-format-check.yaml`
*Inputs:* same as above.

### Markdown Check

*File:* `.github/workflows/markdown-check.yaml`
*Inputs:* same as above.

### Python Check

*File:* `.github/workflows/python-check.yaml`
*Inputs:* same as above.

### YAML Check

*File:* `.github/workflows/yaml-check.yaml`
*Inputs:* same as above.

### Clang-Tidy Check

*File:* `.github/workflows/clang-tidy-check.yaml`
*Trigger:* `issue_comment`, `pull_request`, `push`, `workflow_dispatch` (no `workflow_call` inputs).

### CodeQL Analysis

*File:* `.github/workflows/codeql-analysis.yaml`
*Inputs:* includes `language-matrix`, `pr-number`, repository-related inputs, and relevance-check fields.

---

## Build Workflows

### CMake Build

*File:* `.github/workflows/cmake-build.yaml`
*Purpose:* Configures the project with CMake, builds it, and runs the test suite.
*Typical triggers:* `pull_request`, `push`, `workflow_dispatch`, `workflow_call`.
*Inputs:*

| Input | Type | Required | Default | Description |
| ------- | ------ | ---------- | --------- | ------------- |
| `checkout-path` | string | No | N/A | Path to check out code to |
| `build-path` | string | No | N/A | Path for build artifacts |
| `skip-relevance-check` | boolean | No | `false` | Bypass relevance detection |
| `build-combinations` | string | No | N/A | Space- or comma-separated list of `<compiler>/<sanitizer>` combos (e.g. `gcc/asan clang/tsan`). `all` runs every matrix entry; `+<combo>` adds extra combos. |
| `ref` | string | No | N/A | Target branch; the workflow checks out an immutable SHA |
| `repo` | string | No | N/A | Repository to checkout from |
| `pr-base-sha` | string | No | N/A | Base SHA for relevance check |
| `pr-head-sha` | string | No | N/A | Head SHA for relevance check |
| `source-sha` | string | Manual: Yes | N/A | Full SHA that must match the selected workflow branch head |

### Example Invocation (cmake-build)

```yaml
jobs:
  build_and_test:
    uses: Framework-R-D/phlex/.github/workflows/cmake-build.yaml@<commit_sha>
    with:
      build-combinations: "gcc/asan clang/tsan"
      ref: ${{ github.ref_name }}
      source-sha: ${{ github.sha }}
```

---

## Fix Workflows

Fix workflows resolve the PR head once, format that immutable SHA, publish patch artifacts, and never push changes.

### Clang-Format Fix

*File:* `.github/workflows/clang-format-fix.yaml`
*Required reusable-workflow inputs:* `ref`, `repo`
*Optional:* `checkout-path`, `skip-comment`, `source-sha` (required for non-PR callers)

### CMake Format Fix

*File:* `.github/workflows/cmake-format-fix.yaml`
*Required reusable-workflow inputs:* `ref`, `repo`
*Optional:* `checkout-path`, `skip-comment`, `source-sha` (required for non-PR callers)

### Header-Guards Fix

*File:* `.github/workflows/header-guards-fix.yaml`
*Required reusable-workflow inputs:* `ref`, `repo`
*Optional:* `checkout-path`, `skip-comment`, `source-sha` (required for non-PR callers)

### Jsonnet Format Fix

*File:* `.github/workflows/jsonnet-format-fix.yaml`
*Required reusable-workflow inputs:* `ref`, `repo`
*Optional:* `checkout-path`, `skip-comment`, `source-sha` (required for non-PR callers)

### Markdown Fix

*File:* `.github/workflows/markdown-fix.yaml`
*Required reusable-workflow inputs:* `ref`, `repo`
*Optional:* `checkout-path`, `skip-comment`, `source-sha` (required for non-PR callers)

### Python Fix

*File:* `.github/workflows/python-fix.yaml`
*Required reusable-workflow inputs:* `ref`, `repo`
*Optional:* `checkout-path`, `skip-comment`, `source-sha` (required for non-PR callers)

### YAML Fix

*File:* `.github/workflows/yaml-fix.yaml`
*Required reusable-workflow inputs:* `ref`, `repo`
*Optional:* `checkout-path`, `skip-comment`, `source-sha` (required for non-PR callers)

### Clang-Tidy Fix

*File:* `.github/workflows/clang-tidy-fix.yaml`
*Trigger:* `issue_comment` or `workflow_dispatch` (no explicit inputs via `workflow_call`).

---

## Fix Workflow Outputs

All `*-fix` workflows emit a standard set of outputs that can be used by downstream jobs:

| Output | Description |
| -------- | ------------- |
| `changes` | `true` if any files were modified by the formatter/fixer, otherwise `false` |
| `pushed` | Always `false`; formatter workflows never push |
| `commit_sha` | Always empty; no commit is created |
| `commit_sha_short` | Always empty |
| `patch_name` | Filename of the patch artifact when formatting changed files |

These outputs are available via `${{ steps.<step_id>.outputs.<output> }}` in a consuming workflow.

---

## Composite Workflow

### Format-All

*File:* `.github/workflows/format-all.yaml`
Calls all individual formatter fix workflows in parallel against one automatically resolved PR-head SHA and posts a combined patch-artifact comment. Triggered by `@phlexbot format`.

---

## Non-Reusable Internal Workflows

The repository also contains several workflows that are **not** intended for `workflow_call` reuse. They are used internally by the CI pipeline or for special purposes:

* `clang-tidy-check.yaml` – runs clang-tidy analysis; only triggered by `issue_comment`, `pull_request`, `push`, `workflow_dispatch`.
* `clang-tidy-fix.yaml` – applies clang-tidy fixes; triggered by `issue_comment` or `workflow_dispatch`.
* `clang-tidy-report.yaml` – posts clang-tidy results as PR comments; internal companion to the check workflow.
* `codeql-comment.yaml` – posts CodeQL findings as PR comments; used by `codeql-analysis.yaml`.
* `coverage.yaml` – generates code coverage reports; triggers via `issue_comment` and `workflow_dispatch`. It is **not** reusable via `workflow_call`.

  **Key Inputs**

  | Input | Type | Required | Default | Description |
  | ------- | ------ | ---------- | --------- | ------------- |
  | `ref` | string | No | repository default | Branch, ref, or SHA to checkout |
  | `repo` | string | No | current repo | Repository to checkout |
  | `pr-base-sha` | string | No | N/A | Base SHA for relevance check |
  | `phlex-coverage-compiler` | string | No | `clang` | Compiler for coverage build (`gcc` or `clang`) |
  | `phlex-enable-form` | string | No | `ON` | Enable FORM integration (`ON`/`OFF`) |

* `dependabot-auto-merge.yaml` – automates Dependabot PR merges; internal automation.
* `add-issues.yaml` – creates GitHub issues based on repository events; internal automation.

## Bot Command Summary

| Command | Description |
| --------- | ------------- |
| `@phlexbot clang-fix` | Run the Clang-Format fix workflow on the PR head branch. |
| `@phlexbot tidy-fix [checks]` | Generate Clang-Tidy fixes, optionally scoped to checks. |
| `@phlexbot cmake-fix` | Run the CMake-Format fix workflow. |
| `@phlexbot header-guards-fix` | Run the Header-Guards fix workflow. |
| `@phlexbot jsonnet-fix` | Run the Jsonnet-Format fix workflow. |
| `@phlexbot markdown-fix` | Run the Markdown-Fix workflow. |
| `@phlexbot python-fix` | Run the Python-Fix workflow. |
| `@phlexbot yaml-fix` | Run the YAML-Fix workflow. |
| `@phlexbot format` | Run **all** formatter fix workflows (calls `format-all.yaml`). |
| `@phlexbot tidy-check` | Run the Clang-Tidy **check** workflow. |
| `@phlexbot build [combinations]` | Run CMake build and tests for the current PR head. |
| `@phlexbot codeql-check` | Run the CodeQL analysis workflow. |
| `@phlexbot coverage` | Generate coverage reports. |

## Usage Guidelines

1. **Check workflows** – invoke via `workflow_call` from other workflows or rely on automatic CI triggers.
2. **Fix workflows** – invoke via `workflow_call` (set `skip-comment: true` when called from another workflow) or use bot commands in PR comments.
3. **Always provide `ref`, `repo`, and `checkout-path`** when calling a reusable workflow.
4. **Prefer pinning to a commit SHA** for stability; use `@main` only for experimental runs.
5. **When manually dispatching** (`workflow_dispatch`), you may set `skip-relevance-check: true` to run the workflow on all files.

---

*This document supersedes `.github/REUSABLE_WORKFLOWS.md`. The older file has been removed.*
