# Using Reusable Workflows from the Phlex Repository

## Overview and General Instructions

The workflows in `Framework-R-D/phlex/` may be invoked as follows:

1. Automatically as part of CI checks on a PR submitted to `Framework-R-D/phlex`, at PR creation time and thereafter on pushes to the PR branch. This should work whether your PR branch is situated in the primary repository or a fork.
1. Via triggering comments on the PR (`@${{ github.event.repository.name }}bot <action>`).
1. Via the "actions" tab on the project's GitHub web page.

Additionally, you can configure your own fork of Phlex to run CI checks on local PRs, and on its default branch, following the instructions below.

If you are a Phlex-affiliated developer working on a dependent package of Phlex, or on a different Cetmodules-using package, or on Cetmodules itself, you may be able to invoke these workflows on your own project following the information in this guide. However, this is only supported for Phlex-affiliated developers, and even then on a best effort basis. We do **not** support or encourage others to utilize these workflows at this time.

### Running Workflows Manually (`workflow_dispatch`)

Most workflows in this repository can be run manually on any branch, tag, or commit. This is useful for testing changes without creating a pull request or for applying fixes to a specific branch.

To run a workflow manually:

1. Navigate to the **Actions** tab of the Phlex repository (or your fork).
1. In the left sidebar, click the workflow you want to run (e.g., **Clang-Format Check**).
1. Above the list of workflow runs, you will see a banner that says "This workflow has a `workflow_dispatch` event trigger." Click the **Run workflow** dropdown on the right.
1. Use the **Branch/tag** dropdown to select the branch you want to run the workflow on.
1. Some workflows have additional inputs (e.g., the `cmake-build` workflow allows you to specify build combinations). Fill these out as needed.
1. Click the **Run workflow** button.

### For Contributors Working on a Fork of Phlex

If you are developing on a fork of `Framework-R-D/phlex` itself, the CI/CD workflows will run automatically on your pull requests within the fork, just as they do on the main repository. You do not need to use the `uses:` syntax described below.

However, to enable the automatic fixing features (e.g., for `cmake-fix` or `python-fix`), you will need to perform two steps:

1. **Enable Workflows:** By default, GitHub Actions are disabled on forks. You must manually enable them by going to the `Actions` tab of your forked repository and clicking the "I understand my workflows, go ahead and enable them" button.
1. **Create the `WORKFLOW_PAT` Secret:** The auto-fix workflows require a Personal Access Token (PAT) with write permissions to commit changes back to your PR branch. Follow the instructions below to create a PAT and add it as a secret named `WORKFLOW_PAT` **to your forked repository's settings**.

Once you have done this, you can trigger the auto-fix workflows by commenting on a pull request in your fork (e.g., `@${{ github.event.repository.name }}bot format`).

**Note on Authorization:** Comment-triggered workflows use authorization checks to ensure only trusted users can trigger potentially code-modifying operations. The workflows check that the comment author has one of the following associations: `OWNER`, `COLLABORATOR`, or `MEMBER`. This covers repository owners, explicitly invited collaborators, and organization members with any level of repository access. For a detailed analysis of the authorization model and security considerations, see [AUTHORIZATION_ANALYSIS.md](AUTHORIZATION_ANALYSIS.md).

### Creating a Personal Access Token (PAT)

For workflows that automatically commit fixes to pull requests (e.g., formatters), you must create a Personal Access Token (PAT) and add it as a secret to your repository.

1. **Create a PAT:** Follow the GitHub documentation to [create a fine-grained personal access token](https://docs.github.com/en/authentication/keeping-your-account-and-data-secure/managing-your-personal-access-tokens#creating-a-fine-grained-personal-access-token).
   - Give it a descriptive name (e.g., `WORKFLOW_FIXES_PAT`).
   - Grant it the following repository permissions:
      - `Contents`: `Read and write`
      - `Pull requests`: `Read and write`
2. **Add the PAT as a Repository Secret:**
   - In your repository, go to `Settings` > `Secrets and variables` > `Actions`.
   - Create a new repository secret named `WORKFLOW_PAT` and paste your PAT as the value.

### Calling a Reusable Workflow from a Different Repository

To use a workflow, you call it from a workflow file in your own repository's `.github/workflows/` directory. The basic syntax is:

```yaml
jobs:
  some_job:
    uses: Framework-R-D/phlex/.github/workflows/<workflow_file_name>.yaml@<commit_sha>
    with:
      # ... inputs for the workflow ...
    secrets:
      WORKFLOW_PAT: ${{ secrets.WORKFLOW_PAT }}
```

You should follow the instructions in the previous section to create the `WORKFLOW_PAT` secret for your own repository.

**Note:** For stability and security, it is highly recommended to pin the workflow to a specific commit SHA rather than a branch like `@main`. Using a mutable branch means you will automatically receive updates, which could include breaking changes or, in a worst-case scenario, malicious code. Pinning to a commit SHA ensures you are using a fixed, reviewed version of the workflow.

For development purposes, you may choose to use `@main` at your own risk to get the latest changes.

#### Emulating Trigger Types and Relevance Checks

When calling a reusable workflow, it's often desirable to emulate the behavior of the calling workflow's trigger. For example, if your workflow is triggered by a manual `workflow_dispatch`, you likely want the reusable workflow to skip its relevance detection and check all files. Conversely, if triggered by a `pull_request`, you want detection enabled.

You can achieve this by passing the appropriate value to the `skip-relevance-check` input:

```yaml
    with:
      skip-relevance-check: ${{ github.event_name == 'workflow_dispatch' || github.event_name == 'issue_comment' }}
```

Additionally, to ensure the reusable workflow can access the correct code in an extra-repository context, always pass the `ref` and `repo`:

```yaml
    with:
      ref: ${{ github.event.pull_request.head.sha || github.sha }}
      repo: ${{ github.repository }}
```

---

## Available Workflows and Their Inputs

### 1. `cmake-build.yaml`

Builds and tests your project using CMake.

#### Usage Example

```yaml
jobs:
  build_and_test:
    uses: Framework-R-D/phlex/.github/workflows/cmake-build.yaml@<commit_sha>
    with:
      # Optional: A list of build combinations to run (e.g., "gcc/asan clang/tsan")
      build-combinations: 'all -clang/valgrind'
      # Required for PRs from forks if you want auto-formatting to work
      ref: ${{ github.head_ref }}
      repo: ${{ github.repository }}
```

#### All Inputs

- `checkout-path` (string, optional): Path to check out code to.
- `build-path` (string, optional): Path for build artifacts.
- `skip-relevance-check` (boolean, optional, default: `false`): Bypass the check that only runs the build if C++ or CMake files have changed.
- `build-combinations` (string, optional): A space-separated list of build combinations to run.
- `ref` (string, optional): The branch, ref, or SHA to check out.
- `repo` (string, optional): The repository to check out from.
- `pr-base-sha` (string, optional): Base SHA of the PR for relevance check.
- `pr-head-sha` (string, optional): Head SHA of the PR for relevance check.

### 2. `cmake-format-check.yaml`

Checks CMake files for formatting issues using `gersemi`.

#### Usage Example

```yaml
jobs:
  check_cmake_format:
    uses: Framework-R-D/phlex/.github/workflows/cmake-format-check.yaml@<commit_sha>
```

#### All Inputs

- `checkout-path` (string, optional): Path to check out code to.
- `skip-relevance-check` (boolean, optional, default: `false`): Bypass the check that only runs if CMake files have changed.
- `ref` (string, optional): The branch, ref, or SHA to check out.
- `repo` (string, optional): The repository to check out from.
- `pr-base-sha` (string, optional): Base SHA of the PR for relevance check.
- `pr-head-sha` (string, optional): Head SHA of the PR for relevance check.

### 3. `cmake-format-fix.yaml`

Automatically formats CMake files using `gersemi` and commits the changes. Typically triggered by an `issue_comment`.

#### Usage Example

```yaml
name: 'Bot Commands'
on:
  issue_comment:
    types: [created]

jobs:
  pre-check:
    # Extract PR details for the comment trigger
    if: >
      github.event.issue.pull_request &&
      contains(fromJSON('["OWNER", "COLLABORATOR", "MEMBER"]'), github.event.comment.author_association) &&
      (
        startsWith(github.event.comment.body, format('@{0}bot format', github.event.repository.name)) ||
        startsWith(github.event.comment.body, format('@{0}bot cmake-fix', github.event.repository.name))
      )
    runs-on: ubuntu-latest
    outputs:
      ref: ${{ steps.pr_info.outputs.ref }}
      repo: ${{ steps.pr_info.outputs.repo }}
    steps:
      - id: pr_info
        uses: Framework-R-D/phlex/.github/actions/get-pr-info@<commit_sha>

  format-cmake:
    needs: pre-check
    uses: Framework-R-D/phlex/.github/workflows/cmake-format-fix.yaml@<commit_sha>
    with:
      ref: ${{ needs.pre-check.outputs.ref }}
      repo: ${{ needs.pre-check.outputs.repo }}
    secrets:
      WORKFLOW_PAT: ${{ secrets.WORKFLOW_PAT }}
```

#### All Inputs

- `checkout-path` (string, optional): Path to check out code to.
- `ref` (string, **required**): The branch, ref, or SHA to check out.
- `repo` (string, **required**): The repository to check out from.

### 4. `python-check.yaml`

Checks Python code for formatting and type errors using `ruff` and `mypy`.

#### Usage Example

```yaml
jobs:
  check_python:
    uses: Framework-R-D/phlex/.github/workflows/python-check.yaml@<commit_sha>
```

#### All Inputs

- `checkout-path` (string, optional): Path to check out code to.
- `skip-relevance-check` (boolean, optional, default: `false`): Bypass the check that only runs if Python files have changed.
- `ref` (string, optional): The branch, ref, or SHA to check out.
- `repo` (string, optional): The repository to check out from.
- `pr-base-sha` (string, optional): Base SHA of the PR for relevance check.
- `pr-head-sha` (string, optional): Head SHA of the PR for relevance check.

### 5. `python-fix.yaml`

Automatically formats and fixes Python code using `ruff` and commits the changes. Typically triggered by an `issue_comment`.

#### Usage Example

```yaml
name: 'Bot Commands'
on:
  issue_comment:
    types: [created]

jobs:
  pre-check:
    if: >
      github.event.issue.pull_request &&
      contains(fromJSON('["OWNER", "COLLABORATOR", "MEMBER"]'), github.event.comment.author_association) &&
      startsWith(github.event.comment.body, format('@{0}bot python-fix', github.event.repository.name))
    runs-on: ubuntu-latest
    outputs:
      ref: ${{ steps.pr_info.outputs.ref }}
      repo: ${{ steps.pr_info.outputs.repo }}
    steps:
      - id: pr_info
        uses: Framework-R-D/phlex/.github/actions/get-pr-info@<commit_sha>

  fix-python:
    needs: pre-check
    uses: Framework-R-D/phlex/.github/workflows/python-fix.yaml@<commit_sha>
    with:
      ref: ${{ needs.pre-check.outputs.ref }}
      repo: ${{ needs.pre-check.outputs.repo }}
    secrets:
      WORKFLOW_PAT: ${{ secrets.WORKFLOW_PAT }}
```

#### All Inputs

- `checkout-path` (string, optional): Path to check out code to.
- `ref` (string, **required**): The branch, ref, or SHA to check out.
- `repo` (string, **required**): The repository to check out from.

### 6. `markdown-check.yaml`

Checks Markdown files for formatting issues using `markdownlint`.

#### Usage Example

```yaml
jobs:
  check_markdown:
    uses: Framework-R-D/phlex/.github/workflows/markdown-check.yaml@<commit_sha>
```

#### All Inputs

- `checkout-path` (string, optional): Path to check out code to.
- `skip-relevance-check` (boolean, optional, default: `false`): Bypass the check that only runs if Markdown files have changed.
- `ref` (string, optional): The branch, ref, or SHA to check out.
- `repo` (string, optional): The repository to check out from.
- `pr-base-sha` (string, optional): Base SHA of the PR for relevance check.
- `pr-head-sha` (string, optional): Head SHA of the PR for relevance check.

### 7. `markdown-fix.yaml`

Automatically formats Markdown files using `markdownlint` and commits the changes. Typically triggered by an `issue_comment`.

#### Usage Example

```yaml
name: 'Bot Commands'
on:
  issue_comment:
    types: [created]

jobs:
  pre-check:
    if: >
      github.event_name == 'issue_comment' &&
      github.event.issue.pull_request &&
      contains(fromJSON('["OWNER", "COLLABORATOR", "MEMBER"]'), github.event.comment.author_association) &&
      (
        startsWith(github.event.comment.body, format('@{0}bot format', github.event.repository.name)) ||
        startsWith(github.event.comment.body, format('@{0}bot markdown-fix', github.event.repository.name))
      )
    runs-on: ubuntu-latest
    outputs:
      ref: ${{ steps.pr_info.outputs.ref }}
      repo: ${{ steps.pr_info.outputs.repo }}
    steps:
      - id: pr_info
        uses: Framework-R-D/phlex/.github/actions/get-pr-info@<commit_sha>

  fix-markdown:
    needs: pre-check
    uses: Framework-R-D/phlex/.github/workflows/markdown-fix.yaml@<commit_sha>
    with:
      ref: ${{ needs.pre-check.outputs.ref }}
      repo: ${{ needs.pre-check.outputs.repo }}
    secrets:
      WORKFLOW_PAT: ${{ secrets.WORKFLOW_PAT }}
```

#### All Inputs

- `checkout-path` (string, optional): Path to check out code to.
- `ref` (string, **required**): The branch, ref, or SHA to check out.
- `repo` (string, **required**): The repository to check out from.

### 8. `actionlint-check.yaml`

Checks GitHub Actions workflow files for errors and best practices using `actionlint`.

#### Usage Example

```yaml
jobs:
  check_actions:
    uses: Framework-R-D/phlex/.github/workflows/actionlint-check.yaml@<commit_sha>
```

#### All Inputs

- `checkout-path` (string, optional): Path to check out code to.
- `skip-relevance-check` (boolean, optional, default: `false`): Bypass the check that only runs if workflow files have changed.
- `ref` (string, optional): The branch, ref, or SHA to check out.
- `repo` (string, optional): The repository to check out from.
- `pr-base-sha` (string, optional): Base SHA of the PR for relevance check.
- `pr-head-sha` (string, optional): Head SHA of the PR for relevance check.

### 9. `jsonnet-format-check.yaml`

Checks Jsonnet files for formatting issues using `jsonnetfmt`.

#### Usage Example

```yaml
jobs:
  check_jsonnet:
    uses: Framework-R-D/phlex/.github/workflows/jsonnet-format-check.yaml@<commit_sha>
    with:
      # Optional: bypass detection and check all files (useful for manual triggers)
      skip-relevance-check: ${{ github.event_name == 'workflow_dispatch' }}
```

#### All Inputs

- `checkout-path` (string, optional): Path to check out code to.
- `skip-relevance-check` (boolean, optional, default: `false`): Bypass the check that only runs if Jsonnet files have changed.
- `ref` (string, optional): The branch, ref, or SHA to checkout.
- `repo` (string, optional): The repository to checkout from.
- `pr-base-sha` (string, optional): Base SHA of the PR for relevance check.
- `pr-head-sha` (string, optional): Head SHA of the PR for relevance check.

### 10. `jsonnet-format-fix.yaml`

Automatically formats Jsonnet files using `jsonnetfmt` and commits the changes. Typically triggered by an `issue_comment`.

#### Usage Example

```yaml
name: 'Bot Commands'
on:
  issue_comment:
    types: [created]

jobs:
  pre-check:
    if: >
      github.event.issue.pull_request &&
      contains(fromJSON('["OWNER", "COLLABORATOR", "MEMBER"]'), github.event.comment.author_association) &&
      (
        startsWith(github.event.comment.body, format('@{0}bot format', github.event.repository.name)) ||
        startsWith(github.event.comment.body, format('@{0}bot jsonnet-fix', github.event.repository.name))
      )
    runs-on: ubuntu-latest
    outputs:
      ref: ${{ steps.pr_info.outputs.ref }}
      repo: ${{ steps.pr_info.outputs.repo }}
    steps:
      - id: pr_info
        uses: Framework-R-D/phlex/.github/actions/get-pr-info@<commit_sha>

  fix-jsonnet:
    needs: pre-check
    uses: Framework-R-D/phlex/.github/workflows/jsonnet-format-fix.yaml@<commit_sha>
    with:
      ref: ${{ needs.pre-check.outputs.ref }}
      repo: ${{ needs.pre-check.outputs.repo }}
    secrets:
      WORKFLOW_PAT: ${{ secrets.WORKFLOW_PAT }}
```

#### All Inputs

- `checkout-path` (string, optional): Path to check out code to.
- `ref` (string, **required**): The branch, ref, or SHA to checkout.
- `repo` (string, **required**): The repository to checkout from.

### 11. `codeql-analysis.yaml`

Performs static analysis on the codebase using GitHub CodeQL to identify potential security vulnerabilities and coding errors.

#### Usage Example

```yaml
jobs:
  analyze:
    uses: Framework-R-D/phlex/.github/workflows/codeql-analysis.yaml@<commit_sha>
```

#### All Inputs

- `checkout-path` (string, optional): Path to check out code to.
- `build-path` (string, optional): Path for build artifacts.
- `language-matrix` (string, optional, default: `'["cpp", "python", "actions"]'`): JSON array of languages to analyze.
- `pr-number` (string, optional): PR number if run in PR context.
- `pr-head-repo` (string, optional): The full name of the PR head repository.
- `pr-base-repo` (string, optional): The full name of the PR base repository.

### Other Workflows

The repository also provides `clang-format-check.yaml`, `clang-format-fix.yaml`, `clang-tidy-check.yaml`, and `clang-tidy-fix.yaml`. However, these workflows are currently **not** available for reuse via `workflow_call` as they are specifically intended for use on this repository and its forks.
