# Technology Stack

## Languages

- **C++23** — core framework and performance-critical components
- **Python 3.12+** — user algorithms, testing, configuration scripts
- **Jsonnet** — workflow configuration files (`.jsonnet`)
- **CMake 3.31+** — build system

## Compilers

- GCC 14+ (primary)
- Clang (sanitizer support)
- AppleClang (macOS)

## Core Dependencies

| Library | Purpose |
| ------- | ------- |
| Intel TBB | Parallel execution engine |
| Boost (json, program_options) | JSON parsing, CLI |
| fmt | C++ string formatting |
| spdlog | Structured logging |
| jsonnet | Config file parsing |
| cetmodules 4.01.01 | Fermilab CMake modules |

## Test Frameworks

| Tool | Purpose |
| ---- | ------- |
| Catch2 v3.10.0 | C++ unit tests (FetchContent) |
| mimicpp v8 | C++ mocking (FetchContent) |
| Microsoft GSL v4.2.0 | Core Guidelines support (FetchContent) |
| pytest + pytest-cov | Python tests |

## Build Options

```cmake
PHLEX_USE_FORM       # FORM/ROOT integration (default ON in presets)
ENABLE_TSAN          # Thread Sanitizer
ENABLE_ASAN          # Address Sanitizer
ENABLE_COVERAGE      # Code coverage
ENABLE_CLANG_TIDY    # Static analysis during build
ENABLE_PERFETTO      # Perfetto profiling
```

## CMake Presets

Defined in `CMakePresets.json`. `binaryDir` is not specified in the presets file
to give developers flexibility in choosing build locations. In devcontainer
environments, `cmake.buildDirectory` is set to `build/` in the VS Code workspace
settings, and `CMAKE_GENERATOR=Ninja` is set as a container environment variable.
Pass `-B <dir>` explicitly on the command line.

- `default` — standard build with FORM enabled, C++23, compile-commands export
- `coverage-gcc` — GCC/gcov/lcov coverage build
- `coverage-clang` — Clang/llvm-cov coverage build
- `clang-tidy` — inherits `default`; sets `CMAKE_CXX_SCAN_FOR_MODULES=OFF` for clang-tidy compatibility

## Code Quality Tools

| Tool | Config file | Purpose |
| ---- | ----------- | ------- |
| clang-format | `.clang-format` | C++ formatting |
| clang-tidy | `.clang-tidy` | C++ static analysis |
| ruff | `pyproject.toml` | Python lint + format |
| gersemi | `.gersemirc` | CMake formatting |
| cmake-format | `.cmake-format.json` | CMake formatting (alternative) |
| markdownlint-cli2 | `.markdownlint.jsonc` | Markdown linting |
| prettier | `.prettierrc.yaml` | YAML formatting |
| actionlint | `.github/actionlint.yaml` | GitHub Actions linting |
| jsonnetfmt / jsonnet-lint | (pre-commit) | Jsonnet formatting + linting |

## Pre-commit Hooks

Configured in `.pre-commit-config.yaml`. Runs: trailing-whitespace/EOF fixers,
ruff, clang-format, gersemi, jsonnet-format/lint, prettier, markdownlint,
and a custom header-guard fixer (`scripts/fix_header_guards.py`).

`prek` (Rust drop-in for `pre-commit`) is installed in `phlex-dev`. Host
developers may have `pre-commit` instead. Detect which is available:

```bash
PREKCOMMAND=$(command -v prek || command -v pre-commit || echo "")
```

Then use `$PREKCOMMAND` in place of the tool name for all hook commands.

## Coverage

- **GCC path**: gcov → gcovr (XML) / lcov + genhtml (HTML)
- **Clang path**: llvm-cov source-based coverage
- CMake targets: `coverage-xml`, `coverage-html`, `coverage-clean`
- Automated upload to Codecov in CI
- Path normalization via `scripts/normalize_coverage_xml.py` and
  `scripts/normalize_coverage_lcov.py`

## Package Management

Spack is the primary distribution method. Recipes live in
`Framework-R-D/phlex-spack-recipes`. The `scripts/setup-env.sh` script
activates Spack environments automatically when available and degrades
gracefully to system packages.

## Environment Variables

| Variable | Purpose |
| -------- | ------- |
| `PHLEX_SPACK_ROOT` | Custom Spack installation path |
| `PHLEX_SPACK_ENV` | Specific Spack environment name |
| `PHLEX_BUILD_DIR` | Custom build directory |
| `CMAKE_BUILD_TYPE` | Build type override |
| `PHLEX_PLUGIN_PATH` | Plugin search path (test-time) |
| `PYTHONPATH` | Python module search path (test-time) |
| `SPDLOG_LEVEL` | Logging level (debug/info/warn/error) |

## Local GitHub Actions Testing

Use `act` to run workflows locally. `.actrc` at the repo root contains the
full configuration — do not overwrite it. Inside the devcontainer, `DOCKER_HOST`
is set automatically. On the host, activate the rootless Podman socket first:

```bash
systemctl --user enable --now podman.socket
export DOCKER_HOST=unix://${XDG_RUNTIME_DIR}/podman/podman.sock
```

## Jules AI Agent Environment

`dev/jules/` contains tooling for the Jules AI agent:

- `dev/jules/Dockerfile` — Ubuntu 24.04 image with non-root user `jules`
- `dev/jules/prepare-environment.sh` — environment preparation script

To test the Jules environment locally:

```bash
podman build -t phlex-jules-dev -f dev/jules/Dockerfile .
podman run -it --rm --volume "$(pwd):/app" phlex-jules-dev /bin/bash
dev/jules/prepare-environment.sh
```

When assigning tasks to Jules, instruct it to run
`dev/jules/prepare-environment.sh` before starting work.
