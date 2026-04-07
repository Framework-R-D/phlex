# Development Guidelines

## File Formatting — CRITICAL (applies to every file you create or edit)

- Files must end with exactly one newline (`\n`). No trailing blank lines, no
  trailing whitespace on any line (including blank lines within the file).
- The final character must be `\n`; the character before it must not be `\n`
  or a space/tab.

## Language-Specific Formatting

### C++

- clang-format (`.clang-format`): 100-char line limit, 2-space indentation
- clang-tidy (`.clang-tidy`): follow all recommendations
- Header files: `.hpp`; implementation files: `.cpp`
- Test files: `*_test.cpp` or descriptive names

### Python

- ruff for formatting and linting (`pyproject.toml`)
- Google-style docstrings; 99-char line limit; double quotes; type hints
- Test files: `test_*.py`
- Do not name files after standard library modules (e.g., avoid `types.py`)

### CMake

- gersemi or cmake-format: `dangle_align: "child"`, `dangle_parens: true`

### Jsonnet

- jsonnetfmt for formatting; CI enforces this
- Used for workflow configs in `test/` and elsewhere

### Markdown

- MD012: no multiple consecutive blank lines
- MD022: headings surrounded by one blank line
- MD031: fenced code blocks surrounded by one blank line
- MD032: lists surrounded by one blank line
- MD034: no bare URLs — use `[text](url)`
- MD036: use `#` headings, not `**Bold**` for titles
- MD040: always specify code block language

## Naming Conventions

### C++

- `lowercase_snake_case` for types, classes, functions, variables
- `UPPER_CASE` for macros and constants
- Namespaces: `phlex::` (core), `phlex::experimental::` (experimental)

### Python

- PEP 8; descriptive test function names

## Comments and Documentation

- Explain *why*, not *what* or *how* — code is self-documenting for those
- No temporal markers (`NEW:`, `CHANGED:`) — git history tracks changes
- Remove dead code; do not comment it out
- If an expected feature is absent, explain why (e.g., "Single-threaded
  context; locks unnecessary")

## Git Workflow

### Branch Creation

Do not set upstream tracking at branch creation time:

```bash
git checkout -b feature/my-feature          # no --track
git switch --no-track -c feature/my-feature # alternative
```

Set tracking only when pushing for the first time:

```bash
git push -u origin feature/my-feature
```

### Pull Requests

- Pass all CI checks; follow coding guidelines
- Minimize changes — keep PRs focused
- If changes affect `phlex-examples`, create an issue there (or in `phlex`
  if not possible), and notify the user
- If changes require documentation updates in `phlex-design`, create an issue
  there (or in `phlex` if not possible)
- If changes affect Spack recipes in `phlex-spack-recipes`, create an issue
  there (or in `phlex` if not possible)

## Build and Test Workflow

### Environment Setup

Always source `setup-env.sh` before building or testing. This applies in all
environments (devcontainer, local, HPC, CI containers):

```bash
# Standalone repository
. scripts/setup-env.sh

# Multi-project workspace (workspace root contains setup-env.sh)
. srcs/phlex/scripts/setup-env.sh
```

### Build

```bash
# Preferred: use presets
cmake --preset default -S . -B build
ninja -C build

# Or manually
cmake -B build -S . -DCMAKE_BUILD_TYPE=RelWithDebInfo -G Ninja
cmake --build build -j $(nproc)
```

### Test

```bash
ctest --test-dir build -j $(nproc) --output-on-failure
# With timeout guard (recommended):
ctest --test-dir build -j $(nproc) --test-timeout 90
# Run specific tests:
ctest --test-dir build -R "regex"
```

### Coverage

```bash
# Complete workflow
./scripts/coverage.sh all

# Step by step
./scripts/coverage.sh setup test html view

# Via CMake targets (after coverage preset build)
cmake --build build --target coverage-xml
cmake --build build --target coverage-html
```

### Pre-commit Hooks

```bash
# Detect available tool
PREKCOMMAND=$(command -v prek || command -v pre-commit || echo "")

# Install hooks (once per clone; automatic in devcontainer)
$PREKCOMMAND install

# Run all hooks against all files (recommended before opening a PR)
$PREKCOMMAND run --all-files

# Run against changed files only
$PREKCOMMAND run

# Update hook revisions
$PREKCOMMAND auto-update
```

`prek` is installed in `phlex-dev`; not in `phlex-ci`. Fall back to invoking
formatters directly if neither is available.

## Python Integration Details

- `PYTHONPATH` should include only paths containing user Python modules loaded
  by Phlex (source dir and build output dir). Do not append system/Spack/venv
  `site-packages`.
- Type conversion in `plugins/python/src/modulewrap.cpp` uses substring
  matching on type names — this is brittle. Ensure converters exist for all
  types used in tests. Exact matches required (`numpy.float32 != float`).
- Python test structure: C++ driver provides data streams, Jsonnet config
  wires the graph, Python script implements algorithms.

## Memory Management

- `std::shared_ptr` for shared ownership; `std::unique_ptr` for exclusive
- Raw pointers only for non-owning references
- Python/C++ interop: manual `Py_INCREF`/`Py_DECREF`; `PyGILRAII` for GIL
- GC-tracked Python types: `Py_TPFLAGS_HAVE_GC`, implement `tp_traverse` and
  `tp_clear`, call `PyObject_GC_UnTrack` before deallocation

## Error Handling

- C++: `std::runtime_error` for runtime failures; propagate Python exceptions
  as `std::runtime_error`
- Python C API: set exceptions with `PyErr_SetString`/`PyErr_Format`; return
  `nullptr` on error; clear with `PyErr_Clear()` when recovering

## External Software Versions

When specifying versions or commit hashes for external software (e.g., GitHub
Actions), always verify against the official release source:

1. Check existing workflows in `.github/workflows/` for the version currently
   in use
2. Verify whether a newer version exists at the official releases page
3. The authoritative source always takes precedence over training data

## Platform-Specific Code

```cpp
#if __linux__
constexpr double mem_denominator{1e3};
#else // AppleClang
constexpr double mem_denominator{1e6};
#endif
```

Use POSIX APIs where available; document platform requirements.

## Searching Source Directories

If the workspace root contains a `srcs/` directory with symbolic links, always
use `find -L` or `find -follow` to ensure linked directories are traversed.
