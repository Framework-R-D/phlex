# GitHub Actions Composite Actions

This directory contains reusable composite actions for Phlex CI/CD workflows.

## Available Actions

### setup-build-env

Sets up the Phlex build environment by sourcing the entrypoint script and creating build directories.

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
      image: ghcr.io/framework-r-d/phlex-ci:2025-10-21
    
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
        . $GITHUB_WORKSPACE/phlex-src/ci/entrypoint.sh
        cd $GITHUB_WORKSPACE/phlex-build
        ctest -j $(nproc)
```

## Prerequisites

- These composite actions are designed to run in a containerized environment using the `ghcr.io/framework-r-d/phlex-ci` image.
- The repository must be checked out to the `phlex-src` path within the `$GITHUB_WORKSPACE`.

## Benefits

1. **Single Point of Maintenance**: Common logic is centralized in one place.
2. **Consistency**: All workflows use the same configuration patterns.
3. **Preset Detection**: Automatic detection and use of `CMakePresets.json`.
4. **Flexibility**: Customizable through inputs while maintaining sensible defaults.
5. **Maintainability**: Easier to update container images, paths, or build options across all workflows.

## Maintenance

When updating these actions:

1. Update the action definition in the respective `action.yaml` file
2. Changes automatically apply to all workflows using the action
3. Test changes on a feature branch before merging
4. Document any breaking changes in commit messages
