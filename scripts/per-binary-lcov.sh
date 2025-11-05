#!/usr/bin/env bash
# Usage: per-binary-lcov.sh <binary> <profraw> <profdata> <lcov>
# Example: per-binary-lcov.sh test/bin/filter test/filter.d/default.profraw filter.profdata filter.lcov
set -euo pipefail

if [ $# -ne 4 ]; then
  echo "Usage: $0 <binary> <profraw> <profdata> <lcov>" >&2
  exit 1
fi

BIN="$1"
PROFRAW="$2"
PROFDATA="$3"
LCOV="$4"

LLVM_PROFDATA="/spack/var/spack/environments/phlex-ci/.spack-env/view/bin/llvm-profdata"
LLVM_COV="/spack/var/spack/environments/phlex-ci/.spack-env/view/bin/llvm-cov"
PYTHON="/spack/var/spack/environments/phlex-ci/.spack-env/view/bin/python3.14"
NORMALIZER="$(dirname "$0")/normalize_coverage_lcov.py"
REPO_ROOT="/workspaces/phlex"

"$LLVM_PROFDATA" merge --failure-mode=warn -sparse "$PROFRAW" -o "$PROFDATA"
"$LLVM_COV" export -object "$BIN" -instr-profile="$PROFDATA" --format=lcov > "$LCOV"
if [ -f "$NORMALIZER" ]; then
  "$PYTHON" "$NORMALIZER" --repo-root "$REPO_ROOT" --coverage-root "$REPO_ROOT" --coverage-alias "$REPO_ROOT" "$LCOV"
fi
