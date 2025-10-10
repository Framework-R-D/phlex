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

### Coverage in CI

The project automatically runs coverage analysis on every PR and push to main/develop branches. Coverage reports are uploaded to [Codecov](https://codecov.io/gh/Framework-R-D/phlex) for tracking and PR integration.

