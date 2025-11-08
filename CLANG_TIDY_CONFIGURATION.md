# Clang-Tidy Configuration for Phlex

## Overview

Phlex uses `clang-tidy` to enforce modern C++23 practices across both the `phlex` framework and the FORM libraries. The canonical rules live in the repository root `.clang-tidy` file and are executed with `clang-tidy-20` inside the `ghcr.io/framework-r-d/phlex-ci` container.

## Configuration Highlights

### Enabled Check Families

The configuration enables the following check families (with targeted exclusions for noisy diagnostics):

- `bugprone-*` (except `bugprone-easily-swappable-parameters`, `bugprone-exception-escape`)
- `cert-*`
- `clang-analyzer-*`
- `concurrency-*`
- `cppcoreguidelines-*` (disabling items such as `avoid-magic-numbers`, `avoid-c-arrays`, and `macro-usage` to accommodate framework requirements)
- `misc-*` (excluding `misc-non-private-member-variables-in-classes` and `misc-no-recursion`)
- `modernize-*` (excluding `modernize-use-trailing-return-type` and `modernize-use-nodiscard`)
- `performance-*`
- `portability-*`
- `readability-*` (excluding checks like `function-cognitive-complexity`, `identifier-length`, `magic-numbers`, `named-parameter`, and `implicit-bool-conversion`)

Every other check is disabled by default, keeping the signal-to-noise ratio high while still catching defects.

### Naming Rules

Identifier style is enforced through `readability-identifier-naming` options. Key rules include:

- Namespaces, classes, structs, enums, functions, variables, parameters, constants, and typedefs all use `lower_case`.
- Macro definitions remain `UPPER_CASE`.
- Template parameters use `CamelCase` to mirror standard library conventions.
- Private and protected members keep a trailing underscore (`foo_`).

### Complexity Thresholds

`readability-function-size` protects against overly complex functions with the following limits:

- 100 lines
- 50 statements
- 10 branches
- 6 parameters

### Header Coverage

`HeaderFilterRegex` restricts diagnostics to headers under `form/` and `phlex/`, preventing third-party code from triggering warnings.

## GitHub Actions Integration

Two reusable workflows keep clang-tidy results current:

- **`clang-tidy-check.yaml`** runs automatically on pull requests targeting `main` (and on demand). It uses the repository’s composite actions to source `/entrypoint.sh`, configure CMake with presets, build the code, and execute the `clang-tidy-check` CMake target. Results are summarized and log artifacts are uploaded on failure.
- **`clang-tidy-fix.yaml`** responds to the `@phlexbot tidy-fix` comment. It checks out the PR branch, reuses the same containerised toolchain, runs the `clang-tidy-fix` CMake target, commits fixes (when any were produced), and posts status back to the pull request.

Both workflows avoid unnecessary work by filtering for C++/CMake changes before building.

## CMake Support

Helper targets defined in `Modules/private/CreateClangTidyTargets.cmake` make it easy to run clang-tidy locally:

- `clang-tidy-check` — analyzes all sources without modifying them.
- `clang-tidy-fix` — applies automatic fixes produced by clang-tidy.

The helper module automatically discovers project targets, skips generated sources, and uses `run-clang-tidy` when it is available to parallelise analysis.

To enable clang-tidy during every build you can opt into:

```bash
cmake -DENABLE_CLANG_TIDY=ON /path/to/source
cmake --build .
```

## Local Usage

1. Configure with presets (recommended):

   ```bash
   cmake --preset default -S /path/to/phlex -B /path/to/build
   ```

2. Build once so the compilation database is complete:

   ```bash
   cmake --build /path/to/build -j $(nproc)
   ```

3. Run clang-tidy:

   ```bash
   cmake --build /path/to/build --target clang-tidy-check
   ```

4. Apply automatic fixes when desired:

   ```bash
   cmake --build /path/to/build --target clang-tidy-fix
   ```

For ad-hoc analysis, you can target individual files:

```bash
clang-tidy-20 -p /path/to/build path/to/file.cpp
clang-tidy-20 -p /path/to/build --fix path/to/file.cpp
```

## IDE Integration

Both the VS Code C/C++ extension and `clangd` honour the repository `.clang-tidy` automatically when pointed at the build directory. To ensure live diagnostics in VS Code:

```json
"C_Cpp.codeAnalysis.clangTidy.enabled": true,
"C_Cpp.codeAnalysis.clangTidy.useBuildPath": true
```

## Customising the Ruleset

Adjustments are made directly in `.clang-tidy`:

1. Update the `Checks` list to enable or disable diagnostics.
2. Modify `readability-identifier-naming` options to change naming rules.
3. Tweak complexity thresholds if different limits are required.

Always validate changes locally (for example with `clang-tidy-check.yaml` via `act` or by running the CMake targets) before committing.

## References

- [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines)
- [Clang-Tidy Documentation](https://clang.llvm.org/extra/clang-tidy/)
- [Clang-Tidy Checks List](https://clang.llvm.org/extra/clang-tidy/checks/list.html)
