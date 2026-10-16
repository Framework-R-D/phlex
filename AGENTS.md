# Phlex Agent Instructions

If there is a conflict between this guidance and information from current, authoritative sources
(e.g. in this or below-referenced repositories), the latter governs.

## Project

Phlex is a graph-based framework for parallel, hierarchical data processing. It provides execution
and data-product protocols; experiments and toolkits define their own product types and data-layer
models. The core is C++23, the Python extension requires Python 3.12, and workflow configurations
use Jsonnet. CMake requires 3.31 and uses `cetmodules`.

Core code is in `phlex/`; FORM persistence is in `form/`; plugins are in `plugins/`; tests are in
`test/`; developer tools are in `scripts/`.

In the devcontainer, related repositories are mounted under `/workspaces`: `phlex-design` has
design documentation, `phlex-coding-guidelines` has contributor standards, `phlex-examples` has
consumer examples, and `phlex-spack-recipes` has packaging recipes. The host layout may differ.
Check these repositories when a change affects their respective interfaces or content.

Compiler versions and dependency stacks vary by platform and environment. Follow the active CMake
preset, Spack environment, or CI image; do not assume one GCC or Clang version applies everywhere.

## Environment, Build, and Test

The devcontainer activates its Spack environment for interactive shells; use its tools directly.
Outside it, source `scripts/setup-env.sh` when repository-specific environment setup is needed, and
verify the compiler and dependencies selected for the target platform.

`CMakePresets.json` has no `binaryDir`, so pass `-B` explicitly:

```bash
cmake --preset default -B build
cmake --build build --parallel
ctest --test-dir build --output-on-failure --parallel
```

The default preset enables FORM. Other available presets include `clang-tidy`, `coverage-gcc`, and
`coverage-clang`. CTest's configured per-test timeout is 90 seconds. Run focused C++ tests with
`ctest --test-dir build -R '<regex>'`; run the scripts unit tests with `pytest scripts/test/`.
For the full local coverage workflow, use `./scripts/coverage.sh all`.

The devcontainer provides `prek`; outside it, use `pre-commit` if installed. The hook configuration
in `.pre-commit-config.yaml` is the source of truth; run the available tool with `run --all-files`
before broad changes.

## Conventions and Integration

Use `.clang-format`, `.clang-tidy`, `pyproject.toml`, and `.pre-commit-config.yaml` as the sources
of truth for formatting and linting. C++ naming is lower snake case, except `CamelCase` template
parameters and `UPPER_CASE` macros; east-const and the configured trailing member underscores
apply. Python uses Ruff's 99-character limit, double quotes, and Google-style docstrings.

CI checks for newly introduced clang-tidy findings in changed C/C++ code. `WarningsAsErrors` is
empty in `.clang-tidy`, so this is a delta check rather than a global warnings-as-errors build.

When changing Python bindings, inspect `plugins/python/src/modulewrap.cpp`: collection conversions
are hard-coded and need corresponding tests when extended. Follow the established reference-counting
and `py_gilraii` patterns in `plugins/python/src/wrap.hpp`. Keep `PYTHONPATH` limited to user modules;
do not append system, Spack, or virtual-environment `site-packages`.
