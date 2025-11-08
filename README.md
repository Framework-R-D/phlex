# Phlex

**P**arallel, **h**ierarchical, and **l**ayered **ex**ecution of data-processing algorithms

[![Code Coverage](https://codecov.io/gh/Framework-R-D/phlex/branch/main/graph/badge.svg)](https://codecov.io/gh/Framework-R-D/phlex)

## Contributions

_[The contribution policy is currently under development.]_

Except in rare circumstances, changes to the `phlex` repository are proposed and considered through GitHub pull requests (PRs).

> [!NOTE]
>
> - Both external contributors and framework developers are expected to abide by this contribution policy.
> - Even if you are a contributor with merge privileges, your PR should be reviewed by someone else before being merged into the `main` branch.

### Building with Coverage

Phlex ships CMake presets for both GCC (`coverage-gcc`) and Clang (`coverage-clang`) coverage flows. Each preset enables instrumentation, selects the appropriate compiler, and ensures reports are generated in a dedicated build tree.

```bash
# Configure (choose coverage-gcc or coverage-clang)
cmake --preset coverage-gcc -S . -B build-coverage

# Build and run tests
cmake --build build-coverage -j $(nproc)
ctest --test-dir build-coverage -j $(nproc)

# Generate reports
cmake --build build-coverage --target coverage-gcov      # GCC XML + HTML bundle
cmake --build build-coverage --target coverage-llvm      # Clang text summary
cmake --build build-coverage --target coverage-html      # HTML (if lcov available)
cmake --build build-coverage --target coverage-xml       # XML for CI/Codecov
cmake --build build-coverage --target coverage-clean     # Remove coverage artefacts
```

Highlights:

- GCC flows rely on `gcov`, `gcovr`, and `lcov` for instrumentation and reporting.
- Clang flows use `llvm-profdata`/`llvm-cov` and produce both text and LCOV outputs.
- Generated files are normalised so downstream tools (Codecov, IDE plug-ins) can resolve repository paths correctly.
- The default preset location `build-coverage` matches the layout expected by the helper scripts and CI workflows.

#### Using the Coverage Script

For convenience, the `scripts/coverage.sh` script automates the entire coverage workflow:

```bash
# Complete workflow (recommended)
./scripts/coverage.sh all

# Individual commands
./scripts/coverage.sh setup       # Configure and build with coverage
./scripts/coverage.sh test        # Run tests with coverage
./scripts/coverage.sh xml         # Generate XML report
./scripts/coverage.sh html        # Generate HTML report
./scripts/coverage.sh view        # Open HTML report in browser
./scripts/coverage.sh summary     # Show coverage summary
./scripts/coverage.sh upload      # Upload to Codecov

# Chain multiple commands
./scripts/coverage.sh setup test html view
```

**Important**: Coverage data becomes stale when source files change. Always rebuild before generating reports:

```bash
./scripts/coverage.sh setup test html  # Rebuild → test → generate HTML
```

The script automatically:

- Detects standalone vs. multi-project build configurations
- Sources the appropriate `setup-env.sh`
- Chooses the correct preset (GCC or Clang) for the requested coverage mode
- Detects stale instrumentation and rebuilds when needed
- Handles generated source files via temporary symlink trees
- Normalizes coverage paths for Codecov compatibility

#### Coverage and Generated Files

The coverage system handles generated C/C++ files (e.g., from code generators) by:

1. Creating a temporary symlink tree at `.coverage-generated/` in the repository root
2. Mapping build-directory paths to repository-relative paths using `--path-map`
3. Normalizing XML paths with `normalize_coverage_xml.py` and LCOV paths with `normalize_coverage_lcov.py` before upload

This ensures Codecov and local IDEs can match coverage data to the correct files in your repository.

#### VS Code Integration

After generating HTML coverage, the script copies `lcov.info` to the workspace root for VS Code extensions like Coverage Gutters. Install the extension and open any source file to see coverage indicators in the gutter.

#### Uploading to Codecov

To upload coverage manually:

```bash
# Set your token (choose one method)
export CODECOV_TOKEN='your-token'
echo 'your-token' > ~/.codecov_token && chmod 600 ~/.codecov_token

# Upload
./scripts/coverage.sh xml upload
```

The upload command uses the Codecov CLI and automatically normalizes paths for repository compatibility.

### Coverage in CI

The GitHub Actions workflow (`.github/workflows/coverage.yaml`) runs on every PR and on pushes to `main`/`develop` when relevant files change. It:

1. Selects GCC or Clang coverage presets based on workflow inputs.
2. Configures and builds inside the `phlex-ci` container using the composite actions (`setup-build-env`, `configure-cmake`, `build-cmake`).
3. Executes the test suite with coverage instrumentation.
4. Generates reports (`coverage-gcov` for GCC or `coverage-llvm` for Clang) and normalises them for Codecov.
5. Publishes artefacts and uploads results via `codecov/codecov-action@v4` when credentials are available.

Codecov annotates pull requests with coverage deltas and retains the historical coverage dashboard.
