#!/usr/bin/env bash
set -euo pipefail

# Verify that the shared if-gating blocks contain the required phrases across
# GitHub Actions workflows. Exits 1 if any required phrase is missing.
#
# Two gating-block families are checked:
#
#   CHECK-JOB pattern — used by the lint/check job in every *-check workflow:
#     always() && (
#       github.event_name == 'workflow_dispatch' ||
#       inputs.skip-relevance-check ||
#       needs.setup.outputs.has_changes == 'true'
#     )
#
#   FIX-SETUP pattern — used by the setup job in every *-fix workflow that
#   exposes workflow_call (all fix workflows except clang-tidy-fix):
#     inputs.ref != '' ||
#     github.event_name == 'workflow_dispatch' ||
#     (github.event_name == 'issue_comment' &&
#      ... author_association gate ...)

WORKFLOWS_DIR="${1:-.github/workflows}"

# Check workflows whose lint/check job uses the relevance-check if-gate.
CHECK_WORKFLOWS=(
  "actionlint-check.yaml"
  "clang-format-check.yaml"
  "cmake-format-check.yaml"
  "header-guards-check.yaml"
  "jsonnet-format-check.yaml"
  "markdown-check.yaml"
  "python-check.yaml"
  "yaml-check.yaml"
)

# clang-tidy-check has an issue_comment-aware gating block on its setup job.
TIDY_CHECK_WORKFLOWS=(
  "clang-tidy-check.yaml"
)

# Fix workflows that expose workflow_call (use inputs.ref != '' as entry point).
# clang-tidy-fix is excluded: it has no workflow_call trigger.
FIX_WORKFLOWS=(
  "clang-format-fix.yaml"
  "cmake-format-fix.yaml"
  "header-guards-fix.yaml"
  "jsonnet-format-fix.yaml"
  "markdown-fix.yaml"
  "python-fix.yaml"
  "yaml-fix.yaml"
)

# cmake-build uses the full event-list gating block on its setup job.
BUILD_WORKFLOWS=(
  "cmake-build.yaml"
)

# Required phrases for the lint/check job in *-check workflows.
CHECK_JOB_PHRASES=(
  "always()"
  "github.event_name == 'workflow_dispatch'"
  "inputs.skip-relevance-check"
  "needs.setup.outputs.has_changes == 'true'"
)

# Required phrases for the clang-tidy-check setup job.
TIDY_CHECK_PHRASES=(
  "github.event_name == 'workflow_dispatch'"
  "github.event_name == 'pull_request'"
  "github.event_name == 'push'"
  "github.event_name == 'issue_comment'"
  "github.event.issue.pull_request"
  "fromJSON('[\"OWNER\", \"COLLABORATOR\", \"MEMBER\"]')"
  "github.event.comment.author_association"
)

# Required phrases for fix workflow setup jobs (workflow_call-capable).
FIX_PHRASES=(
  "inputs.ref != ''"
  "github.event_name == 'workflow_dispatch'"
  "github.event_name == 'issue_comment'"
  "github.event.issue.pull_request"
  "fromJSON('[\"OWNER\", \"COLLABORATOR\", \"MEMBER\"]')"
  "github.event.comment.author_association"
)

# Required phrases for cmake-build (full event-list gating on setup job).
BUILD_PHRASES=(
  "github.event_name == 'workflow_dispatch'"
  "github.event_name == 'pull_request'"
  "github.event_name == 'push'"
  "github.event_name == 'schedule'"
  "github.event_name == 'workflow_call'"
  "github.event_name == 'issue_comment'"
  "github.event.issue.pull_request"
  "fromJSON('[\"OWNER\", \"COLLABORATOR\", \"MEMBER\"]')"
  "github.event.comment.author_association"
)

pass=0
fail=0

check_workflow() {
  local file="$1"
  local label="$2"
  shift 2
  local phrases=("$@")
  local missing=()

  if [[ ! -f "$file" ]]; then
    echo "ERROR: $file not found"
    fail=$((fail + 1))
    return
  fi

  for phrase in "${phrases[@]}"; do
    if ! grep -qF "$phrase" "$file"; then
      missing+=("$phrase")
    fi
  done

  if [[ ${#missing[@]} -gt 0 ]]; then
    echo "FAIL: $label"
    for m in "${missing[@]}"; do
      echo "  missing: $m"
    done
    fail=$((fail + 1))
  else
    echo "PASS: $label"
    pass=$((pass + 1))
  fi
}

echo "Check workflows (relevance-check gate on lint job)"
echo "==================================================="
for wf in "${CHECK_WORKFLOWS[@]}"; do
  check_workflow "$WORKFLOWS_DIR/$wf" "$wf" "${CHECK_JOB_PHRASES[@]}"
done

echo ""
echo "Clang-tidy check (issue_comment-aware gate on setup job)"
echo "========================================================="
for wf in "${TIDY_CHECK_WORKFLOWS[@]}"; do
  check_workflow "$WORKFLOWS_DIR/$wf" "$wf" "${TIDY_CHECK_PHRASES[@]}"
done

echo ""
echo "Fix workflows (workflow_call-capable, inputs.ref gate)"
echo "======================================================="
for wf in "${FIX_WORKFLOWS[@]}"; do
  check_workflow "$WORKFLOWS_DIR/$wf" "$wf" "${FIX_PHRASES[@]}"
done

echo ""
echo "Build workflow (full event-list gate on setup job)"
echo "==================================================="
for wf in "${BUILD_WORKFLOWS[@]}"; do
  check_workflow "$WORKFLOWS_DIR/$wf" "$wf" "${BUILD_PHRASES[@]}"
done

echo ""
echo "========================================"
echo "Results: $pass passed, $fail failed"
echo "========================================"

if [[ $fail -gt 0 ]]; then
  exit 1
fi
