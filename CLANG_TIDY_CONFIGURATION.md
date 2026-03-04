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
- **Classes/Structs/Enums:** `lower_case`
- **Functions:** `lower_case`
- **Variables/Parameters:** `lower_case`
- **Private/Protected/Constant Members:** `lower_case` with `_` suffix
- **Macros:** `UPPER_CASE`
- **Constants/Enum Values:** `lower_case`
- **Type Aliases/Typedefs:** `lower_case`
- **Template Parameters:** `CamelCase`

### Function Complexity Limits

- **Line Threshold:** 100 lines per function
- **Statement Threshold:** 50 statements per function
- **Branch Threshold:** 10 branches per function

## GitHub Actions Workflows

### Clang-Tidy Check (`clang-tidy-check.yaml`)

**Purpose:** Automatically check C++ code for Core Guidelines compliance on pull requests

**Features:**

- Runs on PR to main branch and manual trigger
- Uses clang-tidy via `CMAKE_CXX_CLANG_TIDY` during build
- Configures project with compile commands export
- Reports warnings and errors with detailed output
- Uploads clang-tidy fixes YAML and log as artifacts
- Posts inline PR comments for detected issues

**How it works:**

1. Configure the project with `CMAKE_EXPORT_COMPILE_COMMANDS=ON` and `CMAKE_CXX_CLANG_TIDY='clang-tidy;--export-fixes=clang-tidy-fixes.yaml'`
2. Build the project - clang-tidy runs automatically on each source file during compilation
3. Parse output and upload artifacts
4. Post inline comments on PR with issue details

**How to use:**

- Automatically runs on every pull request
- Review the workflow output and inline PR comments for details
- Comment `@phlexbot tidy-fix` to attempt automatic fixes

### Clang-Tidy Fix (`clang-tidy-fix.yaml`)

**Purpose:** Automatically apply clang-tidy fixes when triggered by comment

**Features:**

- Triggered by `@phlexbot tidy-fix [<check>...]` comment on PRs
- Optionally specify specific checks to fix
- Attempts to reuse existing fixes from check workflow artifacts
- Falls back to running clang-tidy with `CMAKE_CXX_CLANG_TIDY` if needed
- Applies fixes using `clang-apply-replacements`
- Commits and pushes changes automatically
- Posts inline PR comments with remaining issues

**How it works:**

1. Try to download existing `clang-tidy-fixes.yaml` from check workflow
2. If not available, configure and build with clang-tidy to generate fixes
3. Apply fixes using `clang-apply-replacements`
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

### Build-Time Integration

Enable clang-tidy during compilation using `CMAKE_CXX_CLANG_TIDY`:

```bash
cmake -DCMAKE_CXX_CLANG_TIDY='clang-tidy' -B build -S .
cmake --build build
```

When enabled, clang-tidy runs automatically on every C++ file during compilation, providing immediate feedback.

To export fixes for later application:

```bash
cmake -DCMAKE_CXX_CLANG_TIDY='clang-tidy;--export-fixes=clang-tidy-fixes.yaml' -B build -S .
cmake --build build
clang-apply-replacements build
```

Note: The project sets `CMAKE_EXPORT_COMPILE_COMMANDS=ON` by default, which generates `compile_commands.json` for clang-tidy to use.

## Integration with Existing Workflows

The clang-tidy workflows complement the existing formatting workflows:

- **Format checks:** `@phlexbot format` - Fixes C++, CMake, Jsonnet, and Markdown formatting
- **Tidy checks:** `@phlexbot tidy-fix` - Fixes Core Guidelines violations

Both can be run independently or together as needed.

## Local Usage

### Build-Time Checks (Recommended)

Enable clang-tidy to run on every file during compilation:

```bash
cmake -DCMAKE_CXX_CLANG_TIDY='clang-tidy' -B build -S .
cmake --build build -j $(nproc)
```

This provides immediate feedback as you build, catching issues early.

### Generate and Apply Fixes

To generate fixes and apply them:

```bash
# Configure with fix export
cmake -DCMAKE_CXX_CLANG_TIDY='clang-tidy;--export-fixes=clang-tidy-fixes.yaml' -B build -S .

# Build (generates fixes)
cmake --build build -j $(nproc)

# Apply fixes
clang-apply-replacements build
```

### Manual Invocation

For manual control, you can run clang-tidy directly:

```bash
# Check a specific file
clang-tidy -p build phlex/core/framework_graph.cpp

# Apply fixes to a specific file
clang-tidy -p build --fix phlex/core/framework_graph.cpp

# Check all project files
find phlex -name "*.cpp" | xargs clang-tidy -p build
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
