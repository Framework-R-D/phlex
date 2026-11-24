#!/usr/bin/env bash
set -euo pipefail

# Verify that the shared if-gating blocks contain the required phrases across
# GitHub Actions workflows. Exits 1 if any required phrase is missing.
#
# Relevance gates and workflow-specific event/authorization gates are checked.
#
#   CHECK-JOB pattern used by workflows in CHECK_WORKFLOWS:
#     always() && (
#       github.event_name == 'workflow_dispatch' ||
#       inputs.skip-relevance-check ||
#       needs.setup.outputs.has_changes == 'true'
#     )

WORKFLOWS_DIR="${1:-.github/workflows}"

# Check workflows whose lint/check job uses the relevance-check if-gate.
CHECK_WORKFLOWS=(
  "actionlint-check.yaml"
)

# clang-tidy-check has an issue_comment-aware gating block on its setup job.
TIDY_CHECK_WORKFLOWS=(
  "clang-tidy-check.yaml"
)

# Pre-commit fixes accept only authorized PR comment commands.
PRE_COMMIT_FIX_WORKFLOWS=(
  "pre-commit-fix.yaml"
)

# cmake-build uses the full event-list gating block on its setup job.
BUILD_WORKFLOWS=(
  "cmake-build.yaml"
)

# Coverage workflow uses its own gating logic.
COVERAGE_WORKFLOWS=(
  "coverage.yaml"
)

# Clang-tidy-fix workflow has its own gating logic (no workflow_call).
CLANG_TIDY_FIX_WORKFLOWS=(
  "clang-tidy-fix.yaml"
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

PRE_COMMIT_FIX_PHRASES=(
  "github.event_name == 'issue_comment'"
  "github.event.issue.pull_request"
  "contains(fromJSON('[\"OWNER\","
  "\"COLLABORATOR\", \"MEMBER\"]'), github.event.comment.author_association)"
  "startsWith(github.event.comment.body, format('@{0}bot pre-commit-fix', github.event.repository.name))"
  "startsWith(github.event.comment.body, format('@{0}bot format', github.event.repository.name))"
)

# Required phrases for coverage workflow setup job (custom gating).
COVERAGE_PHRASES=(
  "github.event_name == 'workflow_dispatch'"
  "github.event_name == 'pull_request'"
  "github.event_name == 'push'"
  "github.event_name == 'issue_comment'"
  "github.event.issue.pull_request"
  "fromJSON('[\"OWNER\", \"COLLABORATOR\", \"MEMBER\"]')"
  "github.event.comment.author_association"
  "startsWith(github.event.comment.body, format('@{0}bot coverage', github.event.repository.name))"
)

# Required phrases for clang-tidy-fix workflow setup job (custom gating).
CLANG_TIDY_FIX_PHRASES=(
  "github.event_name == 'workflow_dispatch'"
  "github.event_name == 'issue_comment'"
  "github.event.issue.pull_request"
  "fromJSON('[\"OWNER\", \"COLLABORATOR\", \"MEMBER\"]')"
  "github.event.comment.author_association"
  "startsWith(github.event.comment.body, format('@{0}bot tidy-fix', github.event.repository.name))"
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
echo "Pre-commit fix workflow (authorized PR comment commands)"
echo "======================================================="
for wf in "${PRE_COMMIT_FIX_WORKFLOWS[@]}"; do
  check_workflow "$WORKFLOWS_DIR/$wf" "$wf" "${PRE_COMMIT_FIX_PHRASES[@]}"
done

# Coverage workflow checks
echo ""
echo "Coverage workflow (custom gating on setup job)"
echo "==================================================="
for wf in "${COVERAGE_WORKFLOWS[@]}"; do
  check_workflow "$WORKFLOWS_DIR/$wf" "$wf" "${COVERAGE_PHRASES[@]}"
done

# Clang-tidy-fix workflow checks
echo ""
echo "Clang-tidy-fix workflow (custom gating on setup job)"
echo "==================================================="
for wf in "${CLANG_TIDY_FIX_WORKFLOWS[@]}"; do
  check_workflow "$WORKFLOWS_DIR/$wf" "$wf" "${CLANG_TIDY_FIX_PHRASES[@]}"
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
