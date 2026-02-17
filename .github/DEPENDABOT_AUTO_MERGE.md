# Dependabot Auto-Merge Workflow

## Overview

The `dependabot-auto-merge.yaml` workflow automatically enables auto-merge for Dependabot pull requests targeting the `main` branch. Once all branch protection requirements are met (approvals, CI checks), the PR will merge automatically.

## How It Works

1. **Triggers:** The workflow runs when:
   - A Dependabot PR is opened or updated (`pull_request_target`)
   - A review is submitted on a Dependabot PR (`pull_request_review`)
   - CI checks complete on a Dependabot PR branch (`check_suite`)

2. **Verification:** The workflow verifies that:
   - The PR author is `dependabot[bot]`
   - The base branch is `main`

3. **Actions:**
   - Approves the PR (if not already approved)
   - Enables auto-merge

4. **Merge:** Once all branch protection requirements are satisfied, GitHub automatically merges the PR.

## Important: Use of `pull_request_target`

This workflow uses the `pull_request_target` event instead of `pull_request` to obtain elevated permissions needed to enable auto-merge on protected branches.

**Why this is needed:**
When workflows are triggered by Dependabot PRs using the `pull_request` event, the `GITHUB_TOKEN` has **read-only permissions** for security reasons, even if the workflow declares `contents: write` and `pull-requests: write`. This is a GitHub security feature to prevent malicious dependency updates from modifying the repository.

**Why this is safe:**
Using `pull_request_target` for Dependabot auto-merge is safe because:
1. We verify the PR author is `dependabot[bot]` before taking any action
2. We don't check out or execute code from the PR
3. We only enable auto-merge, which still requires all branch protection rules to pass
4. Dependabot PRs are created by GitHub's trusted Dependabot service

## Alternative Solutions

If you prefer not to use `pull_request_target`, there are more secure alternatives:

### Option 1: Use a GitHub App (Recommended)

Create a GitHub App with appropriate permissions and use it to generate tokens:

1. Create a GitHub App with `contents: write` and `pull_requests: write` permissions
2. Install the app on the repository
3. Generate an installation access token in the workflow
4. Use the token instead of `secrets.GITHUB_TOKEN`

Example:
```yaml
- uses: actions/create-github-app-token@v1
  id: app-token
  with:
    app-id: ${{ secrets.APP_ID }}
    private-key: ${{ secrets.PRIVATE_KEY }}
- name: Enable auto-merge
  env:
    GH_TOKEN: ${{ steps.app-token.outputs.token }}
  run: gh pr merge --auto --merge "$PR_NUMBER"
```

### Option 2: Use a Personal Access Token (PAT)

Create a PAT with appropriate permissions and store it as a repository secret:

1. Create a PAT with `repo` scope
2. Store it as a repository secret (e.g., `DEPENDABOT_PAT`)
3. Use it in the workflow

Example:
```yaml
- name: Enable auto-merge
  env:
    GH_TOKEN: ${{ secrets.DEPENDABOT_PAT }}
  run: gh pr merge --auto --merge "$PR_NUMBER"
```

**Note:** This is simpler than using a GitHub App but less secure because PATs have broader access and don't expire automatically.

### Option 3: Repository Settings

Configure the repository to allow Dependabot to bypass branch protection:

1. Go to repository Settings → Branches → Branch protection rules
2. Edit the rule for `main`
3. Add `dependabot[bot]` to the list of actors who can bypass required pull requests

**Note:** This approach has security implications and is generally not recommended.

## Troubleshooting

### Auto-merge is not being enabled

If the workflow runs but auto-merge is not enabled, check the workflow logs for error messages:

- **"auto-merge is already enabled"** - Auto-merge was already set on a previous run
- **"not authorized for this protected branch"** - The token doesn't have sufficient permissions (should not occur with `pull_request_target`)
- **"Required status checks"** - Waiting for CI checks to pass
- **"Required approving review"** - Waiting for approval (the workflow attempts to approve automatically)

### The PR is not merging automatically

Even after auto-merge is enabled, the PR won't merge until:
1. All required reviews are approved
2. All required status checks pass
3. No blocking conversations exist (if required)
4. All other branch protection requirements are met

Check the PR's "Merge" section for the current status.

## Security Considerations

- The workflow only runs on PRs opened by `dependabot[bot]`
- It does not check out or run code from the PR
- Auto-merge still requires all branch protection rules to pass
- The workflow is transparent and auditable (all runs are visible in the Actions tab)

## References

- [GitHub Docs: Automatically merging a pull request](https://docs.github.com/en/pull-requests/collaborating-with-pull-requests/incorporating-changes-from-a-pull-request/automatically-merging-a-pull-request)
- [GitHub Docs: Automating Dependabot with GitHub Actions](https://docs.github.com/en/code-security/dependabot/working-with-dependabot/automating-dependabot-with-github-actions)
- [Dependabot fetch-metadata action](https://github.com/dependabot/fetch-metadata)
