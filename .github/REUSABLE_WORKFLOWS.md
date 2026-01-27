# Using Reusable Workflows from the Phlex Repository

This guide explains how to integrate the reusable GitHub Actions workflows from the `Framework-R-D/phlex` repository into your own project.

### Prerequisites

#### Personal Access Token (PAT)

For workflows that automatically commit fixes to pull requests (e.g., formatters), you must create a Personal Access Token (PAT) and add it as a secret to your repository.

1.  **Create a PAT:** Follow the GitHub documentation to [create a fine-grained personal access token](https://docs.github.com/en/authentication/keeping-your-account-and-data-secure/managing-your-personal-access-tokens#creating-a-fine-grained-personal-access-token).
    *   Give it a descriptive name (e.g., `WORKFLOW_FIXES_PAT`).
    *   Grant it the following repository permissions:
        *   `Contents`: `Read and write`
        *   `Pull requests`: `Read and write`
2.  **Add the PAT as a Repository Secret:**
    *   In your repository, go to `Settings` > `Secrets and variables` > `Actions`.
    *   Create a new repository secret named `WORKFLOW_PAT` and paste your PAT as the value.

### Calling a Reusable Workflow

To use a workflow, you call it from a workflow file in your own repository's `.github/workflows/` directory. The basic syntax is:

```yaml
jobs:
  some_job:
    uses: Framework-R-D/phlex/.github/workflows/<workflow_file_name>.yaml@cef968c52aab432b836bb28119a9661c82c8b0d1
    with:
      # ... inputs for the workflow ...
    secrets:
      WORKFLOW_PAT: ${{ secrets.WORKFLOW_PAT }}
```

**Note:** For stability and security, it is highly recommended to pin the workflow to a specific commit SHA rather than a branch like `@main`. Using a mutable branch means you will automatically receive updates, which could include breaking changes or, in a worst-case scenario, malicious code. Pinning to a commit SHA ensures you are using a fixed, reviewed version of the workflow.

For development purposes, you may choose to use `@main` at your own risk to get the latest changes.

---

### For Contributors Working on a Fork of Phlex

If you are developing on a fork of `Framework-R-D/phlex` itself, the CI/CD workflows will run automatically on your pull requests within the fork, just as they do on the main repository. You do not need to use the `uses:` syntax described below.

However, to enable the automatic fixing features (e.g., for `cmake-format-fix` or `python-fix`), you will need to perform two steps:

1.  **Enable Workflows:** By default, GitHub Actions are disabled on forks. You must manually enable them by going to the `Actions` tab of your forked repository and clicking the "I understand my workflows, go ahead and enable them" button.
2.  **Create the `WORKFLOW_PAT` Secret:** The auto-fix workflows require a Personal Access Token (PAT) with write permissions to commit changes back to your PR branch. Follow the instructions in the "Prerequisites" section above to create a PAT and add it as a secret named `WORKFLOW_PAT` **to your forked repository's settings**.

Once you have done this, you can trigger the auto-fix workflows by commenting on a pull request in your fork (e.g., `@phlexbot format`).

---

### Available Workflows and Their Inputs

#### 1. `cmake-build.yaml`

Builds and tests your project using CMake.

**Usage Example:**

```yaml
jobs:
  build_and_test:
    uses: Framework-R-D/phlex/.github/workflows/cmake-build.yaml@cef968c52aab432b836bb28119a9661c82c8b0d1
    with:
      # Optional: A list of build combinations to run (e.g., "gcc/asan clang/tsan")
      build-combinations: 'all -clang/valgrind'
      # Required for PRs from forks if you want auto-formatting to work
      ref: ${{ github.head_ref }}
      repo: ${{ github.repository }}
```

**All Inputs:**
*   `checkout-path` (string, optional): Path to check out code to.
*   `build-path` (string, optional): Path for build artifacts.
*   `skip-relevance-check` (boolean, optional, default: `false`): Bypass the check that only runs the build if C++ or CMake files have changed.
*   `build-combinations` (string, optional): A space-separated list of build combinations to run.
*   `ref` (string, optional): The branch or ref to check out.
*   `repo` (string, optional): The repository to check out from (e.g., `my-org/my-repo`).
*   `pr-base-sha` (string, optional): Base SHA of the PR for relevance check.
*   `pr-head-sha` (string, optional): Head SHA of the PR for relevance check.

#### 2. `python-check.yaml`

Checks Python code for formatting and type errors using `ruff` and `mypy`.

**Usage Example:**

```yaml
jobs:
  check_python:
    uses: Framework-R-D/phlex/.github/workflows/python-check.yaml@cef968c52aab432b836bb28119a9661c82c8b0d1
```

**All Inputs:**
*   `checkout-path` (string, optional): Path to check out code to.
*   `skip-relevance-check` (boolean, optional, default: `false`): Bypass the check that only runs if Python files have changed.
*   `pr-base-sha` (string, optional): Base SHA of the PR for relevance check.
*   `pr-head-sha` (string, optional): Head SHA of the PR for relevance check.

#### 3. `cmake-format-fix.yaml`

Automatically formats CMake files using `gersemi` and commits the changes. Typically triggered by an `issue_comment`.

**Usage Example (in a workflow triggered by `issue_comment`):**

```yaml
name: 'Bot Commands'
on:
  issue_comment:
    types: [created]

jobs:
  format-cmake:
    # Run only on comments from collaborators/owners that start with the bot command
    if: >
      github.event.issue.pull_request &&
      (github.event.comment.author_association == 'COLLABORATOR' || github.event.comment.author_association == 'OWNER') &&
      startsWith(github.event.comment.body, format('@{0}bot format', github.event.repository.name))
    uses: Framework-R-D/phlex/.github/workflows/cmake-format-fix.yaml@cef968c52aab432b836bb28119a9661c82c8b0d1
    with:
      # The ref and repo of the PR need to be retrieved and passed
      ref: ${{ steps.get_pr_info.outputs.ref }}
      repo: ${{ steps.get_pr_info.outputs.repo }}
    secrets:
      WORKFLOW_PAT: ${{ secrets.WORKFLOW_PAT }}
```
*Note: You would need a preliminary step (`get_pr_info`) to extract the PR's `ref` and `repo` from the `issue_comment` event.*

**All Inputs:**
*   `checkout-path` (string, optional): Path to check out code to.
*   `ref` (string, **required**): The branch or ref to check out.
*   `repo` (string, **required**): The repository to check out from.

#### 4. `python-fix.yaml`

Automatically formats and fixes Python code using `ruff` and commits the changes. Typically triggered by an `issue_comment`.

**Usage Example (in a workflow triggered by `issue_comment`):**
*Similar to `cmake-format-fix.yaml`, but triggered by a command like `@<repo>bot python-fix`.*

**All Inputs:**
*   `checkout-path` (string, optional): Path to check out code to.
*   `ref` (string, **required**): The branch or ref to check out.
*   `repo` (string, **required**): The repository to check out from.

#### Other Workflows

The repository also provides `actionlint-check.yaml`, `cmake-format-check.yaml`, and `codeql-analysis.yaml`, which can be used in a similar manner.
