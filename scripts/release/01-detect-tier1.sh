#!/usr/bin/env bash
set -euo pipefail

echo "=================================================="
echo "TIER 1 CHANGE DETECTION PROFILE"
echo "=================================================="

for dir in actions/*; do
  [ -d "$dir" ] || continue

  ( cd "$dir" || exit 1
    if ! git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
      exit 0
    fi

    # 1. Fetch latest tag context from remote origin
    git fetch --tags -q
    LATEST_TAG=$(git describe --tags --abbrev=0 2>/dev/null || echo "NONE")

    # 2. Check if this repository depends on any other Framework-R-D actions
    # (If it has zero internal dependencies, it belongs to the First Tier)
    DEPENDS_ON_INTERNAL=$(grep -q "uses: Framework-R-D/" action.yaml 2>/dev/null && echo "YES" || echo "NO")

    if [ "$DEPENDS_ON_INTERNAL" = "YES" ]; then
      # Skips Tier 2 & Tier 3 repositories during the first iteration pass
      exit 0
    fi

    if [ "$LATEST_TAG" = "NONE" ]; then
      echo "📌 REPO: $dir (No tags found - Needs initial v1 release)"
      exit 0
    fi

    # 3. Check for any commits touching action.yaml or source files since the latest tag
    CHANGES=$(git log "${LATEST_TAG}..HEAD" --oneline -- action.yaml 2>/dev/null || echo "")

    if [ -n "$CHANGES" ]; then
      echo "🔥 REPO: $dir has unreleased changes since $LATEST_TAG:"
      echo "$CHANGES" | sed 's/^/   - /'
    else
      echo "✅ REPO: $dir is clean ($LATEST_TAG matches HEAD)"
    fi
  )
done
