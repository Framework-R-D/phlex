# Plan: CI, Release Infrastructure, and Agent Instructions for `action-*` Repositories

## Overview

Add production-quality CI, release infrastructure, agent instructions, and pre-commit
configuration to all 15 `Framework-R-D/action-*` repositories. The deliverables per
repository are:

| File | All 15 repos | `generate-build-matrix` only |
| ---- | :----------: | :---------------------------: |
| `.github/release.yaml` | ✓ | ✓ |
| `.github/workflows/ci.yaml` | ✓ | ✓ |
| `.github/workflows/python-check.yaml` | — | ✓ |
| `.github/actionlint.yaml` | ✓ | ✓ |
| `.markdownlint.jsonc` | ✓ | ✓ |
| `.yamllint.yaml` | ✓ | ✓ |
| `RELEASES.md` | ✓ | ✓ |
| `AGENTS.md` | ✓ | ✓ |
| `.pre-commit-config.yaml` | ✓ | ✓ |
| `pyproject.toml` | — | ✓ |
| `tests/test_generate_matrix.py` | — | ✓ |

The existing `actionlint.yaml` workflow (already on each repo at
`.github/workflows/actionlint.yaml`) is **replaced** by the new `ci.yaml`; the old
file is deleted. The existing `dependabot.yml` is left unchanged.

---

## How to execute this plan

Phases must run in order. Each Phase section is self-contained. Within Phase 2 the
per-repository work is parallel-safe; the Gate serializes before Phase 3.

| Phase | Action |
| ----- | ------ |
| 0 | Environment and credential check |
| 1 | Produce the `add-ci.sh` deployment script (first deliverable) |
| 2 | Run the script against all 15 repositories; then run the Phase 2 Gate |
| 3 | Hand-author `tests/test_generate_matrix.py` for `action-generate-build-matrix` |

---

## Background: GitHub Releases, `release.yaml`, and Dependabot PR descriptions

**What a GitHub Release is.**
A GitHub Release is a named snapshot tied to a git tag. It has a title, a Markdown
body ("release notes"), and is either draft/pre-release/published. It is created via
the GitHub UI → *Releases → Draft a new release*, or via `gh release create`. Published,
non-pre-release releases are what dependabot surfaces in its PR descriptions.

**What `.github/release.yaml` is.**
This is GitHub's *automatically-generated release notes* configuration
(not a workflow). When present it controls how GitHub auto-populates the release body
when you click "Generate release notes" in the UI or pass `--generate-notes` to
`gh release create`. It maps PR labels to changelog categories and can exclude
specific labels (e.g. dependabot bumps). It does **not** run on a schedule; it is
purely a template consumed at release-creation time
([documentation](https://docs.github.com/en/repositories/releasing-projects-on-github/automatically-generated-release-notes))

**How dependabot populates its PR description.**

- *Release notes* block: sourced from the GitHub Release whose `tag_name` matches the
  new version. Dependabot fetches the release body verbatim and renders it.
- *Changelog* block: sourced from `CHANGELOG.md` at the tip of the **default branch**
  (`main`). Dependabot extracts sections whose heading contains the version token(s)
  in the update range (e.g. `## v2` when bumping from v1 → v2). The `## v1 --- DATE`
  heading format already in these repos is compatible; dependabot parses the leading
  version token.

**What this means for release procedure.**
When publishing a new version (say `v2`):

1. Commit changes + prepend a `## v2 --- YYYY-MM-DD` section to `CHANGELOG.md` on `main`.
2. Create annotated tag `v2` at that commit.
3. Create a GitHub Release for `v2` using `gh release create v2 --generate-notes`
   (which uses `release.yaml` to auto-populate categories) then edit/augment the body.
4. Dependabot consumer PRs will then show both the release body and the `## v2` changelog
   section.

---

## Dependency Ordering and Constraints

The CI workflows in the action repos use `Framework-R-D/action-*` dependencies
(self-bootstrapping pattern, consistent with phlex). Specifically, the `ci.yaml`
workflow uses:

- `action-workflow-setup` — ref resolution + change detection (one `file-type` per call)
- `action-run-change-detection` — a second change-detection call for a different
  `file-type` (markdown), since `workflow-setup` only carries one `file-type`

No circular dependency risk exists because these are SHA-pinned references; the CI
workflow in repo `action-X` pins `action-workflow-setup@<sha>`, which is a
different, already-released repo. Dependabot keeps those pins current.

> **Important interface facts (verified against the live `action.yaml` files; the
> executing agent must re-verify before writing the workflow):**
>
> - `action-workflow-setup` outputs exactly: `is_act`, `ref`, `repo`, `base_sha`,
>   `pr_number`, `checkout_path`, `build_path`, `has_changes` (a **single**
>   `has_changes`, derived from its one `file-type`/`include-globs` argument). It
>   does **not** emit per-file-type outputs such as `has_changes_yaml`.
> - `action-workflow-setup` does **not** check out the repository working tree. Each
>   lint job must run its own `actions/checkout`.
> - `action-run-change-detection` requires `checkout-path`, `ref`, `repo`, and
>   `base-ref` (all four are `required: true`), plus a `file-type` (or
>   `include-globs`). Its `has_changes` output is the boolean match result. It
>   performs its own internal sparse checkout for diffing.
> - `raven-actions/actionlint` only scans `.github/workflows/*.y{a,}ml` by default;
>   it does **not** lint the root `action.yaml` (which is a composite action
>   definition, not a workflow). Coverage of `action.yaml` correctness comes from the
>   CodeQL `actions` analysis, which does inspect composite action definitions. State
>   this overlap honestly in `AGENTS.md`; do not claim actionlint validates
>   `action.yaml`.

The SHAs for the self-referenced action dependencies and the external actions must be
resolved at script-write time. Do **not** hard-code SHAs from this document; they may
have been superseded by dependabot updates. Resolve each one with:

```bash
resolve_tag_sha() {
  local owner_repo="$1" tag="$2" ref_sha ref_type
  ref_sha=$(gh api "repos/${owner_repo}/git/ref/tags/${tag}" --jq .object.sha)
  ref_type=$(gh api "repos/${owner_repo}/git/ref/tags/${tag}" --jq .object.type)
  if [ "${ref_type}" = "tag" ]; then
    gh api "repos/${owner_repo}/git/tags/${ref_sha}" --jq .object.sha
  else
    echo "${ref_sha}"
  fi
}
resolve_tag_sha Framework-R-D/action-workflow-setup v1
resolve_tag_sha Framework-R-D/action-run-change-detection v1
resolve_tag_sha actions/checkout v4
resolve_tag_sha raven-actions/actionlint v2
resolve_tag_sha astral-sh/setup-uv v6
resolve_tag_sha DavidAnson/markdownlint-cli2-action v20
resolve_tag_sha github/codeql-action v3
```

> **Tag-major caveats:** the major-version tag you resolve must actually exist.
> `markdownlint-cli2-action` and `codeql-action` major versions move over time; if a
> `resolve_tag_sha` call returns a 404, list available tags with
> `gh api repos/OWNER/REPO/tags --jq '.[].name' | head` and pick the highest stable
> major. Record the chosen `# vN` comment to match the resolved SHA.

**Why `actions/checkout v4` not v6?** The action repos are simple and any current
stable major is acceptable. Use whichever major you resolve successfully and annotate
the pin comment accordingly (`# v4` or `# v6`). Consistency within a single repo is
what matters.

---

## Phase 0 — Environment Check

1. Verify `gh` authentication with `repo` scope and push credentials:

   ```bash
   gh auth status
   gh auth setup-git
   git ls-remote https://github.com/Framework-R-D/action-detect-act-env.git HEAD >/dev/null \
     || { echo 'ERROR: push credentials not working'; exit 1; }
   ```

2. Confirm signing is disabled or the key is available (same check as the original
   export plan Phase 0 step 3):

   ```bash
   SIGKEY=$(git config --global user.signingkey 2>/dev/null || true)
   if git config --global --bool commit.gpgsign 2>/dev/null | grep -q true; then
     if [ -z "${SIGKEY}" ] || [ ! -f "${SIGKEY/#\~/$HOME}" ]; then
       git config --global commit.gpgsign false
     fi
   fi
   ```

---

## Phase 1 — Produce `add-ci.sh` *(first deliverable)*

Write the file `docs/dev/add-action-ci.sh` to the `maintenance/export-reusable-actions`
branch of phlex. This script, when run for a given action repo, adds all the files in
the table above. The script is idempotent (skips files that already exist with the
correct content).

### Script contract

```text
Usage: add-ci.sh <action-name>
  action-name: bare name, e.g. "detect-act-env" (not "action-detect-act-env")
```

The script clones or reuses a working directory in `${TMPDIR:-/tmp}/action-work/`,
commits the new files to `main`, and pushes. It does **not** create or move any tags
or releases.

### Files the script writes

All files below are described with their full content. The script writes them using
heredocs. Content that must vary per repo is parameterized on `ACTION_NAME` and
`REPO_NAME` (`action-${ACTION_NAME}`).

#### `.github/release.yaml`

GitHub's autogenerated release notes configuration. Categories map to PR labels.
Actions repos use a minimal label set (dependabot + manual). Content is **identical**
across all 15 repos:

```yaml
changelog:
  categories:
    - title: "Breaking Changes"
      labels:
        - breaking-change
    - title: "New Features"
      labels:
        - enhancement
    - title: "Bug Fixes"
      labels:
        - bug
    - title: "Documentation"
      labels:
        - documentation
    - title: "Maintenance"
      labels:
        - dependencies
        - github_actions
  exclude:
    labels:
      - skip-changelog
```

#### `.github/actionlint.yaml`

Minimal actionlint config for standalone action repos (no self-hosted runners,
no config-variables scanning needed):

```yaml
self-hosted-runner:
  labels: []
config-variables: null
```

#### `.markdownlint.jsonc`

Identical to phlex's config at `/workspaces/phlex/.markdownlint.jsonc`. Copy verbatim.

#### `.yamllint.yaml`

Identical to phlex's `.yamllint.yaml`. Copy verbatim.

#### `.pre-commit-config.yaml`

Minimal pre-commit configuration appropriate for action repos (no C++, no CMake,
no Jsonnet, no custom scripts — except for `generate-build-matrix`):

```yaml
repos:
  - repo: https://github.com/pre-commit/pre-commit-hooks
    rev: v5.0.0
    hooks:
      - id: trailing-whitespace
      - id: end-of-file-fixer
      - id: check-yaml
      - id: check-toml
      - id: check-merge-conflict
      - id: check-executables-have-shebangs
      - id: mixed-line-ending
  - repo: https://github.com/rbubley/mirrors-prettier
    rev: v3.5.3
    hooks:
      - id: prettier
        types_or: [yaml]
  - repo: https://github.com/DavidAnson/markdownlint-cli2
    rev: v0.17.2
    hooks:
      - id: markdownlint-cli2
        args: [--fix]
```

For `generate-build-matrix` only, additionally add:

```yaml
  - repo: https://github.com/astral-sh/ruff-pre-commit
    rev: v0.9.0
    hooks:
      - id: ruff
        types_or: [python, pyi]
      - id: ruff-format
        types_or: [python, pyi]
```

**NOTE:** Verify the `rev` values for all pre-commit hooks against the relevant repo’s releases page (for example, [ORG/REPO releases](https://github.com/ORG/REPO/releases)) before writing the script. Do not use
version strings from this document without checking.

#### `AGENTS.md`

Agent instructions for the repository. Use a common template parameterized on
`ACTION_NAME` and the action's `description` field (read from `action.yaml`). The
structure is:

```markdown
# Agent Instructions — `action-NAME`

## Repository Purpose

<one-sentence description from action.yaml `description:` field>

This repository contains a single composite GitHub Action previously bundled
within [Framework-R-D/phlex](https://github.com/Framework-R-D/phlex) and
extracted for standalone reuse.

## Action Interface

**Inputs:**  (table: Name | Description | Required | Default)
**Outputs:** (table: Name | Description)

## Repository Structure

| Path | Purpose |
| ---- | ------- |
| `action.yaml` | The composite action definition |
| `CHANGELOG.md` | Per-version changelog (dependabot reads this) |
| `RELEASES.md` | Release procedure and checklist |
| `README.md` | Usage documentation |
| `LICENSE` | Apache 2.0 |
| `.github/release.yaml` | Autogenerated release notes config |
| `.github/workflows/ci.yaml` | CI: actionlint, YAML, Markdown, CodeQL |
| `.github/dependabot.yml` | Weekly dependency updates |

(For `generate-build-matrix` also: `generate_matrix.py`, `tests/`, `pyproject.toml`,
`.github/workflows/python-check.yaml`)

## Development Workflow

- All changes via pull request to `main`.
- CI runs on every PR: actionlint, YAML lint, Markdown lint, CodeQL (`actions`
  language). For `generate-build-matrix`: additionally ruff, mypy, and pytest.
- Use `pre-commit run` before pushing to catch formatting issues
  locally.
- Dependabot opens weekly PRs for action pin updates. They are **not** auto-merged
  (no auto-merge workflow is configured); review and merge them manually, or
  configure branch protection + an auto-merge workflow as a follow-on.

## Coding Standards

- `action.yaml`: all `uses:` references must be SHA-pinned with a `# vN` comment.
  Version-tag pins (e.g. `@v4`) are not acceptable.
- YAML: formatted with prettier (120-char soft limit, 2-space indent).
- Markdown: must pass markdownlint with the rules in `.markdownlint.jsonc`.
- Python (generate-build-matrix only): ruff + mypy clean; Google-style docstrings;
  type hints required; tests in `tests/` with pytest.

## Release Procedure

See `RELEASES.md` for the complete step-by-step checklist.

## Phlex Ecosystem Context

This action is part of the `Framework-R-D` action ecosystem supporting the
[Phlex framework](https://github.com/Framework-R-D/phlex). When releasing a new
version, check whether dependent actions (those that `uses:` this action in their
own `action.yaml`) also need updating. The dependency graph is:

  Level 0 (no internal deps): detect-act-env, detect-relevant-changes, get-pr-info,
    setup-build-env, configure-cmake, build-cmake, collect-format-results,
    complete-pr-comment, generate-build-matrix, post-clang-tidy-results, handle-fix-commit
  Level 1 (depend on Level 0): prepare-check-outputs, prepare-fix-outputs,
    run-change-detection
  Level 2 (depends on Level 1): workflow-setup
```

The `inputs/outputs` tables in the `AGENTS.md` are populated by the script by
parsing `action.yaml` with a small Python one-liner (same technique used in Phase 1
of the original export plan).

#### `RELEASES.md`

Release procedure document. Content is **identical** across all repos (only the
action name in the title differs). Full content (the outer four-backtick fence is a
plan-document presentation device only; the file itself starts at `# Release
Procedure`):

````markdown
# Release Procedure for `action-NAME`

This document is the authoritative checklist for publishing a new version of this
action. Follow every step in order. The release is not complete until all steps are
checked off.

## Background: how releases work

### Tags and versions

This repository uses plain integer major-version tags: `v1`, `v2`, `v3`, ...
There are no patch or minor releases; every published change is a new major version.

### What dependabot reads

When dependabot opens a PR in a consumer repository (e.g. phlex) to update a pin
from `@SHA # v1` to `@SHA # v2`, its PR description contains:

- **Release notes**: the body of the GitHub Release whose `tag_name` is `v2`.
  Written at release-creation time (step 6 below).
- **Changelog**: the `## v2` section extracted from `CHANGELOG.md` at the tip of
  `main`. Written in step 2 below.

Both sources must be present and accurate for the dependabot PR to be useful to
reviewers.

## Pre-release checklist

Before tagging, verify:

- [ ] All CI checks on `main` are green.
- [ ] `action.yaml` reflects the intended interface (all inputs/outputs documented).
- [ ] All `uses:` references in `action.yaml` are SHA-pinned.
- [ ] `README.md` usage example is up to date (inputs shown, SHA will be updated
      post-tag in step 8).

## Release checklist

### Step 1 — Determine the new version number

Increment the current latest version by 1. Example: if `v2` is the latest tag,
the new version is `v3`.

```bash
NEXT_VERSION=v3     # replace with actual next version
TODAY=$(date +%Y-%m-%d)
```

### Step 2 — Add a changelog section to `CHANGELOG.md` on `main`

Prepend a new section immediately after the `# Changelog` heading:

```markdown
## vN --- YYYY-MM-DD

### What's Changed

- <bullet describing each user-visible change; link PRs as (#N)>

### Breaking Changes

- <list breaking changes, or omit this subsection if none>
```

Commit and push directly to `main` (or via a PR if branch protection is enabled):

```bash
git add CHANGELOG.md
git commit -m "docs: changelog for ${NEXT_VERSION}"
git push origin main
```

### Step 3 — Verify `CHANGELOG.md` heading format

Dependabot's changelog extractor requires the version token to appear in the `## ` heading.
Run this sanity check:

```bash
grep "^## ${NEXT_VERSION}" CHANGELOG.md \
  || { echo "ERROR: ${NEXT_VERSION} section missing from CHANGELOG.md"; exit 1; }
```

### Step 4 — Create an annotated tag

```bash
git fetch origin main
git checkout main
git pull --ff-only origin main
git tag "${NEXT_VERSION}" -m "${NEXT_VERSION} - <one-line summary>"
```

Do **not** use a lightweight tag; annotated tags carry authorship and message
metadata and produce cleaner release output.

### Step 5 — Push the tag

```bash
git push origin "${NEXT_VERSION}"
```

### Step 6 — Create the GitHub Release

Use GitHub's autogenerated notes as a starting point:

```bash
gh release create "${NEXT_VERSION}" \
  --title "${NEXT_VERSION} - <one-line summary>" \
  --generate-notes \
  --repo Framework-R-D/action-NAME
```

`--generate-notes` uses `.github/release.yaml` to categorize merged PRs
since the previous tag. Review and augment the generated body:

- Ensure the "Breaking Changes" section is accurate (or absent if none).
- Add a one-paragraph summary at the top explaining *why* this release exists.
- Verify the "Full Changelog" comparison link at the bottom is correct.

Edit the release body with:

```bash
gh release edit "${NEXT_VERSION}" --notes-file /tmp/release-notes.md \
  --repo Framework-R-D/action-NAME
```

### Step 7 — Verify the release

```bash
gh release view "${NEXT_VERSION}" --repo Framework-R-D/action-NAME
```

Confirm:
- [ ] `draft: false`
- [ ] `prerelease: false`
- [ ] Release body contains the "What's Changed" list and no `<PR>` placeholders.
- [ ] Tag name matches `${NEXT_VERSION}`.

### Step 8 — Update `README.md` usage example

The README usage example should be pinned to the new version's commit SHA:

```bash
NEW_SHA=$(gh api repos/Framework-R-D/action-NAME/git/ref/tags/${NEXT_VERSION} \
  --jq .object.sha)
# If the tag is annotated, dereference:
TYPE=$(gh api repos/Framework-R-D/action-NAME/git/ref/tags/${NEXT_VERSION} \
  --jq .object.type)
if [ "$TYPE" = "tag" ]; then
  NEW_SHA=$(gh api repos/Framework-R-D/action-NAME/git/tags/${NEW_SHA} \
    --jq .object.sha)
fi

# Update README.md
OLD_PATTERN="Framework-R-D/action-NAME@[0-9a-f]\{40\} # v[0-9]*"
NEW_STRING="Framework-R-D/action-NAME@${NEW_SHA} # ${NEXT_VERSION}"
sed -i "s|${OLD_PATTERN}|${NEW_STRING}|" README.md
git add README.md
git commit -m "docs: pin README usage example to ${NEXT_VERSION} SHA"
git push origin main
```

### Step 9 — Update dependent action repositories (Level 1 and Level 2 only)

If this action is a **dependency** of other actions (check the dependency graph in
`AGENTS.md`), file issues on those downstream action repos requesting a pin update,
or open PRs directly. Dependabot will also pick this up on its weekly schedule.

### Step 10 — Update `phlex` (or other consumers)

Open a PR on `Framework-R-D/phlex` updating the SHA pin for this action in all
affected workflow files, if the update cannot wait for dependabot's weekly cycle.

## Security notes

- Never push a tag that points to an untrusted or unreviewed commit.
- The tag commit must be on `main` (or a reviewed, merged commit).
- Do not use `--force` when pushing tags; if a tag must be moved, delete it and
  re-create rather than force-pushing, and re-create the GitHub Release.
- Pin all `uses:` references in `action.yaml` to commit SHAs. Version-tag pins
  (`@v4`) are supply-chain risks for composite actions.
````

#### `.github/workflows/ci.yaml` (all repos except `generate-build-matrix`)

This workflow replaces the existing `actionlint.yaml`. It runs on `pull_request`,
`push` to `main`, and `workflow_dispatch`. It has five jobs:

1. **`setup`** — calls `action-workflow-setup` once with `file-type: yaml` (this
   yields `has_changes` for YAML/action files), and calls `action-run-change-detection`
   once more with `file-type: md` (a second `has_changes` for Markdown). The `setup`
   job re-exports both as distinct outputs (`has_changes_yaml`, `has_changes_md`) and
   also re-exports `ref`, `repo`, `base_sha`, and `checkout_path`, which the lint jobs
   need for their own checkouts.
2. **`actionlint`** — `raven-actions/actionlint`; gated on YAML changes.
3. **`yaml-check`** — `yamllint`; gated on YAML changes.
4. **`markdown-check`** — `markdownlint-cli2-action`; gated on Markdown changes.
5. **`codeql`** — CodeQL `actions` language; gated on YAML changes (composite
   `action.yaml` is YAML).

**Gating semantics.** On `pull_request`, a job runs only if its relevant
`has_changes_*` output is `true`. On `push` to `main` and `workflow_dispatch`, every
job runs unconditionally (the `github.event_name != 'pull_request'` arm of each `if`).
Jobs that are gated out are *skipped*, which GitHub reports as neutral (not failed);
do not treat skipped as a CI failure. The `setup` job has no relevance gate and must
succeed for the dependent jobs to evaluate.

**Why two change-detection calls.** `action-workflow-setup` carries only one
`file-type`; it cannot produce both a YAML and a Markdown signal in a single
invocation. The `setup` job therefore runs `workflow-setup` (yaml) and then a separate
`run-change-detection` (md). The `run-change-detection` call needs `checkout-path`,
`ref`, `repo`, and `base-ref`, all sourced from the `workflow-setup` outputs.

**Important:** `action-workflow-setup` does not check out the working tree. Each lint
job performs its own `actions/checkout` into `checkout_path`, and every tool runs with
`working-directory:` set to that path. CodeQL uses `source-root` / `checkout_path`
accordingly.

Permissions block at workflow level:

```yaml
permissions:
  contents: read
  pull-requests: read
  security-events: write  # required for codeql-action/analyze to upload SARIF
```

**CodeQL `actions` language notes (verified against `github/codeql-action`):**

- Input on `init` is `source-root`; input on `analyze` is `checkout_path`. Both exist.
- The `actions` language requires `build-mode: none` (composite actions are not
  compiled). Pass `build-mode: none` on `init`.
- No `codeql-config.yml` is needed; use the default `security-extended` query suite via
  the `queries:` input on `init`.
- SARIF is uploaded by `analyze` automatically (`upload` defaults to `always`); no
  separate upload step is required.
- The CodeQL `actions` language is available in `github/codeql-action` v3 and later;
  confirm the resolved major supports it (`gh api repos/github/codeql-action/tags`).

**Full workflow skeleton** (the `*_SHA` placeholders are replaced with resolved SHAs
at script-write time; the `# vN` comments must match the resolved major):

```yaml
name: CI

on:
  push:
    branches: [main]
  pull_request:
    branches: [main]
  workflow_dispatch:

permissions:
  contents: read
  pull-requests: read
  security-events: write

jobs:
  setup:
    runs-on: ubuntu-latest
    outputs:
      ref: ${{ steps.setup.outputs.ref }}
      repo: ${{ steps.setup.outputs.repo }}
      base_sha: ${{ steps.setup.outputs.base_sha }}
      checkout_path: ${{ steps.setup.outputs.checkout_path }}
      has_changes_yaml: ${{ steps.setup.outputs.has_changes }}
      has_changes_md: ${{ steps.detect_md.outputs.has_changes }}
    steps:
      - name: Workflow setup (YAML relevance)
        id: setup
        uses: Framework-R-D/action-workflow-setup@WORKFLOW_SETUP_SHA # v1
        with:
          file-type: yaml

      - name: Detect Markdown changes
        id: detect_md
        # Only meaningful for pull_request events, where base_sha is populated and
        # a diff base exists. For push / workflow_dispatch the markdown-check job
        # runs unconditionally (its `github.event_name != 'pull_request'` arm), so
        # this detection step is skipped and its empty output is harmless.
        if: github.event_name == 'pull_request' && steps.setup.outputs.is_act != 'true'
        uses: Framework-R-D/action-run-change-detection@RUN_CHANGE_DETECTION_SHA # v1
        with:
          checkout-path: ${{ steps.setup.outputs.checkout_path }}
          ref: ${{ steps.setup.outputs.ref }}
          repo: ${{ steps.setup.outputs.repo }}
          base-ref: ${{ steps.setup.outputs.base_sha }}
          file-type: md

  actionlint:
    needs: setup
    if: >
      always() && needs.setup.result == 'success' && (
        github.event_name != 'pull_request' ||
        needs.setup.outputs.has_changes_yaml == 'true'
      )
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@CHECKOUT_SHA # v4
        with:
          ref: ${{ needs.setup.outputs.ref }}
          path: ${{ needs.setup.outputs.checkout_path }}
          repository: ${{ needs.setup.outputs.repo }}
          persist-credentials: false
      - uses: raven-actions/actionlint@ACTIONLINT_SHA # v2
        with:
          working-directory: ${{ needs.setup.outputs.checkout_path }}

  yaml-check:
    needs: setup
    if: >
      always() && needs.setup.result == 'success' && (
        github.event_name != 'pull_request' ||
        needs.setup.outputs.has_changes_yaml == 'true'
      )
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@CHECKOUT_SHA # v4
        with:
          ref: ${{ needs.setup.outputs.ref }}
          path: ${{ needs.setup.outputs.checkout_path }}
          repository: ${{ needs.setup.outputs.repo }}
          persist-credentials: false
      - name: Install uv
        uses: astral-sh/setup-uv@SETUP_UV_SHA # v6
      - name: Install yamllint
        run: uv tool install yamllint
      - name: Run yamllint
        working-directory: ${{ needs.setup.outputs.checkout_path }}
        run: yamllint .

  markdown-check:
    needs: setup
    if: >
      always() && needs.setup.result == 'success' && (
        github.event_name != 'pull_request' ||
        needs.setup.outputs.has_changes_md == 'true'
      )
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@CHECKOUT_SHA # v4
        with:
          ref: ${{ needs.setup.outputs.ref }}
          path: ${{ needs.setup.outputs.checkout_path }}
          repository: ${{ needs.setup.outputs.repo }}
          persist-credentials: false
      - uses: DavidAnson/markdownlint-cli2-action@MARKDOWNLINT_SHA # vNN
        with:
          # globs are newline-delimited; the leading '!' excludes CHANGELOG.md
          # (its `## vN --- DATE` headings are not standard Markdown). Globs are
          # relative to the working directory, so cd into the checkout first via
          # the action's default cwd (GITHUB_WORKSPACE) + explicit path prefix.
          globs: |
            ${{ needs.setup.outputs.checkout_path }}/**/*.md
            !${{ needs.setup.outputs.checkout_path }}/**/CHANGELOG.md

  codeql:
    needs: setup
    if: >
      always() && needs.setup.result == 'success' && (
        github.event_name != 'pull_request' ||
        needs.setup.outputs.has_changes_yaml == 'true'
      )
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@CHECKOUT_SHA # v4
        with:
          ref: ${{ needs.setup.outputs.ref }}
          path: ${{ needs.setup.outputs.checkout_path }}
          repository: ${{ needs.setup.outputs.repo }}
          persist-credentials: false
      - uses: github/codeql-action/init@CODEQL_SHA # v3
        with:
          languages: actions
          build-mode: none
          queries: security-extended
          source-root: ${{ needs.setup.outputs.checkout_path }}
      - uses: github/codeql-action/analyze@CODEQL_SHA # v3
        with:
          checkout_path: ${{ needs.setup.outputs.checkout_path }}
```

> **Verification note for the executing agent:** the `markdownlint-cli2-action`
> `globs` exclusion-with-path-prefix behavior is fragile. Before relying on it, test
> the chosen pattern against a sample tree (a `CHANGELOG.md` plus a `README.md`) in a
> scratch PR. If the negation does not take effect, the robust fallback is to add a
> repo-level `.markdownlint-cli2.yaml` (or `.markdownlintignore`) listing
> `CHANGELOG.md` under `ignores:` and drop the inline `globs` exclusion. Prefer the
> config-file approach if there is any doubt; record the decision in the commit.

#### `.github/workflows/python-check.yaml` (`generate-build-matrix` only)

A separate workflow file (mirrors phlex's own `python-check.yaml`), independently
triggerable via `workflow_dispatch`. Triggers on `pull_request`, `push` to `main`,
and `workflow_dispatch`. Contains three jobs: a `setup` job (own to this workflow) plus
`python-check` and `python-test`. The `setup` job uses `action-workflow-setup` with
`file-type: python` so its single `has_changes` output gates the Python jobs:

```yaml
name: Python Check

on:
  push:
    branches: [main]
  pull_request:
    branches: [main]
  workflow_dispatch:

permissions:
  contents: read
  pull-requests: read

jobs:
  setup:
    runs-on: ubuntu-latest
    outputs:
      ref: ${{ steps.setup.outputs.ref }}
      repo: ${{ steps.setup.outputs.repo }}
      checkout_path: ${{ steps.setup.outputs.checkout_path }}
      has_changes: ${{ steps.setup.outputs.has_changes }}
    steps:
      - name: Workflow setup (Python relevance)
        id: setup
        uses: Framework-R-D/action-workflow-setup@WORKFLOW_SETUP_SHA # v1
        with:
          file-type: python

  python-check:
    needs: setup
    if: >
      always() && needs.setup.result == 'success' && (
        github.event_name != 'pull_request' ||
        needs.setup.outputs.has_changes == 'true'
      )
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@CHECKOUT_SHA # v4
        with:
          ref: ${{ needs.setup.outputs.ref }}
          path: ${{ needs.setup.outputs.checkout_path }}
          repository: ${{ needs.setup.outputs.repo }}
          persist-credentials: false
      - uses: astral-sh/setup-uv@SETUP_UV_SHA # v6
      - run: uv tool install ruff && uv tool install mypy
      - name: Run ruff check
        working-directory: ${{ needs.setup.outputs.checkout_path }}
        run: ruff check
      - name: Run ruff format check
        working-directory: ${{ needs.setup.outputs.checkout_path }}
        run: ruff format --check
      - name: Run mypy
        working-directory: ${{ needs.setup.outputs.checkout_path }}
        run: mypy .

  python-test:
    needs: setup
    if: >
      always() && needs.setup.result == 'success' && (
        github.event_name != 'pull_request' ||
        needs.setup.outputs.has_changes == 'true'
      )
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@CHECKOUT_SHA # v4
        with:
          ref: ${{ needs.setup.outputs.ref }}
          path: ${{ needs.setup.outputs.checkout_path }}
          repository: ${{ needs.setup.outputs.repo }}
          persist-credentials: false
      - uses: astral-sh/setup-uv@SETUP_UV_SHA # v6
      - name: Run tests with coverage
        working-directory: ${{ needs.setup.outputs.checkout_path }}
        run: |
          uv run --with pytest --with pytest-cov \
            pytest tests/ -v \
            --cov=. \
            --cov-report=term-missing \
            --cov-fail-under=80
```

#### `pyproject.toml` (`generate-build-matrix` only)

Minimal config for ruff and mypy, derived from phlex's `pyproject.toml`:

```toml
[tool.mypy]
ignore_missing_imports = true
no_implicit_optional = true
show_error_codes = true
warn_unreachable = true

[tool.ruff]
target-version = "py312"
line-length = 99

[tool.ruff.lint]
select = ["B", "C4", "D", "E", "F", "I", "W"]
ignore = ["E731", "E203"]

[tool.ruff.lint.pydocstyle]
convention = "google"

[tool.ruff.lint.isort]
known-first-party = ["generate_matrix"]

[tool.ruff.format]
quote-style = "double"

[tool.coverage.run]
omit = ["tests/*"]
```

---

## Phase 1 script structure

The script `docs/dev/add-action-ci.sh` is organized into sections:

```text
Section 0: argument parsing, constants, helper functions (resolve_tag_sha, etc.)
Section 1: resolve all action SHAs (call resolve_tag_sha once each; fail fast if any API call fails)
Section 2: clone or refresh the working directory
Section 3: write files (one function per file; each function is idempotent)
Section 4: delete .github/workflows/actionlint.yaml (replaced by ci.yaml)
Section 5: stage, commit (--no-verify), push
Section 6: for generate-build-matrix only, write additional files:
           - pyproject.toml
           - .github/workflows/python-check.yaml WITH --cov-fail-under=0 (Phase 3
             raises it to 80 alongside the real tests)
           - tests/__init__.py (empty) so pytest collects the package
           - tests/test_smoke.py: a single `import generate_matrix` assertion so the
             python-test job is green on main until Phase 3 lands real tests
```

The script exits non-zero at the first failure. It is safe to re-run; files already
matching the target content are not recommitted (compare SHA before writing).

Commit messages:

- Common files: `"ci: add CI workflows, release config, AGENTS, RELEASES, and pre-commit"`
- Generate-build-matrix extra files: `"ci: add Python check workflow, pyproject.toml, and tests skeleton"`

### Idempotency guards

| Condition | Guard |
| --------- | ----- |
| Repo working dir exists and is clean | Skip clone, just `git pull --ff-only` |
| File already exists with identical content | Skip write (compare git blob SHA) |
| Old `actionlint.yaml` already deleted | `git rm --ignore-unmatch` |
| Nothing staged after writes | Skip commit and push |

---

## Phase 2 — Run the script against all 15 repositories

Run the script once per repo. All 15 can run in parallel:

```bash
for name in \
  build-cmake collect-format-results complete-pr-comment configure-cmake \
  detect-act-env detect-relevant-changes generate-build-matrix get-pr-info \
  handle-fix-commit post-clang-tidy-results prepare-check-outputs \
  prepare-fix-outputs run-change-detection setup-build-env workflow-setup; do
  bash docs/dev/add-action-ci.sh "${name}" &
done
wait
```

### Phase 2 Gate

After all 15 runs complete, verify:

```bash
#!/usr/bin/env bash
set -euo pipefail
ORG="Framework-R-D"
ALL_ACTIONS=(
  build-cmake collect-format-results complete-pr-comment configure-cmake
  detect-act-env detect-relevant-changes generate-build-matrix get-pr-info
  handle-fix-commit post-clang-tidy-results prepare-check-outputs
  prepare-fix-outputs run-change-detection setup-build-env workflow-setup
)
REQUIRED_FILES=(
  ".github/release.yaml"
  ".github/workflows/ci.yaml"
  ".github/actionlint.yaml"
  ".markdownlint.jsonc"
  ".yamllint.yaml"
  ".pre-commit-config.yaml"
  "RELEASES.md"
  "AGENTS.md"
)
ERRORS=()
for name in "${ALL_ACTIONS[@]}"; do
  repo="action-${name}"
  for f in "${REQUIRED_FILES[@]}"; do
    if ! gh api "repos/${ORG}/${repo}/contents/${f}" --jq '.name' &>/dev/null; then
      ERRORS+=("${repo}: missing ${f}")
    fi
  done
  # Verify old actionlint.yaml is gone
  if gh api "repos/${ORG}/${repo}/contents/.github/workflows/actionlint.yaml" &>/dev/null; then
    ERRORS+=("${repo}: old actionlint.yaml still present")
  fi
done
# Check generate-build-matrix extras written in Phase 2 (NOT the test file, which
# is authored in Phase 3 and checked by the Phase 3 verification instead).
for f in "pyproject.toml" ".github/workflows/python-check.yaml"; do
  if ! gh api "repos/${ORG}/action-generate-build-matrix/contents/${f}" --jq '.name' &>/dev/null; then
    ERRORS+=("action-generate-build-matrix: missing ${f}")
  fi
done
if [ "${#ERRORS[@]}" -gt 0 ]; then
  printf 'FAILED:\n'; printf '  %s\n' "${ERRORS[@]}"; exit 1
fi
echo "Phase 2 Gate: all checks passed."
```

---

## Phase 3 — Author `tests/test_generate_matrix.py`

Phase 3 is executed separately from Phase 2 because it requires human judgment
on test coverage, not just mechanical file creation.

> **Phase-ordering hazard.** Phase 2 commits `python-check.yaml` to
> `action-generate-build-matrix`. That workflow's `python-test` job runs
> `pytest tests/ --cov-fail-under=80`, which will **fail** if `tests/` is empty or
> absent. To avoid a window of red CI on `main`:
>
> - The `add-ci.sh` script (Phase 2) must, for `generate-build-matrix` only, commit a
>   placeholder `tests/test_smoke.py` containing a single trivial test that imports
>   `generate_matrix` and asserts it loaded, plus a `tests/__init__.py` if needed.
>   This keeps `pytest` green (a smoke import gives well above 0% but likely below
>   80%, so the script must set `--cov-fail-under=0` in the **placeholder** commit and
>   Phase 3 raises it to 80 once real tests exist). Simpler and recommended: have
>   Phase 2 write `python-check.yaml` with `--cov-fail-under=0`, and Phase 3 bump it to
>   80 in the same commit that adds the real tests.
> - Do not merge any unrelated PR to `action-generate-build-matrix` between Phase 2
>   and Phase 3.

The test file must cover `generate_matrix.py`'s `get_default_combinations()` and
`main()` functions with at least 80% line coverage (enforced by `--cov-fail-under=80`,
which Phase 3 sets in `python-check.yaml`). The module is ~90 lines with no branches
that are infeasible to reach, so 80% is comfortably achievable; aim for 100%.

### Test cases to cover

`get_default_combinations(event_name, all_combinations)`:

| Test | `event_name` | Expected return |
| ---- | ------------ | --------------- |
| push → minimal | `"push"` | `["gcc/none"]` |
| pull_request → minimal | `"pull_request"` | `["gcc/none"]` |
| issue_comment → minimal | `"issue_comment"` | `["gcc/none"]` |
| workflow_dispatch → minimal | `"workflow_dispatch"` | `["gcc/none"]` |
| schedule → perfetto | `"schedule"` | `["gcc/perfetto"]` |
| unknown → minimal | `"unknown_event"` | `["gcc/none"]` |

`main()` (via `subprocess` or by mocking `os.getenv` and `open`):

| Test | `GITHUB_EVENT_NAME` | `USER_INPUT` | `COMMENT_BODY` | Expected `matrix` key |
| ---- | ------------------- | ------------ | -------------- | --------------------- |
| no input, PR event | `"pull_request"` | `""` | `""` | contains `{"compiler":"gcc","sanitizer":"none"}` only |
| no input, schedule | `"schedule"` | `""` | `""` | contains `{"compiler":"gcc","sanitizer":"perfetto"}` only |
| explicit single combo | `"pull_request"` | `"clang/asan"` | `""` | `[{"compiler":"clang","sanitizer":"asan"}]` |
| explicit multiple combos | `"pull_request"` | `"gcc/none clang/tsan"` | `""` | two entries |
| `all` modifier | `"pull_request"` | `"all"` | `""` | all 10 combinations |
| `+` modifier | `"pull_request"` | `"+clang/none"` | `""` | `gcc/none` + `clang/none` |
| `-` modifier | `"pull_request"` | `"-gcc/none +clang/none"` | `""` | `clang/none` only |
| issue_comment trigger, `@phlexbot build` | `"issue_comment"` | `""` | `"@phlexbot build clang/asan"` | `[{"compiler":"clang","sanitizer":"asan"}]` |
| issue_comment, no match | `"issue_comment"` | `""` | `"some other comment"` | default (gcc/none) |

Phase 3 commits, in a single commit to `action-generate-build-matrix` `main`
(message `"test: add unit tests for generate_matrix.py"`):

1. The real `tests/test_generate_matrix.py` (replacing `tests/test_smoke.py`, which is
   deleted in the same commit).
2. An edit to `.github/workflows/python-check.yaml` changing `--cov-fail-under=0` to
   `--cov-fail-under=80`.

The commit may be made via a local clone + push, or via the GitHub API. A local clone
is simpler here because two files change plus one deletion; verify CI passes on the
resulting `main` (or open it as a PR so `python-check` runs before merge — preferred,
since branch protection may exist by the time Phase 3 runs).

### Test file template

Write the test file at `tests/test_generate_matrix.py`. Use `pytest` with
`monkeypatch` for environment variables and `tmp_path` for the fake `GITHUB_OUTPUT`
file. Key imports: `pytest`, `json`, `os`, `sys`; add the repo root to `sys.path`
so `import generate_matrix` resolves. Structure:

```python
"""Tests for generate_matrix.py."""

from __future__ import annotations

import json
import os
import sys
from pathlib import Path

import pytest

# Allow importing generate_matrix from the repo root
sys.path.insert(0, str(Path(__file__).parent.parent))
import generate_matrix  # noqa: E402


class TestGetDefaultCombinations:
    """Tests for get_default_combinations()."""

    ALL = [
        "gcc/none", "gcc/asan", "gcc/tsan", "gcc/valgrind", "gcc/perfetto",
        "clang/none", "clang/asan", "clang/tsan", "clang/valgrind", "clang/perfetto",
    ]

    # ... parametrize all six cases from the table above


class TestMain:
    """Tests for main() via environment variable injection."""

    def _run_main(self, monkeypatch, tmp_path, *, event, user_input="", comment_body=""):
        output_file = tmp_path / "github_output"
        output_file.write_text("")
        monkeypatch.setenv("GITHUB_EVENT_NAME", event)
        monkeypatch.setenv("USER_INPUT", user_input)
        monkeypatch.setenv("COMMENT_BODY", comment_body)
        monkeypatch.setenv("GITHUB_OUTPUT", str(output_file))
        generate_matrix.main()
        content = output_file.read_text()
        matrix_line = next(l for l in content.splitlines() if l.startswith("matrix="))
        return json.loads(matrix_line.split("=", 1)[1])

    # ... implement all nine test cases from the table above
```

---

## Verification

After Phase 3 completes:

1. Each action repo's `main` branch contains all required files (Phase 2 Gate
   confirms this).
2. CI workflow triggers on a dummy PR to `action-detect-act-env` (or any Level 0
   repo) and the relevant `ci.yaml` jobs (`actionlint`, `yaml-check`,
   `markdown-check`, `codeql`) pass (or skip cleanly when not relevant).
3. For `action-generate-build-matrix`: `python-test` job passes with ≥ 80% coverage.
4. `gh api repos/Framework-R-D/action-REPO/contents/.github/workflows/actionlint.yaml`
   returns 404 for all 15 repos (old workflow deleted).
5. `pre-commit run --all-files` in a local clone of any action repo passes cleanly.

---

## Decisions

- **Self-bootstrapping CI**: action repos' `ci.yaml` uses `action-workflow-setup`
  and `action-run-change-detection` (SHA-pinned). Dependabot keeps those pins
  current via the existing `dependabot.yml`. This is consistent with the phlex
  pattern.
- **Integer-only tags** (`v1`, `v2`, …): inherited from the export plan. No minor/patch
  releases. Every `CHANGELOG.md` section heading must match the tag name exactly.
- **CHANGELOG.md excluded from markdownlint**: same as phlex. The `## v1 --- DATE`
  heading format is not standard Markdown and markdownlint would flag it.
- **`--cov-fail-under=80`**: chosen as a practical minimum for a small, logic-dense
  module. Can be raised once the tests are passing.
- **No `WORKFLOW_PAT` secret required**: action repos have no fix-and-commit
  workflows (no `markdown-fix`, `yaml-fix` equivalents). All formatting is enforced
  pre-commit or locally; CI is check-only. This keeps the secret surface minimal.
- **`dependabot-auto-merge.yaml` not added**: auto-merge requires branch protection
  rules to be configured per repo. This is an optional follow-on; defer to a
  separate issue.
- **CodeQL `security-events: write` permission**: required for
  `codeql-action/analyze` to upload SARIF to the repo's code scanning dashboard.
  Without it, the job silently skips the upload; alerts would not appear in the
  Security tab.

---

## Session Factorization and Agent/Context Levels

This plan is designed so that the cheapest capable agent executes each phase with only
the context it needs. Each session below lists: the agent capability tier, the exact
sections of this plan to load, the inputs it consumes, the outputs it produces, and
the gate that must pass before the next session starts.

Agent capability tiers (cheapest first):

- **Tier S (small/cheap)**: mechanical execution — run provided commands, fill
  templates, no design judgment. Suitable for a fast, inexpensive model.
- **Tier M (medium)**: writes a parameterized shell script from a precise spec,
  reconciles minor API/interface drift, debugs predictable failures.
- **Tier L (large/expensive)**: writes correctness-sensitive code (unit tests),
  exercises judgment about coverage and edge cases. Use sparingly.

| Session | Phase(s) | Tier | Sections to load | Produces |
| ------- | -------- | ---- | ---------------- | -------- |
| 1. Preflight | 0 | S | "How to execute", Phase 0 | Verified environment; abort on failure |
| 2. Author `add-ci.sh` | 1 | M | Phase 1 (all file specs), "Dependency Ordering and Constraints", "Phase 1 script structure" | `docs/dev/add-action-ci.sh` committed to the migration branch |
| 3. Deploy to repos | 2 | S | Phase 2, "Phase 2 Gate" | All 15 repos carry the common files; gate green |
| 4. Author matrix tests | 3 | L | Phase 3, the `generate_matrix.py` source (fetch from repo) | `tests/test_generate_matrix.py` + threshold bump; CI green |
| 5. Verify | Verification | S | "Verification" | Confirmation report; file follow-on issues |

### Session 1 — Preflight (Tier S)

- **Context**: only Phase 0 and the execution-order table.
- **Inputs**: `gh` auth, push credentials, git signing config.
- **Outputs**: a pass/fail determination. On any failure, stop and surface the exact
  failing check; do not proceed.
- **Why Tier S**: purely runs the provided commands and reads exit codes.

### Session 2 — Author `add-ci.sh` (Tier M)

- **Context**: the entire Phase 1 file-spec set (every `####` file subsection),
  "Dependency Ordering and Constraints" (for SHA resolution and the interface facts),
  and "Phase 1 script structure" (section layout + idempotency guards).
- **Inputs**: the file templates in Phase 1; the live `action.yaml` of each repo (to
  populate per-repo inputs/outputs tables in `AGENTS.md`).
- **Outputs**: `docs/dev/add-action-ci.sh`, committed (`--no-verify`) to
  `maintenance/export-reusable-actions`. The script must pass `shellcheck` and a
  dry-run against one repo (`detect-act-env`) in a scratch clone before being
  considered done.
- **Critical instructions**: resolve every action SHA at script-write time via
  `resolve_tag_sha` (never hard-code from the plan); honor the
  `generate-build-matrix` placeholder-tests requirement (`--cov-fail-under=0` +
  smoke test) so Phase 2 does not leave red CI on `main`.
- **Why Tier M**: writing a robust, idempotent, parameterized Bash script and
  reconciling any drift between the plan's stated interfaces and the live
  `action.yaml` files requires more than mechanical substitution, but it is not
  correctness-critical logic.
- **Gate before Session 3**: `shellcheck docs/dev/add-action-ci.sh` clean; single-repo
  dry run produces the expected file set in a scratch clone without pushing.

### Session 3 — Deploy to all 15 repositories (Tier S)

- **Context**: Phase 2 and the Phase 2 Gate only. Does **not** need the file specs;
  the script encapsulates them.
- **Inputs**: `docs/dev/add-action-ci.sh` from Session 2.
- **Outputs**: each repo's `main` carries the common files (and the
  `generate-build-matrix` extras incl. placeholder tests); old `actionlint.yaml`
  removed. The Phase 2 Gate script passes.
- **Why Tier S**: runs the script in a parallel loop and then runs the gate. No
  judgment required; the script and gate are self-checking.
- **Gate before Session 4**: the Phase 2 Gate prints "all checks passed".

### Session 4 — Author `generate_matrix.py` tests (Tier L)

- **Context**: Phase 3 only, plus the actual `generate_matrix.py` source fetched
  from `action-generate-build-matrix` at session start (do not rely on the excerpt in
  this plan; read the live file).
- **Inputs**: the test-case tables in Phase 3; the live source module.
- **Outputs**: `tests/test_generate_matrix.py` (replacing `tests/test_smoke.py`) and
  `python-check.yaml` threshold bumped to 80, committed via a PR so `python-check`
  runs before merge.
- **Why Tier L**: correctness-sensitive. The tests must genuinely exercise the
  modifier algebra (`+`, `-`, `all`, explicit combos), the `issue_comment` regex path,
  and the `GITHUB_OUTPUT` writing, and must reach ≥ 80% (target 100%) coverage. Weak
  tests that pass coverage without asserting behavior are a real failure mode here.
- **Gate before Session 5**: the PR's `python-check` workflow is green at
  `--cov-fail-under=80`; the PR is merged to `main`.

### Session 5 — Verify and file follow-ons (Tier S)

- **Context**: the "Verification" section only.
- **Inputs**: all prior outputs.
- **Outputs**: a short report confirming each verification item; open follow-on issues
  for the deferred items recorded in "Decisions" (auto-merge workflow + branch
  protection).
- **Why Tier S**: runs the verification checks and reports.

### Parallelism and isolation notes

- Sessions 1→2→3→4→5 are strictly serial across the gates. Within Session 3, the
  15 per-repo invocations are parallel-safe (independent remotes; the script uses a
  per-repo working directory under `${TMPDIR}/action-work/`).
- Sessions 2 and 4 are the only ones that author code; keep them in separate sessions
  so the expensive Tier L context (Session 4) is not paid for the script-writing work.
- If a later session must re-run an earlier one (e.g. the script needed a fix),
  re-enter at Session 2; Sessions 3–5 are idempotent and safe to repeat.
- Agent Manager note: Sessions 2 and 4 are good candidates for isolated worktrees
  (they commit to phlex's migration branch and to an action repo PR, respectively);
  Sessions 1, 3, 5 operate only against remotes and need no worktree.
