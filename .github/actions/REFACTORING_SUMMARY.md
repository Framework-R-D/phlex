# Composite Actions Refactoring Summary

## Overview

Refactored GitHub Actions workflows to use composite actions for better maintainability and single-point-of-maintenance of common functionality.

## Created Composite Actions

### 1. setup-build-env

**Location**: `.github/actions/setup-build-env/action.yaml`

**Purpose**: Sets up the build environment by sourcing entrypoint script and creating directories

**Provides**:

- Sources `ci/entrypoint.sh`
- Creates build directory
- Outputs source and build directory paths

### 2. configure-cmake

**Location**: `.github/actions/configure-cmake/action.yaml`

**Purpose**: Configures CMake with preset detection and standard options

**Features**:

- Automatic CMakePresets.json detection
- Configurable build type and extra options
- Standard FORM options applied
- Preset usage logging

### 3. build-cmake

**Location**: `.github/actions/build-cmake/action.yaml`

**Purpose**: Builds the project with CMake

**Features**:

- Configurable target selection
- Auto-detected or custom parallel jobs
- Sources entrypoint for environment

## Updated Workflows

### cmake-build.yaml

**Before**: ~45 lines of inline bash with duplicated logic
**After**: Clean composite action calls

**Changes**:

- Replaced Configure CMake step with `configure-cmake` action
- Replaced Build step with `build-cmake` action
- Added `setup-build-env` action
- Reduced from ~45 to ~20 lines for core build logic

### clang-tidy-check.yaml

**Before**: ~30 lines of duplicated CMake configuration
**After**: Clean composite action calls

**Changes**:

- Replaced configuration/build steps with composite actions
- Specified Debug build type and compile commands export
- Consistent with cmake-build pattern

### clang-tidy-fix.yaml

**Before**: ~30 lines of duplicated CMake configuration
**After**: Clean composite action calls

**Changes**:

- Same refactoring as clang-tidy-check
- Maintains PR-specific checkout behavior

## Benefits Achieved

### 1. Single Point of Maintenance

- **Container image**: Only needs updating in composite actions
- **Preset detection logic**: Centralized in `configure-cmake`
- **Standard CMake options**: Defined once in `configure-cmake`
- **Environment setup**: Centralized in `setup-build-env`

### 2. Consistency

- All workflows use identical configuration patterns
- Same preset detection across all builds
- Uniform directory structure handling

### 3. Reduced Duplication

- Eliminated ~100+ lines of duplicated bash code
- Preset detection logic written once, used everywhere
- Build commands standardized

### 4. Easier Updates

- To change container image: Update one place
- To add CMake option: Update `configure-cmake` inputs
- To modify preset logic: Update one action file

### 5. Better Readability

- Workflows are more declarative
- Intent is clearer (configure, then build)
- Less inline bash to parse

## Migration Path for Future Workflows

New workflows should follow this pattern:

```yaml
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
      build-type: <Release|Debug>
      extra-options: '<additional options>'

  - name: Build
    uses: ./phlex-src/.github/actions/build-cmake
    with:
      target: <optional target>
```

## Maintenance Guidelines

1. **Updating container image**: Edit container specification in each workflow (still needs updating in multiple places due to GitHub Actions limitation that composite actions cannot specify containers)

2. **Updating CMake options**: Edit `configure-cmake/action.yaml` inputs and defaults

3. **Updating preset logic**: Edit `configure-cmake/action.yaml` preset detection

4. **Updating build logic**: Edit `build-cmake/action.yaml`

5. **Testing changes**: Test composite action changes on feature branch before merging

## Future Enhancements

Potential future improvements:

1. Add composite action for test execution
2. Add composite action for artifact upload patterns
3. Create preset for coverage builds
4. Add validation checks to composite actions

## Documentation

Complete documentation available in `.github/actions/README.md`
