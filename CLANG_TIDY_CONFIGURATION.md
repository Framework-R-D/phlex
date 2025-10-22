# Clang-Tidy Configuration for Phlex

## Overview

This document describes the clang-tidy configuration and workflows for enforcing C++ Core Guidelines in the Phlex project.

## Configuration File: `.clang-tidy`

The project uses clang-tidy to enforce modern C++23 best practices and C++ Core Guidelines compliance.

### Enabled Check Categories

**Core Guidelines (`cppcoreguidelines-*`):**

- Enforces C++ Core Guidelines recommendations
- Ensures proper resource management (RAII)
- Validates special member functions (rule of five)
- Checks for proper use of const and constexpr
- **Disabled checks:**
  - `avoid-magic-numbers` - Too restrictive for scientific code
  - `avoid-c-arrays` - C arrays sometimes needed for interop
  - `pro-bounds-*` - Allow pointer arithmetic where necessary
  - `macro-usage` - Macros sometimes necessary
  - `non-private-member-variables-in-classes` - Allow public data members in simple structs

**Bug Prevention (`bugprone-*`):**

- Detects common programming errors
- Identifies suspicious constructs
- Catches potential undefined behavior
- **Disabled checks:**
  - `easily-swappable-parameters` - Too many false positives
  - `exception-escape` - Exception handling sometimes intentional

**Security (`cert-*`):**

- CERT C++ Secure Coding Standard compliance
- Security-focused checks for vulnerabilities

**Concurrency (`concurrency-*`):**

- Thread safety checks
- Race condition detection
- Mutex usage validation

**Modernization (`modernize-*`):**

- Suggests modern C++ features (auto, range-for, nullptr, etc.)
- Recommends C++23 idioms
- **Disabled checks:**
  - `use-trailing-return-type` - Not required for all functions

**Performance (`performance-*`):**

- Identifies inefficient code patterns
- Suggests const-ref parameters
- Detects unnecessary copies

**Portability (`portability-*`):**

- Cross-platform compatibility checks
- Ensures standard-conforming code

**Readability (`readability-*`):**

- Enforces consistent naming conventions
- Checks function complexity
- Validates identifier clarity
- **Disabled checks:**
  - `function-cognitive-complexity` - Too restrictive
  - `identifier-length` - Short names acceptable in context
  - `magic-numbers` - Duplicate of cppcoreguidelines check

**Static Analysis (`clang-analyzer-*`):**

- Deep static analysis of code
- Control flow analysis
- Null pointer dereference detection

### Naming Conventions

The configuration enforces consistent naming:

- **Namespaces:** `lower_case`
- **Classes/Structs/Enums:** `CamelCase`
- **Functions:** `lower_case`
- **Variables/Parameters:** `lower_case`
- **Private/Protected Members:** `m_` prefix + `lower_case`
- **Constants/Enum Values:** `UPPER_CASE`
- **Type Aliases/Typedefs:** `CamelCase`
- **Template Parameters:** `CamelCase`

### Function Complexity Limits

- **Line Threshold:** 100 lines per function
- **Statement Threshold:** 50 statements per function
- **Branch Threshold:** 10 branches per function
- **Parameter Threshold:** 6 parameters per function

## GitHub Actions Workflows

### Clang-Tidy Check (`clang-tidy-check.yaml`)

**Purpose:** Automatically check C++ code for Core Guidelines compliance on pull requests.

**Features:**

- Runs on pull requests to the `main` branch and can be triggered manually.
- Uses a containerized environment with clang-tidy version 20.
- Intelligently detects changes to C++ and CMake files to avoid unnecessary runs.
- Configures and builds the project before running the checks to ensure accuracy.
- Uses the `clang-tidy-check` CMake target for consistency.
- Reports warnings and errors with detailed output, including a summary of the most common issues.
- Uploads the full clang-tidy log as a build artifact on failure.

**How it works:**

1.  The workflow first checks for any changes to C++, CMake, or workflow files. If no relevant changes are detected, the job is skipped.
2.  The project is configured with `CMAKE_EXPORT_COMPILE_COMMANDS=ON` to generate the compilation database.
3.  The `clang-tidy-check` CMake target is executed, which runs clang-tidy on all project source files.
4.  If clang-tidy finds any issues, the workflow fails and provides a summary of the errors.

**How to use:**

- The workflow runs automatically on every pull request.
- Review the workflow output for details on any issues.
- To attempt to automatically fix the issues, comment `@phlexbot tidy-fix` on the pull request.

### Clang-Tidy Fix (`clang-tidy-fix.yaml`)

**Purpose:** Automatically apply clang-tidy fixes to a pull request when triggered by a comment.

**Features:**

- Triggered by commenting `@phlexbot tidy-fix` on a pull request.
- Can be triggered with a specific set of checks to apply (e.g., `@phlexbot tidy-fix readability-identifier-naming`).
- Uses the same containerized environment as the check workflow.
- Applies fixes using the `clang-tidy-fix` CMake target.
- Commits and pushes the changes back to the pull request branch.
- Posts a comment on the pull request to indicate the status of the fixes.

**How it works:**

1.  When triggered, the workflow checks out the pull request branch.
2.  The project is configured, and the `clang-tidy-fix` CMake target is executed.
3.  This target runs clang-tidy with the `--fix` and `--fix-errors` flags, which automatically corrects any issues that can be fixed automatically.
4.  The changes are then committed and pushed to the branch.

**Important notes:**

- Not all issues can be fixed automatically. Some may require manual intervention.
- Always review the changes made by the bot before merging the pull request.

## CMake Integration

The project provides CMake targets for running clang-tidy locally. These targets are defined in `Modules/private/CreateClangTidyTargets.cmake`.

### Source File Discovery

The `CreateClangTidyTargets.cmake` script automatically discovers the C++ source files to be analyzed. It does this by recursively gathering all build system targets and then collecting the source files associated with those targets.

The script includes the following features:

-   **Recursive Target Discovery:** It traverses the entire directory structure to find all defined targets.
-   **Optional Inclusion of Tests and FORM:** You can control whether to include test and FORM targets in the analysis by setting the `CLANG_TIDY_INCLUDE_TESTS` and `CLANG_TIDY_INCLUDE_FORM` CMake options.
-   **Exclusion of Generated Files:** The script automatically excludes generated C++ files and rootcling dictionary files from the analysis.
-   **Use of `run-clang-tidy`:** If the `run-clang-tidy` script is available, it will be used to run clang-tidy in parallel, which can significantly speed up the analysis.

### Build-Time Integration

You can enable clang-tidy to run automatically on every C++ file during compilation by setting the `ENABLE_CLANG_TIDY` CMake option:

```bash
cmake -DENABLE_CLANG_TIDY=ON /path/to/source
cmake --build .
```

This provides immediate feedback as you build, helping you to catch issues early.

### CMake Targets

The following CMake targets are available for running clang-tidy manually:

-   **`clang-tidy-check`**: Runs clang-tidy on all project source files in read-only mode.
    ```bash
    cmake --build . --target clang-tidy-check
    ```
-   **`clang-tidy-fix`**: Applies clang-tidy fixes to all project source files.
    ```bash
    cmake --build . --target clang-tidy-fix
    ```

These targets use the `.clang-tidy` configuration file automatically and work with the project's `compile_commands.json` file.

## Local Usage

### Using CMake Targets (Recommended)

The recommended way to run clang-tidy locally is to use the CMake targets:

```bash
# Configure the project
cmake --preset=default /path/to/source

# Build the project first
cmake --build . -j $(nproc)

# Run clang-tidy checks
cmake --build . --target clang-tidy-check

# Apply automatic fixes
cmake --build . --target clang-tidy-fix
```

### Manual Invocation

For more control, you can also run clang-tidy directly:

```bash
# Check a specific file
clang-tidy-20 -p /path/to/build file.cpp

# Apply fixes to a specific file
clang-tidy-20 -p /path/to/build --fix file.cpp
```

## VS Code Integration

The project's `.clang-tidy` configuration is automatically used by the following VS Code extensions:

-   **clangd** (if configured as the C++ language server)
-   **C/C++ extension** (with clang-tidy integration enabled)

To enable real-time clang-tidy feedback in VS Code, add the following to your `.vscode/settings.json` file:

```json
"C_Cpp.codeAnalysis.clangTidy.enabled": true,
"C_Cpp.codeAnalysis.clangTidy.useBuildPath": true
```

## Customization

To adjust the clang-tidy checks or settings, edit the `.clang-tidy` file:

1.  Add or remove check categories in the `Checks:` section.
2.  Modify naming conventions in the `CheckOptions` section.
3.  Adjust the function complexity thresholds.
4.  Enable or disable specific rules.

After making any changes, be sure to test them locally before committing.

## References

-   [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines)
-   [Clang-Tidy Documentation](https://clang.llvm.org/extra/clang-tidy/)
-   [Clang-Tidy Checks List](https://clang.llvm.org/extra/clang-tidy/checks/list.html)
