# Technology Stack

## Programming Languages

### C++

- **Standard**: C++23 (minimum requirement)
- **Primary Language**: Core framework and performance-critical components
- **Compilers Supported**:
  - GCC 14+ (primary, recommended)
  - Clang (with ThreadSanitizer and AddressSanitizer support)
  - AppleClang (macOS support)

### Python

- **Version**: Python 3.12+
- **Integration**: Via cppyy for seamless C++/Python interoperability
- **Use Cases**: User algorithms, testing, configuration scripts
- **Required Modules**: cppyy, numpy (2.0.0+), pytest, pytest-cov (4.0.0+)

### Configuration Languages

- **Jsonnet**: Workflow configuration files (`.jsonnet`)
- **CMake**: Build system (3.31+ required)
- **YAML**: CI/CD workflows, Spack environments

## Build System

### CMake (3.31+)

Primary build system with modern CMake practices.

**Key Features**:

- FetchContent for dependency management (Catch2, GSL, mimicpp, cetmodules)
- CTest integration for testing
- Custom targets for coverage, clang-tidy, formatting
- Multi-configuration support (Debug, Release, RelWithDebInfo, Coverage)

**Build Options**:

```cmake
ENABLE_TSAN          # Thread Sanitizer
ENABLE_ASAN          # Address Sanitizer
ENABLE_PERFETTO      # Perfetto profiling
PHLEX_USE_FORM       # FORM integration
ENABLE_COVERAGE      # Code coverage
ENABLE_CLANG_TIDY    # Static analysis during build
```

**Compiler Flags**:

- `-Wall -Werror -Wunused -Wunused-parameter -pedantic`
- GCC-specific workarounds for versions 14-16
- Sanitizer flags when enabled

### Cetmodules (4.01.01)

Fermilab's CMake modules for HEP software, providing:

- Package configuration
- Installation layout
- Environment setup

## Core Dependencies

### Required Libraries

**Boost** (Components: json, program_options)

- JSON parsing and CLI argument handling

**Intel TBB** (Threading Building Blocks)

- Parallel execution engine
- Task scheduling and work stealing

#### fmt

- Modern C++ formatting library

#### jsonnet

- Configuration file parsing and evaluation

#### spdlog

- Structured logging with multiple sinks

### Testing Frameworks

**Catch2** (v3.10.0)

- Unit testing framework
- Fetched via FetchContent

**mimicpp** (v8)

- Modern C++ mocking framework
- Fetched via FetchContent

**pytest** (Python)

- Python test execution
- pytest-cov for coverage

### Development Tools

**Microsoft GSL** (v4.2.0)

- C++ Core Guidelines Support Library
- Fetched via FetchContent

#### clang-tidy (20 or latest)

- Static analysis
- Configurable via `.clang-tidy`

#### clang-format

- Code formatting
- Configured via `.clang-format`

**cmake-format** / **gersemi**

- CMake file formatting
- Configured via `.cmake-format.json` and `.gersemirc`

#### ruff

- Python linting and formatting
- Configured via `pyproject.toml`

#### markdownlint

- Markdown linting
- Configured via `.markdownlint.jsonc`

#### actionlint

- GitHub Actions workflow linting
- Configured via `.github/actionlint.yaml`

### Optional Dependencies

#### Perfetto

- Performance profiling and tracing
- Enabled with `ENABLE_PERFETTO=ON`

**ROOT** (CERN)

- Scientific data format support
- Required for FORM integration
- TFile, TTree, TBranch support

#### cppyy

- Optional technology for C++/Python interoperability
- Python plugin support uses C API for Python and NumPy

## Code Coverage

### Tools

- **gcov**: GCC's coverage instrumentation
- **gcovr**: XML report generation for Codecov
- **lcov**: HTML report generation
- **genhtml**: HTML visualization

### Workflow

```bash
cmake --preset coverage-gcc  # or coverage-clang
cmake --build . --target coverage-xml    # XML for CI
cmake --build . --target coverage-html   # HTML for local
cmake --build . --target coverage-clean  # Clean data
```

### Integration

- Codecov for CI/CD coverage tracking
- VS Code Coverage Gutters extension support
- Automatic path normalization for generated files

## Package Management

### Spack

Primary distribution method for users.

**Repositories**:

- `fnal_art`: Fermilab art framework recipes
- `phlex-spack-recipes`: Phlex-specific recipes

**Installation**:

```bash
spack repo add https://github.com/FNALssi/fnal_art.git
spack repo add https://github.com/Framework-R-D/phlex-spack-recipes.git
spack env create my-phlex-environment
spack env activate my-phlex-environment
spack add phlex %gcc@14
spack install
```

## Development Commands

### Build

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j $(nproc)
```

### Test

```bash
ctest --test-dir build -j $(nproc)
```

### Coverage

```bash
./scripts/coverage.sh all  # Complete workflow
./scripts/coverage.sh setup test html view  # Step by step
```

### Format Check/Fix

```bash
# C++ formatting
ninja clang-format-check
ninja clang-format-fix

# CMake formatting
ninja cmake-format-check
ninja cmake-format-fix

# Python formatting
ruff check .
ruff format .

# Markdown
markdownlint '**/*.md'
```

### Static Analysis

```bash
ninja clang-tidy-check
ninja clang-tidy-fix
```

## CI/CD

### GitHub Actions

- **Build Matrix**: Multiple compilers, OS, configurations
- **Coverage**: Automatic upload to Codecov
- **Formatting**: Automated checks and fixes
- **CodeQL**: Security analysis
- **Dependabot**: Dependency updates with auto-merge

### Test Timeouts

- Default: 90 seconds per test
- Configurable via `DART_TESTING_TIMEOUT` and `CTEST_TEST_TIMEOUT`

## Environment Variables

### Build-time

- `PHLEX_INSTALL`: Installation directory
- `SPDLOG_LEVEL`: Logging level (debug, info, warn, error)

### Test-time

- `PHLEX_PLUGIN_PATH`: Plugin search path
- `PYTHONPATH`: Python module search path
- `VIRTUAL_ENV`: Python virtual environment
- `PATH`: Executable search path
