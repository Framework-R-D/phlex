# Development Guidelines

## Code Quality Standards

### File Formatting Requirements

**CRITICAL: All text files must follow these rules:**

- Files must end with exactly one newline character (`\n`)
- No trailing blank lines at end of file (no `\n\n` at EOF)
- No trailing whitespace on any line (no spaces or tabs at line endings)
- Blank lines within files should contain only a newline, no spaces/tabs

### Language-Specific Formatting

**C++ Files:**

- Use clang-format (configured in `.clang-format`)
- 100-character line limit
- 2-space indentation
- Follow clang-tidy recommendations (`.clang-tidy`)
- CI automatically checks and provides fixes

**Python Files:**

- Use ruff for formatting and linting (`pyproject.toml`)
- Google-style docstrings
- 99-character line limit
- Double quotes for strings
- Type hints recommended (mypy configured)

**CMake Files:**

- Use cmake-format or gersemi
- `dangle_align: "child"`, `dangle_parens: true`
- Auto-format on save in VS Code

**Markdown Files:**

- Follow markdownlint rules (`.markdownlint.jsonc`)
- No multiple consecutive blank lines (MD012)
- Headings surrounded by one blank line (MD022)
- Code blocks surrounded by one blank line (MD031)
- Lists surrounded by one blank line (MD032)
- No bare URLs - use markdown links (MD034)
- Always specify code block language (MD040)
- Use proper headings instead of bold for section titles (MD036)
  - WRONG: **Tool Name** followed by description
  - RIGHT: #### Tool Name followed by description

### Naming Conventions

**C++ Conventions:**

- lowercase_snake_case for types and classes
- lowercase_snake_case for functions and variables
- UPPER_CASE for macros and constants
- Namespace: `phlex::` for core, `phlex::experimental::` for experimental features

**Python Conventions:**

- Avoid shadowing standard library names (e.g., don't name files `types.py`)
- Follow PEP 8 naming conventions
- Use descriptive names for test functions

**File Naming:**

- Header files: `.hpp` (not `.h`)
- Implementation files: `.cpp`
- Test files: `*_test.cpp` or `test_*.py`
- Configuration files: `.jsonnet` for Phlex workflows

## Architectural Patterns

### Memory Management

**Smart Pointers:**

- Use `std::shared_ptr` for shared ownership
- Use `std::unique_ptr` for exclusive ownership
- Raw pointers only for non-owning references
- Example from codebase:

  ```cpp
  std::shared_ptr<std::vector<cpptype>> vec = std::make_shared<std::vector<cpptype>>();
  ```

**Python/C++ Interop:**

- Manual reference counting with `Py_INCREF`/`Py_DECREF`
- RAII wrapper `PyGILRAII` for GIL management
- Lifeline objects tie Python views to C++ shared_ptr ownership
- Example pattern:

  ```cpp
  PyGILRAII gil;  // Acquire GIL
  Py_INCREF(m_callable);
  // ... use Python objects ...
  Py_DECREF(result);
  ```

### Error Handling

**C++ Exceptions:**

- Use `std::runtime_error` for runtime failures
- Propagate Python exceptions to C++ as `std::runtime_error`
- Example:

  ```cpp
  if (!result) {
    if (!msg_from_py_error(error_msg))
      error_msg = "Unknown python error";
    throw std::runtime_error(error_msg.c_str());
  }
  ```

**Python Error Handling:**

- Set Python exceptions with `PyErr_SetString`, `PyErr_Format`
- Return `nullptr` from functions on error (Python C API convention)
- Clear errors with `PyErr_Clear()` when recovering

### Type System Patterns

**Template Metaprogramming:**

- Use `static_assert` for compile-time validation
- Template specialization for type-specific behavior
- Fold expressions for variadic templates: `(Py_DECREF((PyObject*)args), ...)`

**Type Conversion:**

- Explicit converter functions for C++/Python interop
- Macro-based converter generation (`BASIC_CONVERTER`, `VECTOR_CONVERTER`)
- Type annotation parsing from Python `__annotations__`

**Type Safety:**

- Use `intptr_t` for opaque Python object pointers
- Type erasure for heterogeneous collections
- Product specification system for type-safe data flow

## Common Implementation Patterns

### Python C API Patterns

**PyTypeObject Definition:**

- Use clang-format off/on for struct initialization
- Initialize all fields explicitly
- Version-conditional fields for Python compatibility:

  ```cpp
  // clang-format off
  PyTypeObject Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    (char*)"module.name",  // tp_name
    sizeof(struct_type),   // tp_basicsize
    // ... all fields ...
  #if PY_VERSION_HEX >= 0x03080000
    , 0  // tp_vectorcall
  #endif
  };
  // clang-format on
  ```

**Garbage Collection:**

- Use `Py_TPFLAGS_HAVE_GC` for GC-tracked types
- Implement `tp_traverse` and `tp_clear`
- Call `PyObject_GC_UnTrack` before deallocation
- Use `tp_free` to pair with `tp_alloc`

### Resource Management

**RAII Pattern:**

- Constructor acquires resources
- Destructor releases resources
- Example from resource_usage.cpp:

  ```cpp
  resource_usage::resource_usage() noexcept :
    begin_wall_{steady_clock::now()}, begin_cpu_{get_rusage().elapsed_time}
  {}

  resource_usage::~resource_usage() {
    // Log metrics on destruction
  }
  ```

**Structured Bindings:**

- Use C++17 structured bindings for clarity:

  ```cpp
  auto const [elapsed_time, max_rss] = get_rusage();
  auto const [secs, microsecs] = used.ru_utime;
  ```

### String Handling

**String Building:**

- Use `std::ostringstream` for concatenation
- Use `fmt` library for formatting
- Use `spdlog` for logging with formatting

**Path Handling (Python):**

- Use `pathlib.Path` for path operations
- Use `as_posix()` for cross-platform paths
- Use `resolve()` for canonical paths
- Handle both absolute and relative paths
- Example pattern:

  ```python
  relative = _relative_subpath(path, base)
  if relative is None:
      relative = _relative_subpath(path.resolve(), base.resolve())
  ```

## Testing Patterns

### Test Organization

**C++ Tests:**

- Use Catch2 framework
- Test files in `test/` directory
- Naming: `*_test.cpp` or descriptive names

**Python Tests:**

- Use pytest framework
- Test files: `test_*.py`
- Jsonnet configs wire the test graph
- C++ drivers provide data sources

**Test Structure:**

- Unit tests for individual components
- Integration tests in subdirectories (`test/python/`, `test/form/`)
- Benchmark tests in `test/benchmarks/`
- Mock workflows in `test/mock-workflow/`

### Coverage Testing

**Configuration:**

- Enable with `-DCMAKE_BUILD_TYPE=Coverage -DENABLE_COVERAGE=ON`
- Use `gcov` for C++ coverage
- Use `pytest-cov` for Python coverage

**Targets:**

- `coverage-xml`: XML report for CI/Codecov
- `coverage-html`: HTML report for local viewing
- `coverage-clean`: Clean coverage data

**Path Normalization:**

- Handle generated files via symlink trees
- Normalize paths for Codecov compatibility
- Use `normalize_coverage_xml.py` script

## Build System Patterns

### CMake Conventions

**Target Definition:**

- Use modern CMake (3.31+)
- Prefer `target_link_libraries` over global settings
- Use `PRIVATE`, `PUBLIC`, `INTERFACE` keywords appropriately

**Dependency Management:**

- FetchContent for test frameworks (Catch2, mimicpp, GSL)
- find_package for external libraries (Boost, TBB, fmt)
- Cetmodules for HEP-specific packaging

**Build Options:**

- Use `option()` for user-configurable features
- Sanitizers: `ENABLE_TSAN`, `ENABLE_ASAN`
- Coverage: `ENABLE_COVERAGE`
- Optional features: `PHLEX_USE_FORM`, `ENABLE_PERFETTO`

### Environment Setup

**setup-env.sh Integration:**

- Source before build/test commands
- Handles Spack environments automatically
- Gracefully degrades to system packages
- Multi-mode support: standalone, workspace, Spack

**Spack Integration:**

- Use `spack load` for additional tools
- Recipes in `phlex-spack-recipes` repository
- Environment activation via setup script

## Documentation Standards

### External Software Versions and Hashes

**CRITICAL: Always Verify Against Official Sources:**

- When specifying external software versions or commit hashes (e.g., GitHub Actions, dependencies), ALWAYS verify against the official release source
- First, check what the LATEST version is from the project's releases page (e.g., `https://github.com/owner/repo/releases`)
- Then verify the hash for that latest version at the specific release tag page (e.g., `https://github.com/owner/repo/releases/tag/vX.Y.Z`)
- The authoritative source ALWAYS takes precedence over training data
- Training data may be outdated or incorrect - never trust it for version/hash information without verification
- Example: For `actions/setup-node`, first check `https://github.com/actions/setup-node/releases` for latest version, then verify hash at the specific tag page

### Comment Guidelines

**Explain "Why", Not "What":**

- Code should be self-documenting for what/how
- Comments explain rationale and design decisions
- Example:

  ```cpp
  // TODO: cleanup deferred to Phlex shutdown hook
  // Cannot safely Py_DECREF during arbitrary destruction due to:
  // - TOCTOU race on Py_IsInitialized() without GIL
  // - Module offloading in interpreter cleanup phase 2
  ```

**Avoid Temporal Comments:**

- No "NEW:" or "CHANGED:" markers
- Git history tracks changes
- Comments describe current state

**Remove Dead Code:**

- Don't comment out unused code
- Delete it - Git preserves history

**Explain Absences:**

- If expected feature is missing, explain why
- Example: "Single-threaded context; locks unnecessary"

### Docstring Conventions

**Python:**

- Google-style docstrings
- Include Args, Returns, Raises sections
- Type hints in function signatures
- Example:

  ```python
  def normalize(
      report_path: Path,
      repo_root: Path,
      *,
      coverage_root: Path | None = None,
  ) -> tuple[list[str], list[str]]:
      """Normalize filenames within a Cobertura XML report.
      
      Args:
          report_path: Path to the Cobertura XML.
          repo_root: Root of the repository.
          coverage_root: Directory gcovr treated as root.
      
      Returns:
          A tuple containing missing files and external files.
      """
  ```

## Git Workflow

### Branch Management

**Creating Branches:**

- Don't set upstream tracking at creation
- Use `git checkout -b new-branch` or `git switch --no-track -c new-branch`
- Prevents accidental pushes to base branch

**Pushing Branches:**

- Use `git push -u origin branch-name` only when ready
- Sets tracking only on first push

### Pull Request Guidelines

**Quality Standards:**

- Pass all CI checks
- Follow coding guidelines
- Update documentation if needed
- Create issues in related repos if changes affect them

**Minimize Changes:**

- Keep PRs focused and minimal
- Separate refactoring from feature changes
- Update container configs if needed

## Platform-Specific Considerations

### Cross-Platform Code

**Conditional Compilation:**

- Use `#if __linux__` for Linux-specific code
- Use `#else // AppleClang` for macOS alternatives
- Example:

  ```cpp
  #if __linux__
  constexpr double mem_denominator{1e3};
  #else // AppleClang
  constexpr double mem_denominator{1e6};
  #endif
  ```

**System APIs:**

- Use POSIX APIs where available
- Provide platform-specific implementations
- Document platform requirements

### Compiler Compatibility

**Supported Compilers:**

- GCC 14+ (primary)
- Clang (with sanitizer support)
- AppleClang (macOS)

**Compiler Workarounds:**

- GCC 14-16 specific flags in CMakeLists.txt
- Version-conditional warnings suppression
- Document workarounds with comments
