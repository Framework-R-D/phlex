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

# Ensure CHANGELOG.md is committed before tagging
if ! git diff --quiet CHANGELOG.md; then
  echo "❌ ERROR: Uncommitted changes detected in CHANGELOG.md."
  echo "Please commit your changelog updates before running the release script."
  exit 1
fi

# Step 1: Determine target version from CHANGELOG.md
echo "-> Determining target version from CHANGELOG.md..."
TARGET_VERSION=""

# Iterate through version headers in CHANGELOG.md in order of appearance
while read -r line; do
  VERSION=$(echo "$line" | sed 's/^## //; s/ --- .*//')

  # A version is pending if:
  # 1. The tag doesn't exist yet.
  # 2. The tag exists, but the README hasn't been pinned to this version.
  if ! git rev-parse "$VERSION" >/dev/null 2>&1 || ! grep -q " # ${VERSION}$" README.md; then
    TARGET_VERSION="$VERSION"
    break
  fi
done < <(grep "^## v[0-9]*" CHANGELOG.md)

if [ -z "$TARGET_VERSION" ]; then
  echo "❌ ERROR: No pending versions found in CHANGELOG.md (all versions tagged and pinned)."
  exit 1
fi

NEXT_VERSION="$TARGET_VERSION"
echo "-> Target version identified: $NEXT_VERSION"

# Step 2: Build and push the Annotated Tag
if ! git rev-parse "$NEXT_VERSION" >/dev/null 2>&1; then
  echo "-> Injecting annotated metadata tag..."
  git tag "$NEXT_VERSION" -m "$NEXT_VERSION - automated dependency cycle release"
  git push upstream "$NEXT_VERSION" -q
else
  echo "-> Tag $NEXT_VERSION already exists. Skipping."
fi

# Step 3: Construct server-side Release notes wrapper
if ! gh release view "$NEXT_VERSION" >/dev/null 2>&1; then
  echo "-> Publishing GitHub Release notes configuration..."
  gh release create "$NEXT_VERSION" \
    --title "$NEXT_VERSION - updates" \
    --generate-notes
else
  echo "-> GitHub Release $NEXT_VERSION already exists. Skipping."
fi

# Step 4: Update README.md usage example block automatically
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

if grep -q "$NEW_STRING" README.md; then
  echo "-> README already pinned to $NEXT_VERSION. Skipping commit."
else
  sed -i "s|${OLD_PATTERN}|${NEW_STRING}|" README.md

  # Commit the updated documentation pin back to main
  git add README.md
  git commit -m "docs: pin README usage example to ${NEXT_VERSION} SHA" -q
  git push upstream main -q
fi

echo "🚀 SUCCESS: $NEXT_VERSION is complete and pinned!"
