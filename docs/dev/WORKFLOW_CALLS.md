# Reusable GitHub Actions Workflows

This document describes all reusable GitHub Actions workflows in the phlex repository. These workflows have `workflow_call` triggers and can be invoked by other workflows.

## Trigger Model for Fix Workflows

The `*-fix` workflows (`clang-format-fix`, `clang-tidy-fix`, `cmake-format-fix`, `header-guards-fix`, `jsonnet-format-fix`, `markdown-fix`, `python-fix`, `yaml-fix`) follow a specific trigger model:

- **Triggers**: `issue_comment` (gated to OWNER/COLLABORATOR/MEMBER), `workflow_dispatch`, and `workflow_call`
- **NOT triggered by**: `pull_request` events

These workflows always run in the base-repo context to ensure they have write access to push fixes back to branches.

### Why WORKFLOW_PAT is Required

Commits pushed to re-trigger CI cannot use `GITHUB_TOKEN` because GitHub suppresses workflow runs triggered by `GITHUB_TOKEN` pushes to prevent infinite recursion loops. The `WORKFLOW_PAT` (a personal access token with repository scope) is required to push commits and trigger subsequent CI runs.

## Canonical Base-Input Contract

All check and fix workflows share a common base-input contract passed through the `Framework-R-D/action-workflow-setup` action:

| Input | Type | Required | Description |
| ------- | ------ | ---------- | ------------- |
| `checkout-path` | string | No | Path to check out code to |
| `skip-relevance-check` | boolean | No | Bypass relevance check (default: `false`) |
| `ref` | string | No | The branch, ref, or SHA to checkout |
| `repo` | string | No | The repository to checkout from |
| `pr-base-sha` | string | No | Base SHA of the PR for relevance check |
| `pr-head-sha` | string | No | Head SHA of the PR for relevance check |

## Check Workflows

Check workflows are triggered by `pull_request`, `push`, `workflow_dispatch`, and `workflow_call`. They analyze code without making changes.

### Actionlint Check

**File**: `.github/workflows/actionlint-check.yaml`

Validates GitHub Actions workflow files using actionlint.

#### Inputs

| Input | Type | Required | Default | Description |
| ------- | ------ | ---------- | --------- | ------------- |
| `checkout-path` | string | No | N/A | Path to check out code to |
| `skip-relevance-check` | boolean | No | `false` | Bypass relevance check |
| `pr-base-sha` | string | No | N/A | Base SHA of the PR for relevance check |
| `pr-head-sha` | string | No | N/A | Head SHA of the PR for relevance check |
| `ref` | string | No | N/A | The branch, ref, or SHA to checkout |
| `repo` | string | No | N/A | The repository to checkout from |

#### Example Invocation

```yaml
uses: ./.github/workflows/actionlint-check.yaml
with:
  checkout-path: .
  skip-relevance-check: false
  ref: refs/heads/main
```

### Clang-Format Check

**File**: `.github/workflows/clang-format-check.yaml`

Checks C++ source files for formatting compliance using clang-format 20.

#### Inputs

| Input | Type | Required | Default | Description |
| ------- | ------ | ---------- | --------- | ------------- |
| `checkout-path` | string | No | N/A | Path to check out code to |
| `skip-relevance-check` | boolean | No | `false` | Bypass relevance check |
| `pr-base-sha` | string | No | N/A | Base SHA of the PR for relevance check |
| `pr-head-sha` | string | No | N/A | Head SHA of the PR for relevance check |
| `ref` | string | No | N/A | The branch, ref, or SHA to checkout |
| `repo` | string | No | N/A | The repository to checkout from |

#### Example Invocation

```yaml
uses: ./.github/workflows/clang-format-check.yaml
with:
  checkout-path: .
  skip-relevance-check: false
```

### CMake Format Check

**File**: `.github/workflows/cmake-format-check.yaml`

Checks CMake files for formatting compliance using gersemi.

#### Inputs

| Input | Type | Required | Default | Description |
| ------- | ------ | ---------- | --------- | ------------- |
| `checkout-path` | string | No | N/A | Path to check out code to |
| `skip-relevance-check` | boolean | No | `false` | Bypass relevance check |
| `pr-base-sha` | string | No | N/A | Base SHA of the PR for relevance check |
| `pr-head-sha` | string | No | N/A | Head SHA of the PR for relevance check |
| `ref` | string | No | N/A | The branch, ref, or SHA to checkout |
| `repo` | string | No | N/A | The repository to checkout from |

#### Example Invocation

```yaml
uses: ./.github/workflows/cmake-format-check.yaml
with:
  checkout-path: .
```

### Header Guards Check

**File**: `.github/workflows/header-guards-check.yaml`

Checks C++ header files for proper include guard patterns.

#### Inputs

| Input | Type | Required | Default | Description |
| ------- | ------ | ---------- | --------- | ------------- |
| `checkout-path` | string | No | N/A | Path to check out code to |
| `skip-relevance-check` | boolean | No | `false` | Bypass relevance check |
| `pr-base-sha` | string | No | N/A | Base SHA of the PR for relevance check |
| `pr-head-sha` | string | No | N/A | Head SHA of the PR for relevance check |
| `ref` | string | No | N/A | The branch, ref, or SHA to checkout |
| `repo` | string | No | N/A | The repository to checkout from |

#### Example Invocation

```yaml
uses: ./.github/workflows/header-guards-check.yaml
with:
  checkout-path: .
```

### Jsonnet Format Check

**File**: `.github/workflows/jsonnet-format-check.yaml`

Checks Jsonnet files for formatting compliance using jsonnetfmt v0.22.0.

#### Inputs

| Input | Type | Required | Default | Description |
| ------- | ------ | ---------- | --------- | ------------- |
| `checkout-path` | string | No | N/A | Path to check out code to |
| `skip-relevance-check` | boolean | No | `false` | Bypass relevance check |
| `ref` | string | No | N/A | The branch, ref, or SHA to checkout |
| `repo` | string | No | N/A | The repository to checkout from |
| `pr-base-sha` | string | No | N/A | Base SHA of the PR for relevance check |
| `pr-head-sha` | string | No | N/A | Head SHA of the PR for relevance check |

#### Example Invocation

```yaml
uses: ./.github/workflows/jsonnet-format-check.yaml
with:
  checkout-path: .
```

### Markdown Check

**File**: `.github/workflows/markdown-check.yaml`

Validates Markdown files using markdownlint-cli2, excluding CHANGELOG.md.

#### Inputs

| Input | Type | Required | Default | Description |
| ------- | ------ | ---------- | --------- | ------------- |
| `checkout-path` | string | No | N/A | Path to check out code to |
| `skip-relevance-check` | boolean | No | `false` | Bypass relevance check |
| `pr-base-sha` | string | No | N/A | Base SHA of the PR for relevance check |
| `pr-head-sha` | string | No | N/A | Head SHA of the PR for relevance check |
| `ref` | string | No | N/A | The branch, ref, or SHA to checkout |
| `repo` | string | No | N/A | The repository to checkout from |

#### Example Invocation

```yaml
uses: ./.github/workflows/markdown-check.yaml
with:
  checkout-path: .
```

### Python Check

**File**: `.github/workflows/python-check.yaml`

Runs ruff for linting and formatting checks, and MyPy for type checking.

#### Inputs

| Input | Type | Required | Default | Description |
| ------- | ------ | ---------- | --------- | ------------- |
| `checkout-path` | string | No | N/A | Path to check out code to |
| `skip-relevance-check` | boolean | No | `false` | Bypass relevance check |
| `pr-base-sha` | string | No | N/A | Base SHA of the PR for relevance check |
| `pr-head-sha` | string | No | N/A | Head SHA of the PR for relevance check |
| `ref` | string | No | N/A | The branch, ref, or SHA to checkout |
| `repo` | string | No | N/A | The repository to checkout from |

#### Example Invocation

```yaml
uses: ./.github/workflows/python-check.yaml
with:
  checkout-path: .
```

### YAML Check

**File**: `.github/workflows/yaml-check.yaml`

Validates YAML files using yamllint.

#### Inputs

| Input | Type | Required | Default | Description |
| ------- | ------ | ---------- | --------- | ------------- |
| `checkout-path` | string | No | N/A | Path to check out code to |
| `skip-relevance-check` | boolean | No | `false` | Bypass relevance check |
| `pr-base-sha` | string | No | N/A | Base SHA of the PR for relevance check |
| `pr-head-sha` | string | No | N/A | Head SHA of the PR for relevance check |
| `ref` | string | No | N/A | The branch, ref, or SHA to checkout |
| `repo` | string | No | N/A | The repository to checkout from |

#### Example Invocation

```yaml
uses: ./.github/workflows/yaml-check.yaml
with:
  checkout-path: .
```

### Clang-Tidy Check

**File**: `.github/workflows/clang-tidy-check.yaml`

Runs clang-tidy on C++ source files. Also accepts `issue_comment` triggers.

#### Inputs

This workflow does not expose inputs via `workflow_call`. It is triggered by `issue_comment`, `pull_request`, `push`, or `workflow_dispatch`.

#### Example Invocation

```yaml
uses: ./.github/workflows/clang-tidy-check.yaml
with:
  # Uses issue_comment to trigger via: @reponamebot tidy-check
```

### CMake Build

**File**: `.github/workflows/cmake-build.yaml`

Configures and builds the project with CMake, then runs tests.

#### Inputs

| Input | Type | Required | Default | Description |
| ------- | ------ | ---------- | --------- | ------------- |
| `checkout-path` | string | No | N/A | Path to check out code to |
| `build-path` | string | No | N/A | Path for build artifacts |
| `skip-relevance-check` | boolean | No | `false` | Bypass relevance check |
| `build-combinations` | string | No | N/A | Space- or comma-separated list of build combinations to run |
| `ref` | string | No | N/A | The branch, ref, or SHA to checkout |
| `repo` | string | No | N/A | The repository to checkout from |
| `pr-base-sha` | string | No | N/A | Base SHA of the PR for relevance check |
| `pr-head-sha` | string | No | N/A | Head SHA of the PR for relevance check |

#### Example Invocation

```yaml
uses: ./.github/workflows/cmake-build.yaml
with:
  checkout-path: .
  build-path: build
  build-combinations: "gcc/none clang/asan"
```

### Coverage

**File**: `.github/workflows/coverage.yaml`

Generates code coverage reports using GCC or Clang.

#### Inputs

This workflow does not expose inputs via `workflow_call`. It is triggered by `issue_comment`, `pull_request`, `push`, `schedule`, or `workflow_dispatch`.

#### Example Invocation

```yaml
uses: ./.github/workflows/coverage.yaml
with:
  # Uses issue_comment to trigger via: @reponamebot coverage
```

## Fix Workflows

Fix workflows are triggered by `issue_comment`, `workflow_dispatch`, or `workflow_call`. They modify code and push changes back to branches.

### Clang-Format Fix

**File**: `.github/workflows/clang-format-fix.yaml`

Applies clang-format 20 to C++ source files.

#### Inputs

| Input | Type | Required | Default | Description |
| ------- | ------ | ---------- | --------- | ------------- |
| `checkout-path` | string | No | N/A | Path to check out code to |
| `ref` | string | Yes | N/A | The branch name to checkout and push fixes to |
| `repo` | string | Yes | N/A | The repository to checkout from |
| `skip-comment` | boolean | No | `false` | Skip posting PR comments |

#### Outputs

| Output | Description |
| -------- | ------------- |
| `changes` | Whether any fixes were applied |
| `pushed` | Whether the fixes were pushed to the remote branch |
| `commit_sha` | The full commit SHA of the applied fixes |
| `commit_sha_short` | The short commit SHA of the applied fixes |
| `patch_name` | Name of the patch file if fixes could not be pushed |

#### Example Invocation

```yaml
uses: ./.github/workflows/clang-format-fix.yaml
with:
  checkout-path: .
  ref: refs/heads/my-feature
  repo: ${{ github.repository }}
  skip-comment: false
```

### CMake Format Fix

**File**: `.github/workflows/cmake-format-fix.yaml`

Applies gersemi formatting to CMake files.

#### Inputs

| Input | Type | Required | Default | Description |
| ------- | ------ | ---------- | --------- | ------------- |
| `checkout-path` | string | No | N/A | Path to check out code to |
| `ref` | string | Yes | N/A | The branch name to checkout and push fixes to |
| `repo` | string | Yes | N/A | The repository to checkout from |
| `skip-comment` | boolean | No | `false` | Skip posting PR comments |

#### Outputs

| Output | Description |
| -------- | ------------- |
| `changes` | Whether any fixes were applied |
| `pushed` | Whether the fixes were pushed to the remote branch |
| `commit_sha` | The full commit SHA of the applied fixes |
| `commit_sha_short` | The short commit SHA of the applied fixes |
| `patch_name` | Name of the patch file if fixes could not be pushed |

#### Example Invocation

```yaml
uses: ./.github/workflows/cmake-format-fix.yaml
with:
  checkout-path: .
  ref: refs/heads/my-feature
  repo: ${{ github.repository }}
```

### Header Guards Fix

**File**: `.github/workflows/header-guards-fix.yaml`

Applies header guard fixes to C++ header files.

#### Inputs

| Input | Type | Required | Default | Description |
| ------- | ------ | ---------- | --------- | ------------- |
| `checkout-path` | string | No | N/A | Path to check out code to |
| `ref` | string | Yes | N/A | The branch name to checkout and push fixes to |
| `repo` | string | Yes | N/A | The repository to checkout from |
| `skip-comment` | boolean | No | `false` | Skip posting PR comments |

#### Outputs

| Output | Description |
| -------- | ------------- |
| `changes` | Whether any fixes were applied |
| `pushed` | Whether the fixes were pushed to the remote branch |
| `commit_sha` | The full commit SHA of the applied fixes |
| `commit_sha_short` | The short commit SHA of the applied fixes |
| `patch_name` | Name of the patch file if fixes could not be pushed |

#### Example Invocation

```yaml
uses: ./.github/workflows/header-guards-fix.yaml
with:
  checkout-path: .
  ref: refs/heads/my-feature
  repo: ${{ github.repository }}
```

### Jsonnet Format Fix

**File**: `.github/workflows/jsonnet-format-fix.yaml`

Applies jsonnetfmt formatting to Jsonnet files.

#### Inputs

| Input | Type | Required | Default | Description |
| ------- | ------ | ---------- | --------- | ------------- |
| `checkout-path` | string | No | N/A | Path to check out code to |
| `ref` | string | Yes | N/A | The branch name to checkout and push fixes to |
| `repo` | string | Yes | N/A | The repository to checkout from |
| `skip-comment` | boolean | No | `false` | Skip posting PR comments |

#### Outputs

| Output | Description |
| -------- | ------------- |
| `changes` | Whether any fixes were applied |
| `pushed` | Whether the fixes were pushed to the remote branch |
| `commit_sha` | The full commit SHA of the applied fixes |
| `commit_sha_short` | The short commit SHA of the applied fixes |
| `patch_name` | Name of the patch file if fixes could not be pushed |

#### Example Invocation

```yaml
uses: ./.github/workflows/jsonnet-format-fix.yaml
with:
  checkout-path: .
  ref: refs/heads/my-feature
  repo: ${{ github.repository }}
```

### Markdown Fix

**File**: `.github/workflows/markdown-fix.yaml`

Applies markdownlint-cli2 formatting fixes to Markdown files.

#### Inputs

| Input | Type | Required | Default | Description |
| ------- | ------ | ---------- | --------- | ------------- |
| `checkout-path` | string | No | N/A | Path to check out code to |
| `ref` | string | Yes | N/A | The branch name to checkout and push fixes to |
| `repo` | string | Yes | N/A | The repository to checkout from |
| `skip-comment` | boolean | No | `false` | Skip posting PR comments |

#### Outputs

| Output | Description |
| -------- | ------------- |
| `changes` | Whether any fixes were applied |
| `pushed` | Whether the fixes were pushed to the remote branch |
| `commit_sha` | The full commit SHA of the applied fixes |
| `commit_sha_short` | The short commit SHA of the applied fixes |
| `patch_name` | Name of the patch file if fixes could not be pushed |

#### Example Invocation

```yaml
uses: ./.github/workflows/markdown-fix.yaml
with:
  checkout-path: .
  ref: refs/heads/my-feature
  repo: ${{ github.repository }}
```

### Python Fix

**File**: `.github/workflows/python-fix.yaml`

Applies ruff formatting and lint fixes to Python files.

#### Inputs

| Input | Type | Required | Default | Description |
| ------- | ------ | ---------- | --------- | ------------- |
| `checkout-path` | string | No | N/A | Path to check out code to |
| `ref` | string | Yes | N/A | The branch name to checkout and push fixes to |
| `repo` | string | Yes | N/A | The repository to checkout from |
| `skip-comment` | boolean | No | `false` | Skip posting PR comments |

#### Outputs

| Output | Description |
| -------- | ------------- |
| `changes` | Whether any fixes were applied |
| `pushed` | Whether the fixes were pushed to the remote branch |
| `commit_sha` | The full commit SHA of the applied fixes |
| `commit_sha_short` | The short commit SHA of the applied fixes |
| `patch_name` | Name of the patch file if fixes could not be pushed |

#### Example Invocation

```yaml
uses: ./.github/workflows/python-fix.yaml
with:
  checkout-path: .
  ref: refs/heads/my-feature
  repo: ${{ github.repository }}
```

### YAML Fix

**File**: `.github/workflows/yaml-fix.yaml`

Applies prettier formatting to YAML files.

#### Inputs

| Input | Type | Required | Default | Description |
| ------- | ------ | ---------- | --------- | ------------- |
| `checkout-path` | string | No | N/A | Path to check out code to |
| `ref` | string | Yes | N/A | The branch name to checkout and push fixes to |
| `repo` | string | Yes | N/A | The repository to checkout from |
| `skip-comment` | boolean | No | `false` | Skip posting PR comments |

#### Outputs

| Output | Description |
| -------- | ------------- |
| `changes` | Whether any fixes were applied |
| `pushed` | Whether the fixes were pushed to the remote branch |
| `commit_sha` | The full commit SHA of the applied fixes |
| `commit_sha_short` | The short commit SHA of the applied fixes |
| `patch_name` | Name of the patch file if fixes could not be pushed |

#### Example Invocation

```yaml
uses: ./.github/workflows/yaml-fix.yaml
with:
  checkout-path: .
  ref: refs/heads/my-feature
  repo: ${{ github.repository }}
```

### Clang-Tidy Fix

**File**: `.github/workflows/clang-tidy-fix.yaml`

Applies clang-tidy fixes via clang-apply-replacements. Accepts an optional comma-separated list of checks to apply.

#### Inputs

This workflow does not expose inputs via `workflow_call`. It is triggered by `issue_comment` or `workflow_dispatch`.

#### Example Invocation

```yaml
uses: ./.github/workflows/clang-tidy-fix.yaml
with:
  # Uses issue_comment to trigger via: @reponamebot tidy-fix
  # Or workflow_dispatch with inputs for ref and tidy-checks
```

## Composite Workflows

### Format All

**File**: `.github/workflows/format-all.yaml`

Calls all individual formatter workflows (`clang-format-fix`, `cmake-format-fix`, `header-guards-fix`, `jsonnet-format-fix`, `markdown-fix`, `python-fix`, `yaml-fix`) in sequence and aggregates the results into a single PR comment.

This workflow is triggered only by `issue_comment` (gated to OWNER/COLLABORATOR/MEMBER).

#### Example Invocation

```yaml
uses: ./.github/workflows/format-all.yaml
with:
  # Triggered via issue_comment: @reponamebot format
```

## Usage Guidelines

1. **Check workflows**: Use `workflow_call` to invoke check workflows from other workflows. They are also triggered automatically on `pull_request` and `push` events.

2. **Fix workflows**: Use `workflow_call` to invoke fix workflows programmatically, or use the bot commands in PR comments:
   - `@reponamebot clang-fix`
   - `@reponamebot cmake-fix`
   - `@reponamebot header-guards-fix`
   - `@reponamebot jsonnet-fix`
   - `@reponamebot markdown-fix`
   - `@reponamebot python-fix`
   - `@reponamebot yaml-fix`
   - `@reponamebot format` (runs all formatters)

3. **When invoking via `workflow_call`**: Always pass `ref`, `repo`, and `checkout-path`. Set `skip-comment: true` when calling from another fixed workflow to avoid duplicate comments.

4. **Triggering fix workflows**: Use `workflow_dispatch` for manual triggering, or use the `issue_comment` trigger via bot commands in PRs.
