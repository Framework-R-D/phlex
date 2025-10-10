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

Phlex supports comprehensive code coverage measurement using CMake's built-in coverage support:

```bash
# Configure with coverage enabled
cmake -DCMAKE_BUILD_TYPE=Coverage -DENABLE_COVERAGE=ON /path/to/phlex/source

# Build and run tests
cmake --build . -j $(nproc)
ctest -j $(nproc)

# Generate coverage reports
ninja coverage-xml    # XML report for CI/Codecov
ninja coverage-html   # HTML report for local viewing
ninja coverage        # Basic coverage info via ctest
ninja coverage-clean  # Clean coverage data files
```

The coverage system:

- Uses GCC's built-in `gcov` for instrumentation
- Generates reports with `gcovr` (XML) and `lcov` (HTML)
- Automatically filters to project sources only
- Handles known GCC coverage bugs gracefully
- Integrates with VS Code coverage extensions

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
- Sources environment setup scripts (`setup-env.sh`)
- Detects stale instrumentation and rebuilds when needed
- Handles generated source files via temporary symlink trees
- Normalizes coverage paths for Codecov compatibility

#### Coverage and Generated Files

The coverage system handles generated C/C++ files (e.g., from code generators) by:

1. Creating a temporary symlink tree at `.coverage-generated/` in the repository root
2. Mapping build-directory paths to repository-relative paths using `--path-map`
3. Normalizing XML paths with `normalize_coverage_xml.py` before upload

This ensures Codecov can match coverage data to the correct files in your repository.

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

The project automatically runs coverage analysis on every PR and push to main/develop branches. The workflow:

1. Builds with `-DCMAKE_BUILD_TYPE=Coverage -DENABLE_COVERAGE=ON`
2. Runs all tests via `ctest`
3. Generates XML coverage report using `cmake --build . --target coverage-xml`
4. Normalizes paths for generated files using `normalize_coverage_xml.py`
5. Uploads to [Codecov](https://codecov.io/gh/Framework-R-D/phlex) via `codecov/codecov-action@v4`

Coverage reports are uploaded to Codecov for tracking and PR integration, with automatic comments on PRs showing coverage changes.

