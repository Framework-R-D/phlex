# Design for a Custom Spack Dependency Update Tool

## 1. Introduction

This document outlines the design for a custom tool that provides functionality similar to GitHub's Dependabot for Spack environments. Dependabot does not natively support Spack, so this tool will fill that gap by automatically checking for updates to Spack packages and creating pull requests with the necessary changes.

## 2. Proposed Solution

The proposed solution is a Python-based command-line tool, orchestrated by a GitHub Action. The tool will:

1.  Parse `spack.yaml` and `spack.lock` to identify all dependencies in a Spack environment.
2.  Use the `spack` command-line interface to find the latest available versions of each package.
3.  Update the `spack.yaml` file to use the new versions.
4.  Re-concretize the environment to generate an updated `spack.lock` file.
5.  If updates are found, create a pull request with the changes.

## 3. Detailed Design

### 3.1. Core Logic (Python Script)

The core of the tool will be a Python script that performs the dependency analysis and updates.

#### 3.1.1. Parsing Spack Files

-   **`spack.yaml`**: The script will use a standard YAML parsing library (like `PyYAML`) to read the `spack.yaml` file. It will extract the list of top-level dependencies from the `specs` section.
-   **`spack.lock`**: The `spack.lock` file is a JSON file that contains the full, concretized dependency tree. The script will parse this file to get a list of all packages (both top-level and transitive) and their exact pinned versions.

#### 3.1.2. Version Discovery

-   The script will require a functional Spack installation in its execution environment.
-   For each package identified in the `spack.lock` file, the script will execute the command `spack info --output json <package-name>`.
-   The JSON output from this command contains a list of all available versions for that package. The script will parse this output to determine the latest version.
-   The script will use a robust version comparison library (like `packaging.version`) to compare the current version with the latest available version, correctly handling semantic versioning.

#### 3.1.3. Update Logic

-   If a newer version of a package is found, the script will modify the `spack.yaml` file.
-   **Top-level dependencies**: If the package is a direct dependency listed in the `specs` section of `spack.yaml`, the script will update the version specifier there.
-   **Transitive dependencies**: If the package is a transitive dependency, a new entry will be added to the `packages` section of `spack.yaml` to pin the dependency to the new version.
-   **Re-concretization (Stretch Goal)**: After modifying `spack.yaml`, the script will run `spack concretize --force`. This command will update the `spack.lock` file with the new dependency tree. The script will capture the output of this command to detect and report any errors (e.g., version conflicts).

### 3.2. Automation with GitHub Actions

The Python script will be executed by a GitHub Action workflow.

-   **Trigger**: The workflow will be configured to run on a schedule (e.g., `cron: '0 0 * * 1'` for weekly runs every Monday at midnight).
-   **Environment**: The action will run in a containerized environment. A custom Docker image will be created that includes:
    -   A recent version of Python.
    -   A full Spack installation.
-   **Workflow Steps**:
    1.  **Checkout**: The action will check out the repository's code.
    2.  **Run the Script**: The Python script will be executed.
    3.  **Check for Changes**: The action will check if the script modified the `spack.yaml` or `spack.lock` files.
    4.  **Create Pull Request**: If changes are detected, the action will use a community action (e.g., `peter-evans/create-pull-request`) to create a new pull request. The PR body will contain a summary of the updated packages.
