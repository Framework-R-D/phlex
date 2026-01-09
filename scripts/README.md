# Phlex Scripts

This directory contains helper scripts for Phlex development and testing.

## Environment Setup

### setup-env.sh

Configures the build environment for Phlex. Supports multiple deployment scenarios and gracefully adapts to different environments.

#### Usage

**Basic usage** (from any location):

```bash
# From repository root
. scripts/setup-env.sh

# From scripts directory
. setup-env.sh

# From anywhere (using absolute path)
. /path/to/phlex/scripts/setup-env.sh
```

**Note**: The script must be **sourced** (using `.` or `source`), not executed directly.

#### Supported Environments

The script automatically detects and adapts to:

1. **Multi-project workspace with Spack MPD**
   - Workspace structure: `workspace/srcs/phlex/`, `workspace/build/`, `workspace/local/`
   - Uses Spack MPD for project selection
   - Activates local Spack environment

2. **Standalone repository with Spack**
   - Direct clone of phlex repository
   - Uses Spack for dependencies
   - Activates specified Spack environment

3. **Standalone repository with system packages**
   - Direct clone of phlex repository
   - Dependencies from system package manager (apt, dnf, brew, etc.)
   - No Spack required

4. **CI/Container environment**
   - Automatically detects GitHub Actions or container environment
   - Uses container-specific Spack setup

#### Environment Variables

Configure the script behavior by setting these variables **before** sourcing:

| Variable | Description | Example |
|----------|-------------|---------|
| `PHLEX_SPACK_ROOT` | Path to Spack installation | `export PHLEX_SPACK_ROOT=/opt/spack` |
| `PHLEX_SPACK_ENV` | Spack environment to activate | `export PHLEX_SPACK_ENV=phlex-dev` |
| `PHLEX_BUILD_DIR` | Build directory location | `export PHLEX_BUILD_DIR=/tmp/phlex-build` |
| `CMAKE_BUILD_TYPE` | CMake build type | `export CMAKE_BUILD_TYPE=Debug` |

#### What the Script Does

1. **Detects build mode**: Multi-project workspace or standalone repository
2. **Finds Spack** (optional): Searches common locations or uses `PHLEX_SPACK_ROOT`
3. **Activates Spack environment** (optional): Uses MPD or standard activation
4. **Sets environment variables**: `PHLEX_SOURCE_DIR`, `PHLEX_BUILD_DIR`, etc.
5. **Checks build tools**: Verifies CMake, GCC, Ninja, coverage tools
6. **Provides feedback**: Clear success/warning/error messages

#### Graceful Degradation

The script is designed to work in minimal environments:

- **Spack not found?** → Warns and continues (assumes system packages)
- **Spack MPD not available?** → Skips silently, uses standard activation
- **Optional tools missing?** → Warns but continues (e.g., Ninja, lcov)
- **Critical tools missing?** → Fails with helpful installation instructions

#### Examples

##### Example 1: Multi-project workspace with Spack

```bash
cd /workspace/phlex
. srcs/phlex/scripts/setup-env.sh
# Detects multi-project mode, activates local Spack environment
cmake --preset default -S srcs -B build
ninja -C build
```

##### Example 2: Standalone with custom Spack environment

```bash
export PHLEX_SPACK_ENV=/home/user/spack-envs/phlex-dev
cd /home/user/phlex
. scripts/setup-env.sh
# Uses specified Spack environment
cmake -S . -B build
cmake --build build
```

##### Example 3: System packages only (no Spack)

```bash
# Install dependencies via system package manager
sudo apt install cmake g++ ninja-build gcovr lcov

cd /home/user/phlex
. scripts/setup-env.sh
# Warns that Spack not found, but succeeds with system tools
cmake -S . -B build
ninja -C build
```

##### Example 4: CI environment

```bash
# GitHub Actions workflow already has environment configured
. $GITHUB_WORKSPACE/phlex-src/scripts/setup-env.sh
# Detects CI, uses existing environment
```

#### Troubleshooting

##### Error: "Spack not found - assuming dependencies from system"

- This is a warning, not an error
- Either install Spack, or ensure CMake and GCC are available via system packages
- Set `PHLEX_SPACK_ROOT` if Spack is installed in a non-standard location

##### Error: "CMake not found - required for building"

- Install CMake via system package manager or Spack
- System: `apt install cmake` / `dnf install cmake` / `brew install cmake`
- Spack: `spack install cmake && spack load cmake`

##### Error: "Failed to activate local Spack environment"

- Check that `workspace/local/spack.yaml` exists
- Ensure Spack environment is properly configured
- Try activating manually: `spack env activate /path/to/local`

##### Info: "Ninja not found - builds will use make"

- This is informational - builds will still work
- Install Ninja for faster builds: `apt install ninja-build`

## Coverage Testing

### coverage.sh

Provides convenient commands for managing code coverage analysis.

#### Usage (coverage.sh)

```bash
# From repository root
./scripts/coverage.sh [--preset <coverage-clang|coverage-gcc>] [COMMAND] [COMMAND...]

# Multiple commands in sequence
./scripts/coverage.sh setup test xml html
```

#### Presets

The `--preset` flag controls the toolchain and instrumentation method:

- **`coverage-clang`** (Default):
  - Uses LLVM source-based coverage.
  - Best for local development (fast, accurate).
  - Generates high-fidelity HTML reports.
  - Key commands: `setup`, `test`, `html`, `view`, `summary`.

- **`coverage-gcc`**:
  - Uses `gcov` instrumentation.
  - Best for CI pipelines requiring XML output (e.g., Codecov).
  - Key commands: `setup`, `test`, `xml`, `upload`.

#### Commands

| Command | Description |
|---------|-------------|
| `setup` | Configure and build with coverage instrumentation |
| `clean` | Remove coverage data files |
| `test` | Run tests with coverage collection |
| `report` | Generate both XML and HTML coverage reports |
| `xml` | Generate XML coverage report only |
| `html` | Generate HTML coverage report only |
| `view` | Open HTML coverage report in browser |
| `summary` | Show coverage summary in terminal |
| `upload` | Upload coverage to Codecov |
| `all` | Complete workflow: setup, test, generate all reports |
| `help` | Show help message |

#### Environment Setup (coverage.sh)

The `coverage.sh` script automatically sources `setup-env.sh` if found:

1. First tries workspace-level: `$WORKSPACE_ROOT/setup-env.sh`
2. Then tries repository-level: `$PROJECT_SOURCE/scripts/setup-env.sh`
3. If neither found, assumes environment is already configured

#### Coverage Examples

**Complete coverage workflow**:

```bash
./scripts/coverage.sh all
```

**Step-by-step workflow**:

```bash
./scripts/coverage.sh setup    # Configure with coverage
./scripts/coverage.sh test     # Run tests
./scripts/coverage.sh xml      # Generate XML report
./scripts/coverage.sh html     # Generate HTML report
./scripts/coverage.sh view     # View in browser
```

**Generate and upload to Codecov**:

```bash
export CODECOV_TOKEN='your-token-here'
./scripts/coverage.sh setup test xml upload
```

**Clean and regenerate**:

```bash
./scripts/coverage.sh clean test xml html
```

#### Codecov Token Setup

Choose one method:

```bash
# Method 1: Environment variable
export CODECOV_TOKEN='your-token'

# Method 2: Token file (more secure)
echo 'your-token' > ~/.codecov_token
chmod 600 ~/.codecov_token
```

#### Custom Build Directory

```bash
export BUILD_DIR=/custom/build/location
./scripts/coverage.sh all
```

## Development Workflow

### Initial Setup

```bash
# 1. Source environment
. scripts/setup-env.sh

# 2. Configure project
cmake --preset default -S srcs -B build

# 3. Build
ninja -C build

# 4. Test
cd build && ctest
```

### Coverage Analysis

```bash
# One-command complete coverage
./scripts/coverage.sh all

# Or step by step
./scripts/coverage.sh setup
./scripts/coverage.sh test
./scripts/coverage.sh html
./scripts/coverage.sh view
```

### Continuous Development

```bash
# Keep environment active in your shell
. scripts/setup-env.sh

# Make code changes, then rebuild
ninja -C build

# Run specific tests
cd build && ctest -R test_name

# Check coverage for specific changes
./scripts/coverage.sh clean test summary
```

## Additional Notes

- All scripts should be run from the repository root or scripts directory
- Scripts provide colored output when running in an interactive terminal
- Environment setup is idempotent - safe to source multiple times
- Scripts follow the project's shell scripting standards (see copilot-instructions.md)
