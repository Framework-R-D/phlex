#!/usr/bin/env bash
set -euo pipefail

REPO_DIR="${1:-}"
if [ -z "$REPO_DIR" ] || [ ! -d "$REPO_DIR" ]; then
  echo "Error: Pass a valid local repository directory name."
  exit 1
fi

cd "$REPO_DIR"
echo "=================================================="
echo "EXECUTING AUTOMATED RELEASE FOR: $REPO_DIR"
echo "=================================================="

# Ensure workspace tree is clean and synchronized
git fetch upstream main -q
git checkout main -q
git pull --ff-only upstream main -q

# Step 1: Calculate NEXT_VERSION increment rule
LATEST_TAG=$(git describe --tags --abbrev=0 2>/dev/null || echo "v0")
VERSION_NUM=$(echo "$LATEST_TAG" | sed 's/v//')
NEXT_NUM=$((VERSION_NUM + 1))
NEXT_VERSION="v${NEXT_NUM}"
TODAY=$(date +%Y-%m-%d)

echo "-> Current: $LATEST_TAG  --> Targets Next: $NEXT_VERSION"

# Step 2 & 3: Validate CHANGELOG.md has headings updated for the new version
echo "-> Verifying CHANGELOG.md headings..."
grep -q "^## ${NEXT_VERSION}" CHANGELOG.md \
  || { echo "❌ ERROR: Stop! Prepend '## ${NEXT_VERSION} --- ${TODAY}' to CHANGELOG.md before running."; exit 1; }

# Step 4 & 5: Build and push the Annotated Tag
echo "-> Injecting annotated metadata tag..."
git tag "$NEXT_VERSION" -m "$NEXT_VERSION - automated dependency cycle release"
git push upstream "$NEXT_VERSION" -q

# Step 6 & 7: Construct server-side Release notes wrapper
echo "-> Publishing GitHub Release notes configuration..."
gh release create "$NEXT_VERSION" \
  --title "$NEXT_VERSION - updates" \
  --generate-notes

# Step 8: Update README.md usage example block automatically
echo "-> Calculating immutable SHA signatures..."
NEW_SHA=$(gh api "repos/{owner}/{repo}/git/ref/tags/${NEXT_VERSION}" --jq .object.sha)
TYPE=$(gh api "repos/{owner}/{repo}/git/ref/tags/${NEXT_VERSION}" --jq .object.type)

if [ "$TYPE" = "tag" ]; then
  NEW_SHA=$(gh api "repos/{owner}/{repo}/git/tags/${NEW_SHA}" --jq .object.sha)
fi

REPO_FULL_NAME=$(gh repo view --json nameWithOwner --jq '.nameWithOwnership')

echo "-> Found Target Release SHA: $NEW_SHA"
echo "-> Modifying local README string tokens..."

OLD_PATTERN="${REPO_FULL_NAME}@[0-9a-f]\{40\} # v[0-9]*"
NEW_STRING="${REPO_FULL_NAME}@${NEW_SHA} # ${NEXT_VERSION}"

sed -i "s|${OLD_PATTERN}|${NEW_STRING}|" README.md

# Commit the updated documentation pin back to main
git add README.md
git commit -m "docs: pin README usage example to ${NEXT_VERSION} SHA" -q
git push upstream main -q

echo "🚀 SUCCESS: $NEXT_VERSION is complete and pinned!"
