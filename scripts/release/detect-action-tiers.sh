#!/usr/bin/env bash
set -euo pipefail

echo "=================================================="
echo "ACTION TIER CHANGE DETECTION"
echo "=================================================="

# 1. Find all action repositories
ACTION_REPOS=()
for dir in actions/*; do
  [ -d "$dir" ] || continue
  [ -f "$dir/action.yaml" ] || continue
  ACTION_REPOS+=("${dir#actions/}")
done

# 2. Determine dependencies for each repo
declare -A DEPENDENCIES
declare -A TIERS

for repo in "${ACTION_REPOS[@]}"; do
  # Extract dependencies on Framework-R-D/action-*
  # Format: Framework-R-D/action-NAME@SHA # vVERSION
  # We want the NAME part.
  DEPS=$(grep "uses: Framework-R-D/action-" "actions/$repo/action.yaml" | sed -E 's/.*Framework-R-D\/action-([^@ ]+).*/\1/' || true)
  DEPENDENCIES["$repo"]="$DEPS"
done

# 3. Iteratively assign tiers
UNASSIGNED=("${ACTION_REPOS[@]}")
CURRENT_TIER=1

while [ ${#UNASSIGNED[@]} -gt 0 ]; do
  TIER_REPOS=()
  STILL_UNASSIGNED=()

  for repo in "${UNASSIGNED[@]}"; do
    CAN_ASSIGN=true
    for dep in ${DEPENDENCIES["$repo"]}; do
      # If dep is an action repo and not yet assigned a tier, we can't assign this repo yet
      if [[ " ${ACTION_REPOS[*]} " == *" $dep "* ]] && [ -z "${TIERS["$dep"]:-}" ]; then
        CAN_ASSIGN=false
        break
      fi
    done

    if [ "$CAN_ASSIGN" = true ]; then
      TIER_REPOS+=("$repo")
    else
      STILL_UNASSIGNED+=("$repo")
    fi
  done

  if [ ${#TIER_REPOS[@]} -eq 0 ]; then
    echo "❌ Error: Circular dependency detected among: ${STILL_UNASSIGNED[*]}"
    exit 1
  fi

  for repo in "${TIER_REPOS[@]}"; do
    TIERS["$repo"]=$CURRENT_TIER
  done

  UNASSIGNED=("${STILL_UNASSIGNED[@]}")
  ((CURRENT_TIER++))
done

# Pre-calculate latest tags and change status for all action repos to avoid redundant git calls
declare -A LATEST_TAGS
declare -A HAS_CHANGES
declare -A UNCOMMITTED

for repo in "${ACTION_REPOS[@]}"; do
  (
    cd "actions/$repo" || exit 1
    git fetch --tags -q
    TAG=$(git describe --tags --abbrev=0 2>/dev/null || echo "NONE")
    echo "REPO_TAG=$TAG"
    if [ "$TAG" != "NONE" ]; then
      REPO_CHANGES=$(git log "${TAG}..HEAD" --oneline -- action.yaml 2>/dev/null || echo "")
      if [ -n "$REPO_CHANGES" ]; then
        echo "REPO_CHANGES=YES"
      else
        echo "REPO_CHANGES=NO"
      fi
    else
      echo "REPO_CHANGES=NONE"
    fi

    UNCOMMITTED_STATUS=$(git status --porcelain 2>/dev/null || echo "")
    if [ -n "$UNCOMMITTED_STATUS" ]; then
      echo "REPO_UNCOMMITTED=YES"
    else
      echo "REPO_UNCOMMITTED=NO"
    fi
  ) > "actions/.tmp_${repo}_status"
done

# Load the pre-calculated data
for repo in "${ACTION_REPOS[@]}"; do
  REPO_TAG="NONE"
  REPO_CHANGES="NO"
  REPO_UNCOMMITTED="NO"
  source "actions/.tmp_${repo}_status"
  LATEST_TAGS["$repo"]=$REPO_TAG
  HAS_CHANGES["$repo"]=$REPO_CHANGES
  UNCOMMITTED["$repo"]=$REPO_UNCOMMITTED
  rm "actions/.tmp_${repo}_status"
done

# 4. Analyze each tier
for ((tier=1; tier < CURRENT_TIER; tier++)); do
  echo "--- Tier $tier ---"

  for repo in "${ACTION_REPOS[@]}"; do
    if [ "${TIERS["$repo"]:-}" -eq "$tier" ]; then
      TAG=${LATEST_TAGS["$repo"]}
      CHANGE_STATUS=${HAS_CHANGES["$repo"]}

      if [ "$TAG" = "NONE" ]; then
        echo "📌 REPO: $repo (No tags found)"
      elif [ "$CHANGE_STATUS" = "YES" ]; then
        echo "🔥 REPO: $repo has unreleased changes since $TAG:"
        ( cd "actions/$repo" && git log "${TAG}..HEAD" --oneline -- action.yaml ) | sed 's/^/   - /'
      else
        echo "✅ REPO: $repo is clean ($TAG matches HEAD)"
      fi

      if [ "${UNCOMMITTED["$repo"]:-}" = "YES" ]; then
        echo "   ⚠️  REPO: $repo has uncommitted changes!"
      fi

      # Check dependencies for annotations
      for dep in ${DEPENDENCIES["$repo"]}; do
        if [[ " ${ACTION_REPOS[*]} " == *" $dep "* ]]; then
          # 1. Check if dep has changes
          DEP_CHANGE_STATUS=${HAS_CHANGES["$dep"]}
          if [ "$DEP_CHANGE_STATUS" = "YES" ]; then
            echo "   ⚠️  Dependency $dep has unreleased changes!"
          fi

          # 2. Check for version mismatch
          DEP_TAG=${LATEST_TAGS["$dep"]}
          REF_VERSION=$(grep "uses: Framework-R-D/action-$dep" "actions/$repo/action.yaml" | sed -E 's/.*# ([^ ]+).*/\1/' | head -n 1 || true)
          if [ -n "$REF_VERSION" ] && [ "$DEP_TAG" != "NONE" ]; then
            if [ "$(printf '%s\n%s' "$REF_VERSION" "$DEP_TAG" | sort -V | tail -n 1)" != "$REF_VERSION" ]; then
               echo "   ⚠️  Dependency $dep is at version $DEP_TAG, but $repo refers to $REF_VERSION"
            fi
          fi
        fi
      done
    fi
  done
done

echo ""
echo "=================================================="
echo "CHANGELOGS TO UPDATE"
echo "=================================================="
for repo in "${ACTION_REPOS[@]}"; do
  if [ "${HAS_CHANGES["$repo"]:-}" = "YES" ]; then
    echo "actions/$repo/CHANGELOG.md"
  fi
done
