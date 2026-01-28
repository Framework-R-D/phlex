# GitHub Actions Composite Actions

> **Note**
> This document describes the low-level reusable *actions* used within this repository. For instructions on how to use the complete, high-level *workflows* (e.g., `cmake-build.yaml`) from your own project, please see the guide in [`.github/REUSABLE_WORKFLOWS.md`](../REUSABLE_WORKFLOWS.md).

This directory contains reusable composite actions for Phlex CI/CD workflows.

## Available Actions

### setup-build-env

Verifies the container build environment and creates build directories.

**Inputs:**

- `source-path` (optional): Path where source code is checked out (default: `phlex-src`)
- `build-path` (optional): Path for build directory (default: `phlex-build`)

**Outputs:**

- `source-dir`: Absolute path to source directory
- `build-dir`: Absolute path to build directory

**Example:**

```yaml
- name: Setup build environment
  uses: ./phlex-src/.github/actions/setup-build-env
```

### configure-cmake

Configures CMake with automatic preset detection and customizable options.

**Inputs:**

- `source-path` (optional): Path where source code is checked out (default: `phlex-src`)
- `build-path` (optional): Path for build directory (default: `phlex-build`)
- `build-type` (optional): CMake build type (default: `Release`)
- `extra-options` (optional): Additional CMake configuration options
- `enable-form` (optional): Enable FORM support (default: `ON`)
- `form-root-storage` (optional): Enable FORM root storage (default: `ON`)

**Features:**

- Automatically detects and uses `CMakePresets.json` if present
- Sources the entrypoint script
- Applies standard project options (`PHLEX_USE_FORM`, `FORM_USE_ROOT_STORAGE`)

**Example:**

```yaml
- name: Configure CMake
  uses: ./phlex-src/.github/actions/configure-cmake
  with:
    build-type: Debug
    extra-options: '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DENABLE_COVERAGE=ON'
```

### build-cmake

Builds the project using CMake with configurable parallelism and target selection.

**Inputs:**

- `source-path` (optional): Path where source code is checked out (default: `phlex-src`)
- `build-path` (optional): Path for build directory (default: `phlex-build`)
- `target` (optional): CMake target to build (empty for default target)
- `parallel-jobs` (optional): Number of parallel jobs (empty for auto-detect with `nproc`)

**Example:**

```yaml
# Build default target with auto-detected parallel jobs
- name: Build project
  uses: ./phlex-src/.github/actions/build-cmake

# Build specific target
- name: Run clang-tidy
  uses: ./phlex-src/.github/actions/build-cmake
  with:
    target: clang-tidy-check
```

## Usage Pattern

Typical workflow usage:

```yaml
jobs:
  my-job:
    runs-on: ubuntu-24.04
    container:
      image: ghcr.io/framework-r-d/phlex-ci:latest
    
    steps:
    - name: Checkout code
      uses: actions/checkout@v4
      with:
        path: phlex-src
    
    - name: Setup build environment
      uses: ./phlex-src/.github/actions/setup-build-env
    
    - name: Configure CMake
      uses: ./phlex-src/.github/actions/configure-cmake
      with:
        build-type: Release
    
    - name: Build
      uses: ./phlex-src/.github/actions/build-cmake
    
    - name: Run tests
      run: |
        . /entrypoint.sh
        cd $GITHUB_WORKSPACE/phlex-build
        ctest -j $(nproc)
```

## Benefits

1. **Single Point of Maintenance**: Common logic is centralized in one place
2. **Consistency**: All workflows use the same configuration patterns
3. **Preset Detection**: Automatic detection and use of CMakePresets.json
4. **Flexibility**: Customizable through inputs while maintaining defaults
5. **Maintainability**: Easier to update container images, paths, or build options

## Maintenance

When updating these actions:

1. Update the action definition in the respective `action.yaml` file
2. Changes automatically apply to all workflows using the action
3. Test changes on a feature branch before merging
4. Document any breaking changes in commit messages
