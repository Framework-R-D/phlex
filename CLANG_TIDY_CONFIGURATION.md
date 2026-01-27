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

**Purpose:** Automatically check C++ code for Core Guidelines compliance on pull requests

**Features:**

- Runs on PR to main branch and manual trigger
- Uses clang-tidy version 20 via CMake target
- Configures project and builds it first (required for accurate analysis)
- Uses CMake's `clang-tidy-check` target for consistency
- Reports warnings and errors with detailed output
- Uploads clang-tidy log as artifact on failure

**How it works:**

1. Configure the project with `CMAKE_EXPORT_COMPILE_COMMANDS=ON`
2. Build the project to ensure all generated files exist
3. Run `cmake --build . --target clang-tidy-check`
4. CMake automatically runs clang-tidy on all project source files

**How to use:**

- Automatically runs on every pull request
- Review the workflow output for details on any issues
- Comment `@phlexbot tidy-fix` to attempt automatic fixes

### Clang-Tidy Fix (`clang-tidy-fix.yaml`)

**Purpose:** Automatically apply clang-tidy fixes when triggered by comment

**Features:**

- Triggered by `@phlexbot tidy-fix` comment on PRs
- Uses clang-tidy version 20 via CMake target
- Builds project first, then applies fixes using CMake's `clang-tidy-fix` target
- Commits and pushes changes automatically
- Comments on PR with status

**How it works:**

1. Configure and build the project
2. Run `cmake --build . --target clang-tidy-fix`
3. CMake runs clang-tidy with `--fix --fix-errors` on all sources
4. Commit and push any changes

**Important notes:**

- Not all issues can be auto-fixed (some require manual intervention)
- The workflow will commit whatever clang-tidy can automatically fix
- Review auto-generated changes before merging
- Complex refactorings may need manual implementation

**How to use:**

1. Review clang-tidy check results
2. Comment `@phlexbot tidy-fix` on the PR
3. Bot will apply automatic fixes and commit
4. Review the changes and manually fix any remaining issues

## CMake Integration

The project provides CMake targets for clang-tidy analysis:

### Build-Time Integration

Enable clang-tidy during compilation:

```bash
cmake -DENABLE_CLANG_TIDY=ON /path/to/source
cmake --build .
```

When enabled, clang-tidy runs automatically on every C++ file during compilation, providing immediate feedback.

### CMake Targets

**`clang-tidy-check`** - Run clang-tidy on all project sources (read-only):

```bash
cmake --build . --target clang-tidy-check
```

**`clang-tidy-fix`** - Apply clang-tidy fixes to all project sources:

```bash
cmake --build . --target clang-tidy-fix
```

These targets:

- Use the `.clang-tidy` configuration file automatically
- Only analyze project source files (not dependencies)
- Work with the project's compile_commands.json
- Are generator-independent (work with Ninja, Make, etc.)

## Integration with Existing Workflows

The clang-tidy workflows complement the existing formatting workflows:

- **Format checks:** `@phlexbot format` - Fixes C++, CMake, and Jsonnet formatting
- **Tidy checks:** `@phlexbot tidy-fix` - Fixes Core Guidelines violations

Both can be run independently or together as needed.

## Local Usage

### Using CMake Targets (Recommended)

The project provides CMake targets for clang-tidy:

```bash
# Configure project (compile_commands.json is created automatically)
cmake --preset=default /path/to/source

# Build the project first
cmake --build . -j $(nproc)

# Run clang-tidy checks
cmake --build . --target clang-tidy-check

# Apply automatic fixes
cmake --build . --target clang-tidy-fix
```

### Build-Time Checks

Enable clang-tidy to run on every file during compilation:

```bash
cmake -DENABLE_CLANG_TIDY=ON /path/to/source
cmake --build .
```

This provides immediate feedback as you build, catching issues early.

### Manual Invocation

For manual control, you can run clang-tidy directly:

```bash
# Check a specific file
clang-tidy-20 -p /path/to/build file.cpp

# Apply fixes to a specific file
clang-tidy-20 -p /path/to/build --fix file.cpp

# Check all project files
find srcs/phlex -name "*.cpp" | \
  xargs clang-tidy-20 -p /path/to/build
```

## VS Code Integration

The project's `.clang-tidy` configuration is automatically used by:

- **clangd** (if configured as C++ language server)
- **C/C++ extension** with clang-tidy integration enabled

To enable real-time clang-tidy feedback in VS Code, add to `.vscode/settings.json`:

```json
"C_Cpp.codeAnalysis.clangTidy.enabled": true,
"C_Cpp.codeAnalysis.clangTidy.useBuildPath": true
```

## Customization

To adjust checks or settings, edit `.clang-tidy`:

1. Add/remove check categories in the `Checks:` section
2. Modify naming conventions in `CheckOptions`
3. Adjust complexity thresholds
4. Enable/disable specific rules

After changes, test locally before committing to ensure the configuration works as expected.

## References

- [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines)
- [Clang-Tidy Documentation](https://clang.llvm.org/extra/clang-tidy/)
- [Clang-Tidy Checks List](https://clang.llvm.org/extra/clang-tidy/checks/list.html)
