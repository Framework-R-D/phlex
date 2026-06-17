#!/usr/bin/env bash
set -euo pipefail

echo "=================================================="
echo "Opening Dependabot Management UI"
echo "=================================================="

if ! git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  echo "❌ Error: This folder is not a git repository."
  exit 1
fi

# 1. Fetch the absolute repository web URL from the GitHub CLI metadata
REPO_URL=$(gh repo view --json url --jq '.url')

# 2. Construct the direct deep-link target path for Dependabot logs
# On GitHub, this route lives under: /network/updates
TARGET_LOG_PAGE="${REPO_URL}/network/updates"

echo "--> Target URL: $TARGET_LOG_PAGE"
echo "--> Launching your browser..."

# 3. Detect operating system environment and trigger the native window engine
if command -v xdg-open >/dev/null 2>&1; then
  # Linux environment handler
  xdg-open "$TARGET_LOG_PAGE" 2>/dev/null
elif command -v open >/dev/null 2>&1; then
  # macOS environment handler
  open "$TARGET_LOG_PAGE"
else
  echo "❌ Error: Could not detect a desktop environment opener utility (xdg-open or open)."
  echo "          Please open this link manually instead:"
  echo "          $TARGET_LOG_PAGE"
  exit 1
fi

echo "⚡ Success: Browser session initialized."
