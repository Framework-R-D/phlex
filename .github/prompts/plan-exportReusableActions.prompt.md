# Plan: Export GitHub Actions to Standalone Repositories

## Overview

Extract all 15 composite actions from `Framework-R-D/phlex/.github/actions/` into
individual public repositories `Framework-R-D/action-NAME`, preserving git history via
`git filter-repo`. Each new repo gets standard CI infrastructure (actionlint, dependabot),
a versioned release (`v1`), and a standard README + CHANGELOG. phlex workflows are then
updated on branch `maintenance/export-reusable-actions` (PR superseding #614).

The first deliverable is `docs/dev/export-actions-plan.md`, a self-contained operator
guide that any agent can execute end-to-end without further clarification.

## How to execute this plan

This document is long. It is structured so that an agent (or operator) executes one
Phase at a time and can ignore everything outside the current Phase.

- Phases must run in order: 0 -> 1 -> 2 -> 2 Gate -> 3 -> 3 Gate -> 4 -> 4 Gate -> 5 -> 6.
- Within Phases 2-4 the per-action work is parallel-safe; the *Gates* serialize before
  the next Phase begins.
- Each Phase section is self-contained. The Phase 1 section additionally requires the
  *Action Inventory* and *Shell Script Skeleton* sections (Phase 1 packages them into
  the operator guide). Phase 5 additionally requires the *Action Inventory* (it depends
  on the directly-referenced vs transitively-used distinction).
- All Phase 2-4 per-repo work is performed by a single script, `export-action.sh`,
  whose body is the *Shell Script Skeleton* in this document. Reading the script's
  internals is not required to run it; if it aborts, the per-phase *Script-internal
  reference* in the appendix explains each MANDATORY CHECK by label.
- Sub-step labels of the form `6a`, `10b`, `14c`, `18d` are stable cross-reference
  anchors used by the script's MANDATORY CHECK messages. They are not sequential
  numbering and do not need to be read in order.

### Per-Phase reading checklist

| Phase | Sections to load | Action |
| --- | --- | --- |
| 0 | Phase 0 | Run prose commands |
| 1 | Phase 1, Action Inventory, Shell Script Skeleton | Run prose commands; produce `docs/dev/export-actions-plan.md` |
| 2 | Phase 2, Phase 2 Gate | Run script per action; then run Gate once |
| 3 | Phase 3, Phase 3 Gate | Run script per action; then run Gate once |
| 4 | Phase 4, Phase 4 Gate | Run script for `workflow-setup`; then run Gate once |
| 5 | Phase 5, Action Inventory | Run prose commands |
| 6 | Phase 6 | Run prose commands |

If the script aborts during Phase 2, 3, or 4, additionally load the *Appendix:
Script-internal reference* section keyed by the MANDATORY CHECK label printed in
the error message.

---

## Dependency Ordering -- Critical Constraint

Actions must be created in dependency order because Level 1/2 `action.yaml` files
must contain pinned commit SHAs of their dependencies, which only exist after those
dependencies' repos are created and tagged.

**Level 0** (11 actions, no internal deps, all parallelizable):

| Source dir | New repo |
| --- | --- |
| `detect-act-env` | `action-detect-act-env` |
| `detect-relevant-changes` | `action-detect-relevant-changes` |
| `get-pr-info` | `action-get-pr-info` |
| `setup-build-env` | `action-setup-build-env` |
| `configure-cmake` | `action-configure-cmake` |
| `build-cmake` | `action-build-cmake` |
| `collect-format-results` | `action-collect-format-results` |
| `complete-pr-comment` | `action-complete-pr-comment` |
| `generate-build-matrix` | `action-generate-build-matrix` |
| `post-clang-tidy-results` | `action-post-clang-tidy-results` |
| `handle-fix-commit` | `action-handle-fix-commit` |

**Level 1** (3 actions, blocks on Level 0 v1 tag SHAs):

| Source dir | New repo | Pinned deps |
| --- | --- | --- |
| `prepare-check-outputs` | `action-prepare-check-outputs` | `action-detect-act-env`, `action-get-pr-info` |
| `prepare-fix-outputs` | `action-prepare-fix-outputs` | `action-get-pr-info` |
| `run-change-detection` | `action-run-change-detection` | `action-detect-relevant-changes` |

**Level 2** (1 action, blocks on Level 1 v1 tag SHAs):

| Source dir | New repo | Pinned deps |
| --- | --- | --- |
| `workflow-setup` | `action-workflow-setup` | `action-prepare-check-outputs`, `action-prepare-fix-outputs`, `action-run-change-detection` |

---

## Idempotency Guards

All idempotency guards required for safe re-runs (all present in the script or in
the named prose step; no separate guard-insertion step exists):

| Guard | Trigger condition | Location |
| --- | --- | --- |
| Branch already exists | Skip branch creation if `maintenance/export-reusable-actions` exists | Phase 1 step 1 and Phase 5 step 1 |
| Remote repo exists | Skip `gh repo create` if remote already exists | Script section 4 |
| SHA file present | Skip tag creation if `.sha` already recorded | Script section 5 |
| Tag already on remote | Skip `git push origin v1` if tag already pushed | Script section 5 |
| Release already exists | Skip `gh release create` if v1 release already exists | Script section 5 |
| CHANGELOG placeholder replaced | Skip CHANGELOG commit if `<PR>` already replaced | Phase 6 loop |
| `phase6-results` file present | Skip entire Phase 6 subshell if repo already completed | Phase 6 loop |

---

## Phase 0 -- Environment Check

1. Verify `git filter-repo` is installed (install via `pip install git-filter-repo`
   if missing; ensure `~/.local/bin` is on `PATH`):

   ```bash
   command -v git-filter-repo \
     || { echo 'ERROR: git-filter-repo not found'; exit 1; }
   ```

2. Verify `gh` authentication and `git` push credentials for `Framework-R-D`:

   ```bash
   # Basic auth check (also prints the granted scopes; 'repo' must be present)
   gh auth status

   # Programmatic confirmation that the 'repo' scope is granted:
   gh auth status 2>&1 | grep -F "'repo'" \
     || { echo "ERROR: token lacks 'repo' scope -- re-authenticate"; exit 1; }

   # Confirm the org is accessible:
   gh api orgs/Framework-R-D --jq .login

   # Confirm org membership (advisory: 204 = member, 404 = not a public member).
   # If you are a private member, the API may return 302 -- the check is advisory.
   USER_LOGIN=$(gh api user --jq .login)
   STATUS=$(gh api -i "orgs/Framework-R-D/members/${USER_LOGIN}" 2>&1 \
     | awk 'NR==1 && match($0, /HTTP\/[0-9.]+ ([0-9]+)/, m) {print m[1]; exit}')
   case "${STATUS}" in
     204) echo "OK: public org member" ;;
     302|404) echo "WARNING: membership check inconclusive (status=${STATUS}); attempt repo creation and check for 403" ;;
     *) echo "WARNING: unexpected status=${STATUS} from members endpoint" ;;
   esac

   # Configure git to use gh credentials for HTTPS pushes:
   gh auth setup-git

   # Smoke test: a non-interactive push-credential check against an existing repo.
   git ls-remote https://github.com/Framework-R-D/phlex.git HEAD >/dev/null \
     || { echo 'ERROR: gh credential helper not active -- re-run gh auth setup-git'; exit 1; }
   ```

   If not authenticated: run `gh auth login` (select HTTPS + browser) or
   `gh auth login --with-token <<< "${GH_TOKEN}"`; the PAT must have `repo` scope.
   If org-repo-creation fails with 403, ask an org owner to grant
   "Can create repositories" to your account.

   Do not use `gh auth status --show-token`: it prints the token to the log.

3. Disable commit and tag signing if the configured signing key is absent.
   The export script commits directly into newly-created repos; a missing key
   causes every commit to fail with `fatal: failed to write commit object`.

   ```bash
   # If commit.gpgsign or tag.gpgsign is true, check whether the key exists.
   # If not, disable signing for this session (re-enable after Phase 6 if desired).
   SIGKEY=$(git config --global user.signingkey 2>/dev/null || true)
   if git config --global --bool commit.gpgsign 2>/dev/null | grep -q true; then
     if [ -z "${SIGKEY}" ] || [ ! -f "${SIGKEY/#\~/$HOME}" ]; then
       echo "WARNING: commit.gpgsign=true but signing key absent -- disabling for this session"
       git config --global commit.gpgsign false
     fi
   fi
   if git config --global --bool tag.gpgsign 2>/dev/null | grep -q true; then
     if [ -z "${SIGKEY}" ] || [ ! -f "${SIGKEY/#\~/$HOME}" ]; then
       echo "WARNING: tag.gpgsign=true but signing key absent -- disabling for this session"
       git config --global tag.gpgsign false
       git config --global tag.forcesignannotated false
     fi
   fi
   ```

   If the signing key *is* present (e.g. a GPG key or a resident SSH key reachable
   via `ssh-agent`), this check is a no-op and signing will proceed normally.

4. Confirm the source history is present and not shallow:

   ```bash
   git -C /workspaces/phlex log --oneline -- .github/actions/ >/dev/null \
     || { echo 'ERROR: phlex source history for .github/actions/ is empty'; exit 1; }
   if git -C /workspaces/phlex rev-parse --is-shallow-repository | grep -q true; then
     echo 'ERROR: phlex repo is shallow -- run: git fetch --unshallow'
     exit 1
   fi
   ```

---

## Phase 1 -- Implementation Guide *(first deliverable)*

This Phase produces `docs/dev/export-actions-plan.md`, the self-contained operator
guide, and commits it as the first commit on the migration branch.

The Shell Script Skeleton in the appendix of this file is already fully expanded
(all heredocs are inline). Phase 1 treats it as data to copy verbatim. Do not
execute the script during Phase 1; execution happens in Phases 2-4.

1. Write `docs/dev/export-actions-plan.md` with exactly two top-level sections:

   - **Section A: Shell Script Skeleton.** Copy the entire `## Shell Script
     Skeleton` section from this file verbatim, including the heredocs and the
     `## Appendix: Script-internal reference` section. The script consumes two
     environment variables, `README_INPUTS` and `README_OUTPUTS`, which the
     operator sets to pipe-delimited Markdown table rows (one row per input/output)
     before invoking the script. Example:

     ```bash
     export README_INPUTS="| name | Description | required | default |"
      export README_OUTPUTS="| name | Description |"
      ```

   - **Section B: Per-action argument table.** A Markdown table with four columns
      -- *Action Name*, *Level*, *Description argument*, *Dep KEY=SHA arguments*
      -- where each row contains the exact arguments to pass to
      `export-action.sh` for that action.

      Populate the rows by reading the *Action Inventory* section of this file.
      Do not copy Inventory prose into the output; only emit the derived arguments.

      **Table format:** Use spaces around `|` characters (extensible style).

      **Description column rules:**

      - Use the full text of the `**Purpose**:` bullet, up to but not including the
        next bullet starting with `**Inputs**`, `**Outputs**`, `**Files**`,
        `**External deps**`, `**Note**`, `**Internal deps to repin**`,
        `**README_INPUTS**`, or `**Post-creation issue**`. Treat the Purpose as a
        single string regardless of internal line breaks.
      - Bold inline text within Purpose (e.g. `**Requires the \`phlex-ci\`
        container**`) is part of the description and must be included. The
        Markdown markers (`**`, backticks) will appear literally in the GitHub
        repo description, which renders as plain text -- this is cosmetic only
        and acceptable.
      - Enclose the result in single quotes. If the text itself contains a single
        quote, close the single-quoted string, append `\'`, and reopen
        (`'...don'\''t...'`). Backticks inside single quotes are literal and
        require no escaping.

      **Dep KEY=SHA column rules:**

      - For Level 0 actions: the cell is exactly the literal text `(none)`.
      - For Level 1 and Level 2 actions: emit one `KEY=FILL-AFTER-PHASE-2-COMPLETES`
        (Level 1) or `KEY=FILL-AFTER-PHASE-3-COMPLETES` (Level 2) per dep, joined
        by ` ` (space) within the cell. KEY is the bare source directory name of the
        dep (e.g. `detect-act-env`), *not* the new repo name. Example for
        `prepare-check-outputs`:

        ```text
        detect-act-env=FILL-AFTER-PHASE-2-COMPLETES get-pr-info=FILL-AFTER-PHASE-2-COMPLETES
        ```

      - The operator replaces the `FILL-AFTER-PHASE-N-COMPLETES` tokens with
        actual SHAs from the corresponding `.sha` files before invoking the script
        at the start of Phase 3 / Phase 4.

      **`generate-build-matrix` special case:** Append a footnote to that row:
      "`README_INPUTS` / `README_OUTPUTS` cover `action.yaml` fields only; see
      Action Inventory note for `generate_matrix.py` CLI exclusion."

2. Create the migration branch and commit the guide as the sole file in the first
   commit on that branch:

   ```bash
   cd /workspaces/phlex
   git checkout main && git pull --ff-only
   # Idempotency guard: 'Branch already exists'
   if git show-ref --verify --quiet refs/heads/maintenance/export-reusable-actions; then
     git checkout maintenance/export-reusable-actions
   else
     git checkout -b maintenance/export-reusable-actions
   fi
   git add docs/dev/export-actions-plan.md
   git commit --no-verify -m 'docs: add export-actions-plan'
   ```

3. Verify the first commit on the branch (counting from `main`) touches exactly
   `docs/dev/export-actions-plan.md` and nothing else:

   ```bash
   FIRST_COMMIT=$(git rev-list main..HEAD | tail -1)
   FILES=$(git show --pretty='' --name-only "${FIRST_COMMIT}")
   [ "${FILES}" = "docs/dev/export-actions-plan.md" ] \
     || { echo "ERROR: first branch commit contains files other than the plan: ${FILES}"; exit 1; }
   echo 'OK: first commit on branch contains exactly export-actions-plan.md'
   ```

---

## Phase 2 -- Level 0 Repositories *(parallel within Phase; per-action)*

Phase 2 runs `export-action.sh` once per Level 0 action (11 actions, all
parallel-safe). Each invocation produces one new public repo, one annotated v1
tag, one GitHub release, and writes `${TMPDIR:-/tmp}/action-work/NAME.sha` for
consumption by later Phases.

Per-action invocation:

```bash
export README_INPUTS="<rows from Action Inventory for NAME>"
export README_OUTPUTS="<rows from Action Inventory for NAME>"
bash export-action.sh NAME "<description for NAME>"
```

To run all 11 Level 0 actions in parallel (`README_INPUTS` / `README_OUTPUTS`
must be set per action; the simplest approach is one terminal per action or a
driver script that exports them per iteration):

```bash
# Driver sketch -- adapt to your operator workflow. Sequential execution is
# permitted but slower; correctness is identical.
for name in detect-act-env detect-relevant-changes get-pr-info setup-build-env \
            configure-cmake build-cmake collect-format-results complete-pr-comment \
            generate-build-matrix post-clang-tidy-results handle-fix-commit; do
  (
    # Set README_INPUTS / README_OUTPUTS for this action (from Phase 1 table)
    export README_INPUTS="..." README_OUTPUTS="..."
    bash export-action.sh "${name}" "<description>"
  ) &
done
wait
```

After all 11 invocations exit, run the **Phase 2 Gate** (next section).

If a script invocation aborts with an error message naming a MANDATORY CHECK
(`6a`, `6b`, `6c`, `6d`), consult the *Appendix: Script-internal reference*
entry with the matching label.

**Expected pushes per repo during Phase 2:** exactly two pushes to `main`.
First push: the "Add license, README, changelog, and CI" commit
(script section 4). Second push: the "Pin README usage example to v1 commit
SHA" commit (script section 5). One additional push happens in Phase 6 for the
CHANGELOG `<PR>` substitution. More than two pushes during Phase 2 indicates a
script bug; investigate before continuing.

### Phase 2 Gate -- Level 0 SHA Verification

> Run this **once** in a fresh shell after all 11 parallel Level 0 scripts have
> exited. Do not embed this check inside any per-action script. Save as
> `phase2-gate.sh` and invoke with `bash phase2-gate.sh`.

```bash
#!/usr/bin/env bash
set -euo pipefail
WORK_DIR="${TMPDIR:-/tmp}/action-work"
ORG="Framework-R-D"
LEVEL0=(detect-act-env detect-relevant-changes get-pr-info setup-build-env
        configure-cmake build-cmake collect-format-results complete-pr-comment
        generate-build-matrix post-clang-tidy-results handle-fix-commit)

for name in "${LEVEL0[@]}"; do
  SHA_FILE="${WORK_DIR}/${name}.sha"
  test -f "${SHA_FILE}" \
    || { echo "ERROR: missing SHA file for ${name} -- re-run its export script"; exit 1; }
  SHA_VAL=$(cat "${SHA_FILE}")
  [[ "${SHA_VAL}" =~ ^[0-9a-f]{40}$ ]] \
    || { echo "ERROR: SHA file for ${name} contains invalid value: '${SHA_VAL}'"; exit 1; }
  # Resolve remote tag, dereferencing annotated-tag objects
  REF_SHA=$(gh api "repos/${ORG}/action-${name}/git/ref/tags/v1" --jq .object.sha)
  REF_TYPE=$(gh api "repos/${ORG}/action-${name}/git/ref/tags/v1" --jq .object.type)
  if [ "${REF_TYPE}" = "tag" ]; then
    REMOTE_SHA=$(gh api "repos/${ORG}/action-${name}/git/tags/${REF_SHA}" --jq .object.sha)
  else
    REMOTE_SHA="${REF_SHA}"
  fi
  [ "${REMOTE_SHA}" = "${SHA_VAL}" ] \
    || { echo "ERROR: ${name}: SHA file (${SHA_VAL}) != remote v1 tag commit (${REMOTE_SHA})"; exit 1; }
done

echo 'All 11 Level 0 SHA files present, valid, and matching remote tags.'
echo
echo 'MANUAL REMINDER: file a GitHub issue on Framework-R-D/action-generate-build-matrix'
echo 'requesting a python-check.yaml workflow (ruff + mypy) and unit tests for'
echo 'generate_matrix.py before closing out the Phase 2 work.'
```

---

## Phase 3 -- Level 1 Repositories *(parallel within Phase; per-action)*

Phase 3 runs the same `export-action.sh` script as Phase 2, but passes additional
`KEY=SHA` positional arguments naming the Level 0 deps each Level 1 action
depends on. The script's section 2 replaces every
`Framework-R-D/phlex/.github/actions/${KEY}@main` reference in `action.yaml`
with `Framework-R-D/action-${KEY}@${SHA} # v1`.

Per-action invocation (Level 1):

```bash
WORK_DIR="${TMPDIR:-/tmp}/action-work"
export README_INPUTS="..." README_OUTPUTS="..."
bash export-action.sh prepare-check-outputs "<description>" \
  detect-act-env=$(cat "${WORK_DIR}/detect-act-env.sha") \
  get-pr-info=$(cat "${WORK_DIR}/get-pr-info.sha")
```

Run all 3 Level 1 actions in parallel as in Phase 2. After all exit, run the
**Phase 3 Gate**.

If the script aborts with a MANDATORY CHECK label (`14a`), consult the
*Appendix: Script-internal reference*.

**Non-`@main` dep references:** the script assumes every Level 1/2 internal dep
reference uses `@main` in the phlex `action.yaml`. If your phlex history uses a
different suffix (e.g. a pinned SHA or branch name in a fork), override at
invocation time:

```bash
DEP_REF_SUFFIX="@some-branch" bash export-action.sh NAME "..." KEY=SHA ...
```

The script's section-2 sed pattern uses `${DEP_REF_SUFFIX:-@main}`; no script
edit is required.

### Phase 3 Gate -- Level 1 SHA Verification

> Run this **once** after all 3 Level 1 scripts exit. Save as `phase3-gate.sh`
> and invoke with `bash phase3-gate.sh`.

```bash
#!/usr/bin/env bash
set -euo pipefail
WORK_DIR="${TMPDIR:-/tmp}/action-work"
ORG="Framework-R-D"
LEVEL1=(prepare-check-outputs prepare-fix-outputs run-change-detection)

for name in "${LEVEL1[@]}"; do
  SHA_FILE="${WORK_DIR}/${name}.sha"
  test -f "${SHA_FILE}" \
    || { echo "ERROR: missing SHA file for ${name}"; exit 1; }
  SHA_VAL=$(cat "${SHA_FILE}")
  [[ "${SHA_VAL}" =~ ^[0-9a-f]{40}$ ]] \
    || { echo "ERROR: invalid SHA in file for ${name}: '${SHA_VAL}'"; exit 1; }
  REF_SHA=$(gh api "repos/${ORG}/action-${name}/git/ref/tags/v1" --jq .object.sha)
  REF_TYPE=$(gh api "repos/${ORG}/action-${name}/git/ref/tags/v1" --jq .object.type)
  if [ "${REF_TYPE}" = "tag" ]; then
    REMOTE_SHA=$(gh api "repos/${ORG}/action-${name}/git/tags/${REF_SHA}" --jq .object.sha)
  else
    REMOTE_SHA="${REF_SHA}"
  fi
  [ "${REMOTE_SHA}" = "${SHA_VAL}" ] \
    || { echo "ERROR: ${name}: SHA file (${SHA_VAL}) != remote v1 tag (${REMOTE_SHA})"; exit 1; }
done
echo 'All 3 Level 1 SHA files present, valid, and matching remote tags.'
```

---

## Phase 4 -- Level 2 Repository *(one action; depends on Phase 3 Gate)*

Phase 4 runs `export-action.sh` once for `workflow-setup`, passing the three
Level 1 SHAs as dep arguments:

```bash
WORK_DIR="${TMPDIR:-/tmp}/action-work"
export README_INPUTS="..." README_OUTPUTS="..."
bash export-action.sh workflow-setup "<description>" \
  prepare-check-outputs=$(cat "${WORK_DIR}/prepare-check-outputs.sha") \
  prepare-fix-outputs=$(cat "${WORK_DIR}/prepare-fix-outputs.sha") \
  run-change-detection=$(cat "${WORK_DIR}/run-change-detection.sha")
```

After it exits, run the **Phase 4 Gate**.

### Phase 4 Gate -- Level 2 SHA Verification

> Run this once after the `workflow-setup` script exits. Save as
> `phase4-gate.sh` and invoke with `bash phase4-gate.sh`.

```bash
#!/usr/bin/env bash
set -euo pipefail
WORK_DIR="${TMPDIR:-/tmp}/action-work"
ORG="Framework-R-D"
name="workflow-setup"

SHA_FILE="${WORK_DIR}/${name}.sha"
test -f "${SHA_FILE}" \
  || { echo "ERROR: missing SHA file for ${name}"; exit 1; }
SHA_VAL=$(cat "${SHA_FILE}")
[[ "${SHA_VAL}" =~ ^[0-9a-f]{40}$ ]] \
  || { echo "ERROR: invalid SHA in file for ${name}: '${SHA_VAL}'"; exit 1; }
REF_SHA=$(gh api "repos/${ORG}/action-${name}/git/ref/tags/v1" --jq .object.sha)
REF_TYPE=$(gh api "repos/${ORG}/action-${name}/git/ref/tags/v1" --jq .object.type)
if [ "${REF_TYPE}" = "tag" ]; then
  REMOTE_SHA=$(gh api "repos/${ORG}/action-${name}/git/tags/${REF_SHA}" --jq .object.sha)
else
  REMOTE_SHA="${REF_SHA}"
fi
[ "${REMOTE_SHA}" = "${SHA_VAL}" ] \
  || { echo "ERROR: ${name}: SHA file (${SHA_VAL}) != remote v1 tag (${REMOTE_SHA})"; exit 1; }
echo 'Level 2 SHA file present, valid, and matching remote tag.'
```

---

## Phase 5 -- Update phlex *(depends on Phases 2-4 Gates; all 15 v1 SHAs known)*

Phase 5 replaces every internal `Framework-R-D/phlex/.github/actions/NAME@main`
reference in `.github/workflows/` with the corresponding standalone-repo SHA,
deletes the bundled `.github/actions/` directory, and opens the migration PR.

There is no shell-script equivalent for Phase 5; run the prose commands directly.

**Note:** `.github/actions/` contains 15 action subdirectories *and* one
`README.md` file. The `git rm -r .github/actions/` in step 4 removes the README
too; this is intended.

Define the portable `sed -i` helper and the WORK_DIR at the top of any shell
session that runs Phase 5:

```bash
sed_inplace() { if [[ "$(uname)" == Darwin ]]; then sed -i '' "$@"; else sed -i "$@"; fi; }
WORK_DIR="${TMPDIR:-/tmp}/action-work"
cd /workspaces/phlex   # or wherever the phlex working copy is
```

1. Ensure the working copy is on an up-to-date `main`, then check out the
   migration branch (idempotent re-runs are safe):

   ```bash
   git checkout main && git pull --ff-only
   # Idempotency guard: 'Branch already exists'
   if git show-ref --verify --quiet refs/heads/maintenance/export-reusable-actions; then
     git checkout maintenance/export-reusable-actions
   else
     git checkout -b maintenance/export-reusable-actions
   fi
   ```

2. Pre-replacement validation. Run all four sub-checks; STOP at the first failure
   and resolve before continuing to step 3.

   **18a.** Confirm the directly-referenced internal-action set matches the
   expected inventory:

   ```bash
   FOUND=$(grep -rh "Framework-R-D/phlex/.github/actions" .github/workflows/ \
     | sed "s|.*actions/||;s|@.*||" | sort -u)
   EXPECTED=$(printf '%s\n' \
     build-cmake collect-format-results complete-pr-comment configure-cmake \
     generate-build-matrix handle-fix-commit post-clang-tidy-results run-change-detection \
     setup-build-env workflow-setup | sort)
   diff <(echo "${FOUND}") <(echo "${EXPECTED}") \
     || { echo 'ERROR: referenced-action set diverges from EXPECTED -- update inventory and re-run'; exit 1; }
   ```

   If the diff fails, inspect the raw grep output to determine whether YAML
   anchors or multi-line `uses:` values produced malformed names:

   ```bash
   grep -rh "Framework-R-D/phlex/.github/actions" .github/workflows/
   ```

   The 5 transitively-used Level 0 actions (`detect-act-env`, `get-pr-info`,
   `detect-relevant-changes`, `prepare-check-outputs`, `prepare-fix-outputs`)
   are intentionally absent from the directly-referenced set.

   **18b.** Verify all internal references use `@main`:

   ```bash
   if grep -r 'Framework-R-D/phlex/.github/actions' .github/workflows/ \
        | grep -v '@main' >/dev/null; then
     echo 'ERROR: some internal references do not use @main -- adjust manually before sed'
     exit 1
   fi
   ```

   **18c.** Detect YAML anchors that define action references (sed cannot
   dereference aliases):

   ```bash
   if grep -rn '&.*actions/' .github/workflows/ \
        | grep -q 'Framework-R-D/phlex'; then
     echo 'ERROR: phlex action references inside YAML anchors -- expand the anchor before sed'
     exit 1
   fi
   ```

   **18d.** Detect dynamic `uses:` expressions:

   ```bash
   if grep -rn 'uses:.*\${{' .github/workflows/ >/dev/null; then
     echo 'WARNING: dynamic uses: expressions present -- inspect each match:'
     grep -rn 'uses:.*\${{' .github/workflows/
     echo 'STOP if any expression resolves to an internal phlex action path; resolve manually before continuing.'
     exit 1
   fi
   ```

3. Replace every internal reference. The sed delimiter is `|` to avoid escaping
   forward slashes.

   ```bash
   mapfile -t WORKFLOW_FILES < <(ls .github/workflows/*.yaml .github/workflows/*.yml 2>/dev/null)
   [ "${#WORKFLOW_FILES[@]}" -gt 0 ] \
     || { echo 'ERROR: no workflow files found (.yaml or .yml)'; exit 1; }

   DIRECT_ACTIONS=(workflow-setup handle-fix-commit complete-pr-comment setup-build-env
                   configure-cmake build-cmake generate-build-matrix
                   post-clang-tidy-results collect-format-results run-change-detection)

   for name in "${DIRECT_ACTIONS[@]}"; do
     SHA=$(cat "${WORK_DIR}/${name}.sha")
     sed_inplace "s|Framework-R-D/phlex/.github/actions/${name}@main|Framework-R-D/action-${name}@${SHA} # v1|g" \
       "${WORKFLOW_FILES[@]}"
   done
   ```

   Verify no original references remain for any directly-referenced action
   (catches a mis-spelled name in the sed pattern):

   ```bash
   for name in "${DIRECT_ACTIONS[@]}"; do
     if grep -r "Framework-R-D/phlex/.github/actions/${name}" "${WORKFLOW_FILES[@]}" >/dev/null; then
       echo "ERROR: unreplaced reference for ${name}"; exit 1
     fi
   done
   if grep -r 'Framework-R-D/phlex/.github/actions' "${WORKFLOW_FILES[@]}" >/dev/null; then
     echo 'ERROR: residual internal action references in workflows'; exit 1
   fi
   echo 'OK: all internal references replaced'
   ```

   Sanity-check that the 5 transitively-used actions had no direct workflow
   references either before or after:

   ```bash
   for name in detect-act-env get-pr-info detect-relevant-changes \
               prepare-check-outputs prepare-fix-outputs; do
     if grep -r "Framework-R-D/phlex/.github/actions/${name}" .github/workflows/ >/dev/null; then
       echo "UNEXPECTED: ${name} found in workflows -- inventory may be wrong"; exit 1
     fi
   done
   ```

4. Remove the bundled actions directory (removes 15 action subdirectories plus
   `.github/actions/README.md`; idempotent):

   ```bash
   git rm -r --ignore-unmatch .github/actions/ \
     || echo 'INFO: .github/actions already removed'
   ```

5. Verify the Phase 1 deliverable is still committed on this branch:

   ```bash
   git ls-files --error-unmatch docs/dev/export-actions-plan.md >/dev/null \
     || { echo 'ERROR: export-actions-plan.md not on this branch -- complete Phase 1 first'; exit 1; }
   ```

6. Commit (stage the workflow edits from step 3 explicitly; `git rm` already
   staged the `.github/actions/` deletions from step 4):

   ```bash
   git add .github/workflows/
   git commit --no-verify -m 'chore: use standalone action repositories (supersedes #614)'
   ```

7. Push (the phlex working copy has no `origin` remote; push to `upstream`
   which points at `Framework-R-D/phlex`):

   ```bash
   git push -u upstream maintenance/export-reusable-actions --no-verify
   ```

8. Open the PR (record the PR number for Phase 6):

   ```bash
   gh pr create --base main \
     --title 'chore: use standalone action repositories' \
     --body 'Migrates internal composite actions to standalone Framework-R-D/action-* repos. Supersedes #614. Links: see docs/dev/export-actions-plan.md.'
   ```

---

## Phase 6 -- Update PR Placeholders *(depends on Phase 5; PR number now known)*

Phase 6 substitutes the actual phlex PR number for the `<PR>` placeholder in
each of the 15 new repos' `CHANGELOG.md` and v1 release notes.

**Phase 6 is mandatory, not optional.** The CHANGELOG and release notes on all
15 repos contain a broken `<PR>` placeholder link until Phase 6 completes. The
migration is not done until Phase 6 succeeds for all 15 repos.

**Pre-condition:** the phlex PR opened in Phase 5 step 8 is open and has its
final number. If that PR is later closed and replaced, re-run Phase 6 with the
new `PR_NUMBER`; the per-repo `<PR>` guard will skip repos that have already
been updated to the *old* PR number, so for those you must edit
`CHANGELOG.md` manually before re-running.

```bash
#!/usr/bin/env bash
set -euo pipefail
# Replace 999 with the phlex PR number recorded in Phase 5 step 8 before running.
PR_NUMBER="${PR_NUMBER:-999}"
[ "${PR_NUMBER}" != "999" ] \
  || { echo 'ERROR: set PR_NUMBER to the actual phlex PR number before running Phase 6'; exit 1; }
ORG="Framework-R-D"
WORK_DIR="${TMPDIR:-/tmp}/action-work"

sed_inplace() { if [[ "$(uname)" == Darwin ]]; then sed -i '' "$@"; else sed -i "$@"; fi; }
export -f sed_inplace

# Ensure gh credential helper is active for HTTPS clones
gh auth setup-git

mkdir -p "${WORK_DIR}/phase6-results"

ALL_ACTIONS=(
  detect-act-env detect-relevant-changes get-pr-info setup-build-env
  configure-cmake build-cmake collect-format-results complete-pr-comment
  generate-build-matrix post-clang-tidy-results handle-fix-commit
  prepare-check-outputs prepare-fix-outputs run-change-detection
  workflow-setup
)

for ACTION_NAME in "${ALL_ACTIONS[@]}"; do
  REPO_NAME="action-${ACTION_NAME}"

  (
    # Idempotency guard: 'phase6-results file present'
    if [ -f "${WORK_DIR}/phase6-results/${REPO_NAME}" ]; then
      echo "Skipping ${REPO_NAME}: already completed in a prior run."
      exit 0
    fi

    # Re-clone if the working dir is gone or dirty. A dirty working dir typically
    # means a Phase 2-4 run was interrupted; starting fresh is safest.
    if [ ! -d "${WORK_DIR}/${ACTION_NAME}" ] \
      || [ -n "$(git -C "${WORK_DIR}/${ACTION_NAME}" status --porcelain 2>/dev/null)" ]; then
      rm -rf "${WORK_DIR:?}/${ACTION_NAME}"
      git clone "https://github.com/${ORG}/${REPO_NAME}.git" \
        "${WORK_DIR}/${ACTION_NAME}" \
        || { echo "ERROR: clone failed for ${REPO_NAME}"; exit 1; }
    fi
    cd "${WORK_DIR}/${ACTION_NAME}" \
      || { echo "ERROR: cannot cd into ${WORK_DIR}/${ACTION_NAME}"; exit 1; }

    git checkout main && git pull --ff-only origin main \
      || { echo "ERROR: cannot fast-forward ${REPO_NAME} main -- inspect manually (likely cause: dependabot commit pushed after Phase 2-4)"; exit 1; }

    # Idempotency guard: 'CHANGELOG placeholder replaced'
    if grep -q '<PR>' CHANGELOG.md; then
      sed_inplace "s|<PR>|${PR_NUMBER}|g" CHANGELOG.md
      git add CHANGELOG.md
      git commit --no-verify -m "Update migration PR reference to #${PR_NUMBER}"
      git push --no-verify origin main
    else
      echo "INFO: <PR> already replaced in ${REPO_NAME} CHANGELOG -- skipping commit"
    fi

    # Regenerate v1 release notes with the resolved PR number. Use --notes-file
    # for reliable multi-line handling.
    NOTES_TMP=$(mktemp)
    cat > "${NOTES_TMP}" << EOF
## What's Changed

- Initial public release of this action, previously bundled as an internal composite action
  in [Framework-R-D/phlex](https://github.com/Framework-R-D/phlex).
- Source history preserved from the phlex monorepo via \`git filter-repo\`.
- No functional changes from the phlex-internal version.

See [Framework-R-D/phlex#${PR_NUMBER}](https://github.com/Framework-R-D/phlex/pull/${PR_NUMBER}) for the migration pull request.

**Full Changelog**:
https://github.com/Framework-R-D/phlex/commits/main/.github/actions/${ACTION_NAME}
EOF

    # gh release edit may fail if the release was manually deleted after Phase 2-4.
    # Fall back to gh release create, but only after verifying the v1 tag still exists.
    if ! gh release edit v1 \
        --repo "${ORG}/${REPO_NAME}" \
        --notes-file "${NOTES_TMP}"; then
      git ls-remote --tags "https://github.com/${ORG}/${REPO_NAME}.git" v1 \
        | grep -q 'refs/tags/v1' \
        || { rm -f "${NOTES_TMP}"; echo "ERROR: v1 tag missing from ${REPO_NAME} -- re-run Phase 2/3/4 export"; exit 1; }
      if ! gh release view v1 --repo "${ORG}/${REPO_NAME}" &>/dev/null; then
        gh release create v1 \
          --repo "${ORG}/${REPO_NAME}" \
          --title 'v1 - Initial Release' \
          --notes-file "${NOTES_TMP}"
      else
        rm -f "${NOTES_TMP}"
        echo "ERROR: gh release edit failed for ${REPO_NAME} but release exists -- check auth and retry"
        exit 1
      fi
    fi
    rm -f "${NOTES_TMP}"

    echo "Updated ${REPO_NAME}"
    # Record success; subshells cannot modify parent arrays, so use a marker file.
    : > "${WORK_DIR}/phase6-results/${REPO_NAME}"
  ) || echo "WARN: subshell for ${ACTION_NAME} exited non-zero -- will be reported as failed below"
done

# Collect failures.
PHASE6_FAILED=()
for ACTION_NAME in "${ALL_ACTIONS[@]}"; do
  REPO_NAME="action-${ACTION_NAME}"
  [ -f "${WORK_DIR}/phase6-results/${REPO_NAME}" ] \
    || PHASE6_FAILED+=("${REPO_NAME}")
done
if [ "${#PHASE6_FAILED[@]}" -gt 0 ]; then
  echo "FAILED: ${PHASE6_FAILED[*]}"
  echo "Re-running this script is safe: completed repos are skipped via the"
  echo "phase6-results marker files in ${WORK_DIR}/phase6-results/."
  exit 1
fi
echo 'All 15 repos updated successfully.'
```

---

## Verification

Run after Phase 6 completes:

1. `gh repo list Framework-R-D --public --json name --jq '.[].name'` lists at
   least 15 names matching `action-*`.
2. For each `action-NAME`: `gh release view v1 --repo Framework-R-D/action-NAME`
   succeeds (release exists, body contains the actual PR number rather than
   `<PR>`).
3. `grep -r 'Framework-R-D/phlex/.github/actions' .github/` in the phlex working
   copy returns zero matches.
4. `actionlint .github/workflows/*.yaml` reports zero errors.
5. For each Level 1/2 new repo: `grep 'phlex/.github/actions' action.yaml`
   returns zero matches (internal deps fully repinned).
6. For each new repo: `git log --oneline | wc -l` > 1 (history preserved, not a
   single initial commit).

---

## Decisions

- **Naming:** all repos use `Framework-R-D/action-NAME` (GitHub has no sub-org
  nesting).
- **Tags:** plain integers `v1`, `v2`, ...; first release is always `v1`.
- **SHA pinning:** all inter-action references use commit SHA + `# v1`
  annotation. Pre-existing external pins are left unchanged, with one
  exception: `actions/github-script@v8.0.0` in `post-clang-tidy-results` is
  resolved to a commit SHA by the export script
  (MANDATORY CHECK 6d). `actions/download-artifact@v8.0.1` and
  `platisd/clang-tidy-pr-comments@v1.8.0` in the same action intentionally
  retain version-tag pins.
- **Commits and pushes on new repos:** all `--no-verify`; direct push to `main`.
- **Branch protection / immutable tags:** none configured by this plan; the
  operator can enable them manually post-migration if desired.
- **phlex PR:** supersedes #614.

---

## Action Inventory (Quick Reference)

### Level 0 -- No Internal Dependencies

#### `detect-act-env` -> `action-detect-act-env`

- **Purpose**: Detects if the workflow is running under `act` (local GitHub Actions testing tool).
- **Inputs**: none
- **Outputs**: `is_act` (true/false)
- **Files**: `action.yaml`
- **External deps**: none

#### `detect-relevant-changes` -> `action-detect-relevant-changes`

- **Purpose**: Detects changed files between two git refs matching configurable file-type and glob filters.
- **Inputs**: `repo-path`, `base-ref`, `head-ref`, `file-type`, `include-globs`, `exclude-globs`, `type-pattern-add`
- **Outputs**: `matched` (bool), `matched_files` (newline-delimited list)
- **Files**: `action.yaml`
- **External deps**: none

#### `get-pr-info` -> `action-get-pr-info`

- **Purpose**: Retrieves PR information (ref, SHA, repo, base SHA) when triggered by an `issue_comment` event.
- **Inputs**: none
- **Outputs**: `ref`, `sha`, `repo`, `base_sha`
- **Files**: `action.yaml`
- **External deps**: `actions/github-script@ed597411d8f924073f98dfc5c65a23a2325f34cd` (v8.0.0)

#### `setup-build-env` -> `action-setup-build-env`

- **Purpose**: Sets up source and build directories for the Phlex build environment.
- **Inputs**: `source-path` (default: `phlex-src`), `build-path` (default: `phlex-build`)
- **Outputs**: `source-dir`, `build-dir` (absolute paths)
- **Files**: `action.yaml`
- **External deps**: none

#### `configure-cmake` -> `action-configure-cmake`

- **Purpose**: Configures CMake with preset detection and customizable options. **Requires the `phlex-ci` container** (sources `/entrypoint.sh` to activate the Spack environment).
- **Inputs**: `preset`, `source-path`, `build-path`, `build-type`, `extra-options`, `enable-form`, `form-root-storage`, `generator`, `cpp-compiler`
- **Outputs**: none
- **Files**: `action.yaml`
- **External deps**: none (container-specific)

#### `build-cmake` -> `action-build-cmake`

- **Purpose**: Builds the project using CMake. **Requires the `phlex-ci` container**.
- **Inputs**: `source-path`, `build-path`, `target`, `parallel-jobs`
- **Outputs**: none
- **Files**: `action.yaml`
- **External deps**: none (container-specific)

#### `collect-format-results` -> `action-collect-format-results`

- **Purpose**: Aggregates results from multiple formatting jobs and produces a summary message.
- **Inputs**: `results-json` (JSON object)
- **Outputs**: `message`, `has_failures`
- **Files**: `action.yaml`
- **External deps**: none (inline Python)

#### `complete-pr-comment` -> `action-complete-pr-comment`

- **Purpose**: Removes the 'eyes' reaction and adds a completion reaction ('rocket' or 'confused') to the triggering PR comment.
- **Inputs**: `status` (success/failure/cancelled/skipped)
- **Outputs**: none
- **Files**: `action.yaml`
- **External deps**: `actions/github-script@ed597411d8f924073f98dfc5c65a23a2325f34cd` (v8.0.0)

#### `generate-build-matrix` -> `action-generate-build-matrix`

- **Purpose**: Generates a dynamic JSON build matrix (compiler x sanitizer combinations) for CMake build workflows.
- **Inputs**: `user-input`, `comment-body`
- **Outputs**: `matrix` (JSON)
- **Files**: `action.yaml`, `generate_matrix.py`
- **External deps**: none (inline Python script)
- **README_INPUTS / README_OUTPUTS**: set from the `action.yaml` fields only
  (`user-input`, `comment-body` for inputs; `matrix` for outputs). The
  `generate_matrix.py` CLI is excluded from the README.
- **Post-creation issue** *(manual; not automated in the script)*: after the
  repo is created, file an issue against `action-generate-build-matrix`
  requesting a `python-check.yaml` workflow (ruff + mypy, matching phlex's
  `python-check.yaml`) and unit tests for `generate_matrix.py`. In phlex,
  `generate_matrix.py` is covered by ruff and mypy repo-wide but has no unit
  tests; neither coverage transfers to the standalone repo automatically.

#### `post-clang-tidy-results` -> `action-post-clang-tidy-results`

- **Purpose**: Downloads clang-tidy YAML artifacts, posts inline PR comments, and posts a summary comment.
- **Inputs**: `build-path`, `pr-number`, `post-summary`
- **Outputs**: `has_content`
- **Files**: `action.yaml`
- **External deps**: `actions/download-artifact@v8.0.1`, `platisd/clang-tidy-pr-comments@v1.8.0`, `actions/github-script@v8.0.0`
- **Note**: `actions/github-script@v8.0.0` is an unpinned version tag. The
  export script resolves it to a commit SHA via MANDATORY CHECK 6d before the
  first commit. The other two external pins are left as-is.

#### `handle-fix-commit` -> `action-handle-fix-commit`

- **Purpose**: Commits and pushes automatic fix changes; falls back to a patch artifact if the repo is a fork without maintainer write access.
- **Inputs**: `tool`, `working-directory`, `token`, `pr-info-ref`, `pr-info-repo`, `retry-attempts`, `skip-comment`
- **Outputs**: `changes`, `pushed`, `commit_sha`, `commit_sha_short`, `patch_name`
- **Files**: `action.yaml`
- **External deps**:
  - `actions/github-script@ed597411d8f924073f98dfc5c65a23a2325f34cd` (v8.0.0)
  - `thollander/actions-comment-pull-request@24bffb9b452ba05a4f3f77933840a6a841d1b32b` (v3.0.1)
  - `actions/upload-artifact@043fb46d1a93c77aae656e7c1c64a875d1fc6a0a` (v7.0.1)

### Level 1 -- Depend on Level 0

#### `prepare-check-outputs` -> `action-prepare-check-outputs`

- **Purpose**: Determines environment, target refs, and build paths for check workflows across `pull_request`, `issue_comment`, and `workflow_dispatch` triggers.
- **Inputs**: `ref`, `repo`, `pr-base-sha`, `checkout-path`, `build-path`
- **Outputs**: `is_act`, `ref`, `repo`, `base_sha`, `pr_number`, `checkout_path`, `build_path`
- **Internal deps to repin**: `detect-act-env`, `get-pr-info`

#### `prepare-fix-outputs` -> `action-prepare-fix-outputs`

- **Purpose**: Determines target ref and repository for fix workflows.
- **Inputs**: `ref`, `repo`, `checkout-path`
- **Outputs**: `ref`, `repo`, `checkout_path`
- **Internal deps to repin**: `get-pr-info`

#### `run-change-detection` -> `action-run-change-detection`

- **Purpose**: Encapsulates checkout, relevant-change detection, and result reporting.
- **Inputs**: `checkout-path`, `ref`, `repo`, `base-ref`, `head-ref`, `file-type`, `include-globs`, `exclude-globs`
- **Outputs**: `has_changes`, `matched_files`
- **Internal deps to repin**: `detect-relevant-changes`
- **External deps**: `actions/checkout@de0fac2e4500dabe0009e67214ff5f5447ce83dd` (v6.0.2)

### Level 2 -- Depends on Level 1

#### `workflow-setup` -> `action-workflow-setup`

- **Purpose**: Master setup action; combines ref resolution, environment detection, and change detection in a single step. Used by the majority of phlex workflows.
- **Inputs**: `mode`, `ref`, `repo`, `pr-base-sha`, `checkout-path`, `build-path`, `file-type`, `include-globs`, `exclude-globs`, `head-ref`
- **Outputs**: `is_act`, `ref`, `repo`, `base_sha`, `pr_number`, `checkout_path`, `build_path`, `has_changes`
- **Internal deps to repin**: `prepare-check-outputs`, `prepare-fix-outputs`, `run-change-detection`

---

## Shell Script Skeleton

The implementation guide produced in Phase 1 contains this script verbatim. It
is parameterized per action and ready to run in the devcontainer.

```bash
#!/usr/bin/env bash
# Export a single phlex composite action to its own GitHub repository.
#
# Usage:
#   export-action.sh <action-name> <description> [<dep-name>=<dep-sha> ...]
#
# Required environment variables (set before invoking):
#   README_INPUTS    Markdown table rows for the ## Inputs section
#                    (one row per input, populated from action.yaml).
#   README_OUTPUTS   Markdown table rows for the ## Outputs section.
#
# Optional environment variables:
#   DEP_REF_SUFFIX        Suffix used in the phlex `uses:` references that this
#                         action's action.yaml resolves to. Defaults to "@main".
#                         Override (e.g. DEP_REF_SUFFIX=@some-branch) only if
#                         your phlex history pins deps with a different suffix.
#   ALLOW_SINGLE_COMMIT   Set to 1 to skip the single-commit safety check
#                         (used when a brand-new action genuinely has only one
#                         commit in phlex history).
#
# Exit code 0 on success; non-zero on any MANDATORY CHECK failure. Each
# MANDATORY CHECK is labelled (6a, 6b, 6c, 6d, 14a). On failure, consult the
# Appendix: Script-internal reference entry for the named label.
set -euo pipefail

command -v git-filter-repo &>/dev/null \
  || { echo 'ERROR: git-filter-repo not found -- install: pip install git-filter-repo (ensure ~/.local/bin is on PATH)'; exit 1; }

ACTION_NAME="${1:?action name required}"
ACTION_DESCRIPTION="${2:?action description required}"
shift 2  # remaining positional args are dep KEY=SHA pairs
: "${README_INPUTS:?set README_INPUTS to the markdown table rows for the Inputs section}"
: "${README_OUTPUTS:?set README_OUTPUTS to the markdown table rows for the Outputs section}"
DEP_REF_SUFFIX="${DEP_REF_SUFFIX:-@main}"

PHLEX_REPO="/workspaces/phlex"
WORK_DIR="${TMPDIR:-/tmp}/action-work"
ORG="Framework-R-D"
REPO_NAME="action-${ACTION_NAME}"
TODAY=$(date +%Y-%m-%d)
PR_NUMBER="<PR>"  # Replaced in Phase 6 once the phlex PR number is known.

# sed -i portability shim. Exported so subshells inherit it.
sed_inplace() { if [[ "$(uname)" == Darwin ]]; then sed -i '' "$@"; else sed -i "$@"; fi; }
export -f sed_inplace

# Resolve commit SHAs for the actionlint workflow's pinned actions
# (dereferences annotated tags to their underlying commits).
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
CHECKOUT_SHA=$(resolve_tag_sha actions/checkout v4)
ACTIONLINT_SHA=$(resolve_tag_sha raven-actions/actionlint v2)

mkdir -p "${WORK_DIR}"

# ---------------------------------------------------------------------------
# 1. Extract phlex history for this action.
# ---------------------------------------------------------------------------

# Refuse to run against a shallow phlex clone (would truncate history).
if git -C "${PHLEX_REPO}" rev-parse --is-shallow-repository | grep -q true; then
  echo "ERROR: phlex repo is shallow -- run: git -C \"${PHLEX_REPO}\" fetch --unshallow"
  exit 1
fi

# Refuse to run if the action source directory has any uncommitted state
# (tracked diffs or untracked files). filter-repo only captures committed history.
DIRTY=$(git -C "${PHLEX_REPO}" status --porcelain -- ".github/actions/${ACTION_NAME}")
[ -z "${DIRTY}" ] \
  || { echo "ERROR: uncommitted or untracked changes in ${ACTION_NAME} source:"; echo "${DIRTY}"; \
       echo 'Commit, stash, or clean before proceeding.'; exit 1; }

# For generate-build-matrix, verify generate_matrix.py exists in phlex history
# before filter-repo. This catches a renamed/moved file early.
# Note: this pre-check is intentionally omitted here. When the 11 Level 0
# actions run in parallel, concurrent `git clone --no-local` calls hold
# object-database locks on PHLEX_REPO, which can cause `git log` to return
# incomplete output and produce a false-negative. MANDATORY CHECK 6a (below)
# already verifies that generate_matrix.py is present after filter-repo, which
# is the authoritative check and runs against the isolated clone.

# --no-local forces a full object copy. A bare local-path clone could otherwise
# be shallow if the source were shallow (we already enforce non-shallow above).
git clone --no-local "${PHLEX_REPO}" "${WORK_DIR}/${ACTION_NAME}"
cd "${WORK_DIR}/${ACTION_NAME}"
# git-filter-repo parses `git config --list` by splitting each line on `=`.
# Git config values that contain embedded newlines (e.g. shell-function aliases)
# produce continuation lines with no `=`, causing a ValueError in filter-repo's
# Python. Work around this by running filter-repo under a minimal stub HOME that
# contains no multi-line config entries.
FILTER_REPO_HOME="${TMPDIR:-/tmp}/filter-repo-home"
mkdir -p "${FILTER_REPO_HOME}"
if [ ! -f "${FILTER_REPO_HOME}/.gitconfig" ]; then
  printf '[user]\n    name = Export Action\n    email = export@example.com\n[core]\n    sshCommand = ssh\n' \
    > "${FILTER_REPO_HOME}/.gitconfig"
fi
HOME="${FILTER_REPO_HOME}" git filter-repo --subdirectory-filter ".github/actions/${ACTION_NAME}" --force

# Sanity-check commit count after filter-repo.
COMMIT_COUNT=$(git log --oneline | wc -l)
if [ "${COMMIT_COUNT}" -lt 1 ]; then
  echo "ERROR: filter-repo yielded 0 commits -- path '.github/actions/${ACTION_NAME}' does not exist in phlex history"
  echo "  Check: git -C \"${PHLEX_REPO}\" log --oneline -- .github/actions/${ACTION_NAME}"
  echo "  If the path was renamed, find the original with:"
  echo "    git -C \"${PHLEX_REPO}\" log --diff-filter=R --name-status --all -- .github/actions/${ACTION_NAME}"
  echo "  Then re-run filter-repo with --path-rename OLD:NEW in addition to --subdirectory-filter."
  exit 1
fi
if [ "${ALLOW_SINGLE_COMMIT:-0}" != "1" ] && [ "${COMMIT_COUNT}" -le 1 ]; then
  echo "ERROR: filter-repo yielded only ${COMMIT_COUNT} commit(s) for ${ACTION_NAME}."
  echo "  If the action genuinely has only one commit, re-run with: ALLOW_SINGLE_COMMIT=1 $0 $*"
  exit 1
fi

# ---------------------------------------------------------------------------
# MANDATORY CHECK 6a -- action.yaml present at root, no unexpected nesting.
# ---------------------------------------------------------------------------
test -f action.yaml \
  || { echo "ERROR [6a]: action.yaml missing after filter-repo"; exit 1; }
[ -s action.yaml ] \
  || { echo "ERROR [6a]: action.yaml is empty after filter-repo"; exit 1; }
test ! -d .github/actions \
  || { echo "ERROR [6a]: unexpected .github/actions subdirectory after filter-repo -- check action path in phlex history"; exit 1; }
if [ "${ACTION_NAME}" = "generate-build-matrix" ]; then
  if [ ! -f generate_matrix.py ]; then
    echo "ERROR [6a]: generate_matrix.py missing after filter-repo."
    echo "  Find the historical relative path inside this filtered clone:"
    echo "    git log --name-only --all -- '*generate_matrix.py'"
    echo "  Then re-run with --path-rename OLD_PATH:generate_matrix.py."
    exit 1
  fi
fi

# ---------------------------------------------------------------------------
# MANDATORY CHECK 6b -- Level 0 only: no internal phlex refs remain.
# Detected by the absence of dep KEY=SHA positional arguments ("$#" -eq 0).
# ---------------------------------------------------------------------------
if [ "$#" -eq 0 ]; then
  PHLEX_REFS=$(grep -c 'Framework-R-D/phlex' action.yaml 2>/dev/null || true)
  [ "${PHLEX_REFS}" -eq 0 ] \
    || { echo "ERROR [6b]: Level 0 action has ${PHLEX_REFS} phlex ref(s) in action.yaml -- mis-classified?"; exit 1; }
fi

# ---------------------------------------------------------------------------
# MANDATORY CHECK 6d -- post-clang-tidy-results only: resolve unpinned
# actions/github-script@v8.0.0 to a commit SHA before any commit.
# ---------------------------------------------------------------------------
if [ "${ACTION_NAME}" = "post-clang-tidy-results" ]; then
  GS_SHA=$(resolve_tag_sha actions/github-script v8.0.0)
  sed_inplace "s|actions/github-script@v8.0.0|actions/github-script@${GS_SHA} # v8.0.0|g" action.yaml
  if grep -q 'actions/github-script@v8' action.yaml; then
    echo "ERROR [6d]: unpinned github-script reference remains in action.yaml"; exit 1
  fi
fi

# ---------------------------------------------------------------------------
# MANDATORY CHECK 6c -- all actions except post-clang-tidy-results: no
# unpinned version-tag references in action.yaml.
# post-clang-tidy-results legitimately retains @v refs for actions/download-artifact
# and platisd/clang-tidy-pr-comments.
# ---------------------------------------------------------------------------
if [ "${ACTION_NAME}" != "post-clang-tidy-results" ]; then
  if grep -n 'uses:.*@v[0-9]' action.yaml; then
    echo "ERROR [6c]: unpinned version-tag reference(s) in action.yaml -- pin to commit SHA"
    exit 1
  fi
fi

# ---------------------------------------------------------------------------
# MANDATORY CHECK 14a -- Level 1/2 only: verify dep KEYs and reference format.
# ---------------------------------------------------------------------------
for kv in "$@"; do
  dep_name="${kv%%=*}"
  if [[ "${dep_name}" == action-* ]]; then
    echo "ERROR [14a]: dep KEY '${dep_name}' must be the bare source name (e.g. detect-act-env), not 'action-detect-act-env'"
    exit 1
  fi
  # Confirm the reference uses ${DEP_REF_SUFFIX} (default @main). Anything else
  # is an unexpected suffix that the section-2 sed will not match.
  ISSUE=$(grep "Framework-R-D/phlex/.github/actions/${dep_name}" action.yaml \
    | grep -v '@[0-9a-f]\{40\}' | grep -v "${DEP_REF_SUFFIX}$" \
    | grep -v "${DEP_REF_SUFFIX}\s" || true)
  if [ -n "${ISSUE}" ]; then
    echo "ERROR [14a]: dep '${dep_name}' uses unexpected suffix (expected '${DEP_REF_SUFFIX}'):"
    echo "${ISSUE}"
    echo "Override with DEP_REF_SUFFIX=<actual-suffix> and re-run."
    exit 1
  fi
done

# ---------------------------------------------------------------------------
# 2. Repin internal deps. Pairs come in as KEY=SHA positional args.
# ---------------------------------------------------------------------------
for kv in "$@"; do
  dep_name="${kv%%=*}"
  dep_sha="${kv#*=}"
  grep -q "Framework-R-D/phlex/.github/actions/${dep_name}" action.yaml \
    || { echo "ERROR: dep pattern for '${dep_name}' not found in action.yaml"; exit 1; }
  sed_inplace \
    "s|Framework-R-D/phlex/.github/actions/${dep_name}${DEP_REF_SUFFIX}|Framework-R-D/action-${dep_name}@${dep_sha} # v1|g" \
    action.yaml
  RESIDUAL=$(grep -c "Framework-R-D/phlex/.github/actions/${dep_name}" action.yaml || true)
  [ "${RESIDUAL}" -eq 0 ] \
    || { echo "ERROR: ${RESIDUAL} unreplaced reference(s) for '${dep_name}' remain"; exit 1; }
done

# ---------------------------------------------------------------------------
# 3. Add supporting files (LICENSE, README, CHANGELOG, dependabot, actionlint).
# ---------------------------------------------------------------------------
cp "${PHLEX_REPO}/LICENSE" .

cat > README.md << EOF
# \`${REPO_NAME}\`

> ${ACTION_DESCRIPTION}

## Usage

\`\`\`yaml
- uses: ${ORG}/${REPO_NAME}@v1  # pin to commit SHA in production
  with:
    input-name: value
\`\`\`

## Inputs

| Name | Description | Required | Default |
|------|-------------|----------|---------|
${README_INPUTS}

## Outputs

| Name | Description |
|------|-------------|
${README_OUTPUTS}

## License

[Apache 2.0](LICENSE)
EOF

# Append container-requirement note for the two CMake actions.
if [ "${ACTION_NAME}" = "configure-cmake" ] || [ "${ACTION_NAME}" = "build-cmake" ]; then
  cat >> README.md << 'NOTES'

## Notes

> [!NOTE]
> This action requires the `ghcr.io/framework-r-d/phlex-ci` container image
> (sources `/entrypoint.sh` to activate the Spack environment).
> It is not a general-purpose action.
NOTES
fi

cat > CHANGELOG.md << EOF
# Changelog

## v1 --- ${TODAY}

### Initial Release

This action was previously bundled as an internal composite action within
[Framework-R-D/phlex](https://github.com/Framework-R-D/phlex). It has been
extracted into its own repository to allow reuse across other projects.

The full commit history from the phlex monorepo has been preserved.

See [Framework-R-D/phlex#${PR_NUMBER}](https://github.com/Framework-R-D/phlex/pull/${PR_NUMBER}) for the migration pull request.
EOF

mkdir -p .github/workflows
cat > .github/dependabot.yml << 'EOF'
version: 2
updates:
  - package-ecosystem: github-actions
    directory: /
    schedule:
      interval: weekly
EOF

cat > .github/workflows/actionlint.yaml << EOF
name: actionlint

on:
  push:
    branches: [main]
  pull_request:
    branches: [main]

jobs:
  actionlint:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@9c091bb95f68b377484df746e50259b3f3e1b745 # v7.0.0
      - uses: raven-actions/actionlint@${ACTIONLINT_SHA} # v2
EOF

# Release notes (Phase 6 rewrites them once the phlex PR number is known).
cat > "/tmp/release-notes-${ACTION_NAME}.md" << EOF
## What's Changed

- Initial public release of this action, previously bundled as an internal composite action
  in [Framework-R-D/phlex](https://github.com/Framework-R-D/phlex).
- Source history preserved from the phlex monorepo via \`git filter-repo\`.
- No functional changes from the phlex-internal version.

See [Framework-R-D/phlex#${PR_NUMBER}](https://github.com/Framework-R-D/phlex/pull/${PR_NUMBER}) for the migration pull request.

**Full Changelog**:
https://github.com/Framework-R-D/phlex/commits/main/.github/actions/${ACTION_NAME}
EOF

git add .
git commit --no-verify -m "Add license, README, changelog, and CI"
# First of two pushes to main during a clean Phase 2/3/4 run.

# ---------------------------------------------------------------------------
# 4. Create remote repo (idempotent). Repo creation happens AFTER the local
#    commit so the first push delivers a complete history.
# ---------------------------------------------------------------------------
# Idempotency guard: 'Remote repo exists'
if gh repo view "${ORG}/${REPO_NAME}" &>/dev/null; then
  echo "Repo ${ORG}/${REPO_NAME} already exists -- skipping create."
  # If the remote exists but has no commits, the subsequent push will fail with
  # 'refusing to push to an unborn branch'. In that case, delete and re-run:
  #   gh repo delete ${ORG}/${REPO_NAME} --yes
else
  gh repo create "${ORG}/${REPO_NAME}" \
    --public --description "${ACTION_DESCRIPTION}"
    # Note: --no-readme is not a valid flag in gh ≥ 2.x; no README is created
    # by default (opt-in is --add-readme).
fi

# filter-repo leaves origin pointing at phlex; reset it to the new repo.
git remote remove origin 2>/dev/null || true
git remote add origin "https://github.com/${ORG}/${REPO_NAME}.git"

# Push history + supporting-files commit (push #1).
# A 'non-fast-forward' failure here means a previous filter-repo run already
# pushed different commits to this remote. Only force-push if the remote has no
# external consumers and no v1 tag has been created.
git push --no-verify -u origin main

# Ensure default branch is 'main' (gh repo create may default to 'master' under
# certain org settings; this is a no-op when already correct).
gh api "repos/${ORG}/${REPO_NAME}" -X PATCH -f default_branch=main >/dev/null

# ---------------------------------------------------------------------------
# 5. Create annotated tag, record SHA, push tag, create release.
# ---------------------------------------------------------------------------
# Idempotency guard: 'SHA file present' (handles three states:
#   (a) fresh run: tag and SHA file both absent;
#   (b) re-run after TMPDIR cleared: remote tag exists but SHA file absent;
#   (c) full re-run: SHA file already present).
if [ ! -f "${WORK_DIR}/${ACTION_NAME}.sha" ]; then
  if git ls-remote --tags origin v1 | grep -q 'refs/tags/v1$'; then
    # State (b): recover SHA from the remote tag.
    RECOVERED=$(resolve_tag_sha "${ORG}/${REPO_NAME}" v1)
    echo "${RECOVERED}" > "${WORK_DIR}/${ACTION_NAME}.sha"
  else
    # State (a): create the tag locally.
    git tag v1 -m "v1 - Initial release"
    V1_SHA=$(git rev-parse v1^{commit})
    echo "${V1_SHA}" > "${WORK_DIR}/${ACTION_NAME}.sha"
    echo "V1_SHA for ${REPO_NAME}: ${V1_SHA}"
  fi
fi
# When running Phase 2/3/4 in parallel, stdout is interleaved -- always
# read the SHA from "${WORK_DIR}/${ACTION_NAME}.sha", never from stdout.

# Pin the README usage example to the recorded v1 commit SHA (push #2).
V1_SHA_PINNED=$(cat "${WORK_DIR}/${ACTION_NAME}.sha")
if grep -q '@v1  # pin to commit SHA in production' README.md; then
  sed_inplace "s|@v1  # pin to commit SHA in production|@${V1_SHA_PINNED} # v1|" README.md
  git add README.md
  git commit --no-verify -m "Pin README usage example to v1 commit SHA"
  git push --no-verify origin main
fi

# Idempotency guard: 'Tag already on remote'
if git ls-remote --tags origin v1 | grep -q 'refs/tags/v1$'; then
  echo "Tag v1 already on remote -- skipping tag push."
else
  git push --no-verify origin v1
fi

# Idempotency guard: 'Release already exists'
if gh release view v1 --repo "${ORG}/${REPO_NAME}" &>/dev/null; then
  echo "Release v1 already exists -- skipping release creation."
else
  gh release create v1 \
    --repo "${ORG}/${REPO_NAME}" \
    --title "v1 - Initial Release" \
    --notes-file "/tmp/release-notes-${ACTION_NAME}.md"
fi

# Manual post-export reminder for generate-build-matrix.
if [ "${ACTION_NAME}" = "generate-build-matrix" ]; then
  echo
  echo "REMINDER (manual): file a GitHub issue on ${ORG}/${REPO_NAME} requesting"
  echo "a python-check.yaml workflow (ruff + mypy) and unit tests for generate_matrix.py."
fi
```

---

## Appendix: Script-internal reference

Read this section only when the script aborts with an `ERROR [LABEL]` message
referencing one of the labels below.

### 6a -- action.yaml present at root after filter-repo

Triggered when `action.yaml` is missing, empty, or `git filter-repo` produced
an unexpected `.github/actions/` subdirectory at the root of the filtered repo.

Most common cause: the action directory was renamed in phlex history.

Recovery:

```bash
git log --name-only --follow -- action.yaml
```

inside the filtered clone reveals the historical path. Re-run filter-repo
adding `--path-rename OLD_PATH:NEW_PATH` so the file lands at the root, or fix
the `--subdirectory-filter` argument if a wrong directory was chosen.

For `generate-build-matrix`, `generate_matrix.py` must also be present at the
root. If it lived in a subdirectory (e.g. `scripts/generate_matrix.py`) of the
filtered action directory, re-run with
`--path-rename scripts/generate_matrix.py:generate_matrix.py`.

### 6b -- Level 0 action has internal phlex refs

Triggered only when no dep `KEY=SHA` arguments were passed (Level 0 invocation)
but the resulting `action.yaml` contains at least one
`Framework-R-D/phlex/...` reference.

Cause: the action was mis-classified -- it has internal deps and should be
Level 1 or Level 2. Re-check the dependency ordering at the top of this plan,
re-classify the action, and pass the required `KEY=SHA` arguments.

### 6c -- unpinned version-tag reference remains

Triggered when `action.yaml` (for any action other than `post-clang-tidy-results`)
contains a `uses: foo/bar@vN` style reference instead of a 40-char commit SHA.

Resolve manually using the same pattern as `resolve_tag_sha()` in the script:

```bash
gh api repos/OWNER/REPO/git/ref/tags/TAG --jq .object.sha
# If .object.type is "tag", dereference once more:
gh api repos/OWNER/REPO/git/tags/<sha> --jq .object.sha
```

Then `sed_inplace "s|OWNER/REPO@TAG|OWNER/REPO@${SHA} # TAG|g" action.yaml`.

### 6d -- post-clang-tidy-results github-script resolution failed

Triggered only for `post-clang-tidy-results`, when the script's automatic
resolution of `actions/github-script@v8.0.0` did not produce the expected
substitution.

Causes are typically transient (GitHub API rate-limit, network error). Verify
authentication with `gh auth status` and re-run.

### 14a -- Level 1/2 dep reference format unexpected

Triggered when a dep KEY uses the `action-` prefix instead of the bare source
name, or when an internal dep reference in `action.yaml` uses a suffix other
than `${DEP_REF_SUFFIX}` (default `@main`).

Recovery:

- If the KEY is wrong: re-invoke the script with bare source names
  (e.g. `detect-act-env=...` not `action-detect-act-env=...`).
- If the suffix is wrong: identify it with

  ```bash
  grep 'Framework-R-D/phlex/.github/actions' action.yaml
  ```

  then re-invoke with `DEP_REF_SUFFIX=@<actual-suffix> bash export-action.sh ...`.

---

## Relevant Files

- `/workspaces/phlex/.github/actions/*/action.yaml` -- source files to extract.
- `/workspaces/phlex/.github/actions/generate-build-matrix/generate_matrix.py`
  -- supporting file extracted alongside `action.yaml`.
- `/workspaces/phlex/.github/actions/README.md` -- removed by Phase 5 step 4.
- `/workspaces/phlex/.github/workflows/*.yaml` (25 files) -- updated by Phase 5.
- `/workspaces/phlex/LICENSE` -- copied into each new repo.
- `/workspaces/phlex/docs/dev/export-actions-plan.md` -- operator guide created
  in Phase 1.
