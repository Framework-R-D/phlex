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
| :--- | :--- | :--- |
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
| :--- | :--- |
| `setup` | Configure and build with coverage instrumentation |
| `clean` | Remove coverage data files (C++ and Python) |
| `test` | Run tests with coverage collection (C++ and Python) |
| `report` | Generate both XML and HTML coverage reports |
| `xml` | Generate XML coverage report only |
| `html` | Generate HTML coverage report only |
| `python` | Generate Python coverage report using pytest-cov |
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

#### Python Coverage Requirements

Python coverage requires the following packages to be installed:

```bash
pip install pytest pytest-cov
```

When `ENABLE_COVERAGE=ON` and pytest-cov is available, Python tests will automatically generate coverage reports alongside C++ coverage.

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

**Python coverage only**:

```bash
./scripts/coverage.sh setup test python
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

## Developer Tools

These scripts are **not** invoked by CI — they are local developer utilities
for post-processing clang-tidy output and managing GitHub code-scanning alerts.

### `clang_tidy_check_summary.py`

Reads a `clang-tidy-fixes.yaml` file (produced by `run-clang-tidy
-export-fixes`) and writes a compact markdown task list — one line per check
name with its unique occurrence count — suitable for pasting into a GitHub
issue or PR description to track remediation progress.

#### Usage (`clang_tidy_check_summary.py`)

```bash
# Print checklist to stdout
python3 scripts/clang_tidy_check_summary.py build/clang-tidy-fixes.yaml

# Write to a file
python3 scripts/clang_tidy_check_summary.py build/clang-tidy-fixes.yaml \
    -o summary.md

# Add documentation hyperlinks for each check name
python3 scripts/clang_tidy_check_summary.py build/clang-tidy-fixes.yaml \
    --links -o summary.md
```

#### Output format (`clang_tidy_check_summary.py`)

```markdown
- [ ] cert-err34-c (3)
- [ ] modernize-use-nullptr (12)
- [ ] readability-identifier-naming (47)
```

With `--links` each check name becomes a hyperlink to its clang-tidy
documentation page.

#### Typical workflow

```bash
# 1. Run clang-tidy and collect fixes (from repo root)
run-clang-tidy -p build -export-fixes build/fixes.yaml phlex/ form/ plugins/

# 2. Generate a summary for a GitHub issue
python3 scripts/clang_tidy_check_summary.py build/fixes.yaml --links \
    -o summary.md

# 3. Paste summary.md into the issue body
```

#### Uniqueness counting

Two entries are collapsed into a single occurrence when they share the same
`(DiagnosticName, FilePath, FileOffset)` triplet — this avoids inflating
counts when the same location is reported by multiple translation units.

---

### `clang_tidy_fixes_to_problems.py`

Converts a `clang-tidy-fixes.yaml` file into gcc/clang compiler-style
diagnostic lines:

```text
/abs/path/file.cpp:line:col: warning: message [check-name]
```

VS Code's built-in `$gcc` problem matcher recognises this format and turns
each line into a clickable source link in the terminal panel.  The script also
handles CI-to-local path translation via `--path-map`.

#### Usage (`clang_tidy_fixes_to_problems.py`)

```bash
# Print diagnostics to stdout (opens as clickable links in VS Code terminal)
python3 scripts/clang_tidy_fixes_to_problems.py build/clang-tidy-fixes.yaml

# Write to a file
python3 scripts/clang_tidy_fixes_to_problems.py build/fixes.yaml \
    -o build/problems.txt

# Translate CI runner paths to local paths
python3 scripts/clang_tidy_fixes_to_problems.py build/fixes.yaml \
    --path-map /__w/phlex/phlex/phlex-src=/home/user/phlex \
    --workspace-root /home/user/phlex \
    -o build/problems.txt
```

#### Path mapping

clang-tidy embeds absolute paths as seen on the machine that ran the build.
When you download a `clang-tidy-fixes.yaml` artifact from CI you need to
translate the runner paths to your local checkout.

Use `--path-map OLD=NEW` (repeatable).  Two default mappings for the standard
Phlex CI layout are applied automatically after any explicit `--path-map`
entries:

| CI path prefix | Local path |
| :--- | :--- |
| `/__w/phlex/phlex/phlex-src` | `<workspace-root>` |
| `/__w/phlex/phlex/phlex-build` | `<workspace-root>/build` |

#### External-header handling

When a diagnostic's primary location is inside a system or third-party header
that is absent locally, the script searches the attached clang-tidy notes for
a trace back to workspace source code and redirects the reported location
there.  The original external location is preserved in the message text.

---

### `codeql_reset_dismissed_alerts.py`

Reopens every dismissed CodeQL code-scanning alert for a repository via the
GitHub REST API.  Useful before an audit or after a large refactor to ensure
the alert list reflects the current code rather than past dismissals.

#### Authentication

Set a GitHub personal access token (PAT) with the `security_events` scope
before running:

```bash
export GITHUB_TOKEN=ghp_your_token_here

# Or reuse the token from the GitHub CLI
export GH_TOKEN=$(gh auth token)
```

#### Usage (`codeql_reset_dismissed_alerts.py`)

```bash
# Preview what would be reopened (no changes made)
GITHUB_TOKEN=ghp_... python3 scripts/codeql_reset_dismissed_alerts.py \
    --owner Framework-R-D --repo phlex --dry-run

# Reopen all dismissed alerts
GITHUB_TOKEN=ghp_... python3 scripts/codeql_reset_dismissed_alerts.py \
    --owner Framework-R-D --repo phlex
```

#### What it does

1. Pages through all alerts with `state=dismissed` in batches of 100.
2. Prints the alert number, rule ID, and dismissal reason for each one.
3. Issues a `PATCH` request per alert to set `state=open`.

After running, the next scheduled or triggered CodeQL analysis will
re-evaluate every reopened alert and either confirm it as still present or
close it as fixed.

#### Rate limits

Each alert requires one PATCH request.  The GitHub API allows 5 000
authenticated requests per hour; for repositories with large numbers of
dismissed alerts, check your remaining quota first:

```bash
gh api /rate_limit
```

---

## Additional Notes

- All scripts should be run from the repository root or scripts directory
- Scripts provide colored output when running in an interactive terminal
- Environment setup is idempotent - safe to source multiple times
- Scripts follow the project's shell scripting standards (see copilot-instructions.md)
