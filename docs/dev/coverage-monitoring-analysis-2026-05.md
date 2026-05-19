# Coverage Monitoring: Analysis Report

Date: 2026-05-14

## Table of Contents

- [1. Interpretation of Goals](#1-interpretation-of-goals)
- [2. Current Coverage Monitoring Mechanisms](#2-current-coverage-monitoring-mechanisms)
- [3. Correctness and Efficacy Evaluation](#3-correctness-and-efficacy-evaluation)
- [4. Action Items](#4-action-items)
- [Appendix: Original Prompt](#appendix-original-prompt)

## 1. Interpretation of Goals

Based on the `codecov.yml` structure, workflow configuration, and code comments, the coverage
monitoring goals are:

**Primary goal**: Enforce a minimum 80% line/branch coverage of **production code** — meaning C++
framework code and the Python plugin infrastructure (`plugins/python/python`) — as a CI gate on
every PR and push to `main`/`release/*`. A patch threshold of 80% (±5%) ensures new code
contributions do not drag down coverage. Failures in this gate block merging.

**Secondary goal**: Independently track the coverage of **utility/workflow scripts** (`scripts/`)
without those numbers affecting the production-code gate in either direction. Scripts coverage is to
be *visible* in the CodeCov dashboard and *monitored* for developer awareness, but it does not gate
CI.

**Explicit isolation intent**: Scripts coverage data must not appear in the production-code coverage
numbers, either in the numerator (covered lines) or the denominator (total measurable lines), in any
report surface that affects the 80% threshold.

## 2. Current Coverage Monitoring Mechanisms

### 2.1 Build System

Coverage is controlled by the `ENABLE_COVERAGE` CMake option (off by default). Two CMake presets
enable it:

- `CMakePresets.json` `coverage-clang`: Uses `-fprofile-instr-generate -fcoverage-mapping` (LLVM
  source-based coverage). This is the **default CI preset**.
- `CMakePresets.json` `coverage-gcc`: Uses `--coverage -fprofile-arcs -ftest-coverage` (GCC/gcov).
  Used when GCC XML output is needed.

`Modules/private/CreateCoverageTargets.cmake` defines all coverage CMake targets when
`ENABLE_COVERAGE` is on:

| Target | Tool | Output | Scope |
| --- | --- | --- | --- |
| `coverage-llvm` | `llvm-cov` + `llvm-profdata` | `.txt` summary + `coverage-llvm.info` (LCOV) | C++ only |
| `coverage-xml` | `gcovr` | `coverage.xml` (Cobertura) | C++ only |
| `coverage-html` | `lcov` + `genhtml` | `coverage-html/` directory | C++ only |
| `coverage-summary` | `gcovr` | Terminal print | C++ only |
| `coverage-python` | `pytest-cov` | `coverage-python.xml` + HTML | `test/python/` + `plugins/python/python` |
| `coverage-scripts` | `pytest-cov` | `coverage-scripts.xml` | `scripts/` only, omitting `scripts/test/*` |
| `coverage-gcov` | orchestrator | combines xml + html | C++ |
| `coverage-llvm-lcov` | intermediate | `coverage-llvm.info` | C++ |
| `coverage-llvm-normalize` | `normalize_coverage_lcov.py` | rewritten `.info` | C++ |
| `coverage-xml-normalize` | `normalize_coverage_xml.py` | rewritten `.xml` | C++ |
| `coverage-clean` | `find` + `rm` | removes data files | C++ + Python |

C++ exclusion filters applied by both gcovr and llvm-cov:

```text
.*/test/.* | .*/_deps/.* | .*/external/.* | .*/third[-_]?party/.* | .*/boost/.*
| .*/tbb/.* | .*/spack/.* | /usr/.* | /opt/.* | /scratch/.* | .*\.cxx$ | .*\.hh$ | .*\.hxx$
```

These exclude test-helper C++ files, third-party headers, and system paths. Scripts Python code is
never in C++ coverage data at all.

### 2.2 Python Coverage Configuration

`test/python/.coveragerc` configures the production Python coverage target:

```ini
[run]
source = ../../plugins/python/python
omit = test_*.py, **/test_*.py, unit_test_*.py, ...
```

`pyproject.toml` configures coverage for the scripts tests:

```toml
[tool.coverage.run]
omit = ["scripts/test/*"]
```

This section is auto-discovered by pytest-cov when run from the repository root, excluding
`scripts/test/` (`conftest.py`, `test_*.py`) from the coverage measurement of `scripts/`.

The `coverage-scripts` CMake target also explicitly passes `--omit=${CMAKE_CURRENT_SOURCE_DIR}/*`
(resolving to the absolute `scripts/test/*` path), which adds to the omit list when using the CMake
target locally.

### 2.3 CI Workflows

#### `coverage.yaml` — Main Coverage Pipeline

Triggers: `push` to `main`/`release/*`, `pull_request`, `workflow_dispatch`,
`@phlexbot coverage` comment.

Change detection uses `file-type: cpp, cmake, python` with **`exclude-globs: scripts/**`**. This
means if only scripts files changed, `has_changes == 'false'` and the `coverage` job is skipped.

Jobs:

1. **`setup`**: resolves refs, detects relevant changes (excluding `scripts/**`)
1. **`coverage`**: builds (Clang default, GCC optional), runs all tests with `LLVM_PROFILE_FILE`
   set, then runs four report generation steps:
   - `cmake --build . --target coverage-llvm` → `coverage-llvm.info`
   - `cmake --build . --target coverage-python` → `coverage-python.xml`
   - Direct `pytest scripts/test/ --cov=scripts` → `coverage-scripts.xml` (not via CMake target)
   - Bundles artifacts
1. **`coverage-upload`**: downloads bundle, uploads to CodeCov:
   - `coverage.xml`/`coverage-llvm.info` + `coverage-python.xml` → `flags: unittests`
   - `coverage-scripts.xml` (if present) → `flags: scripts`, `fail_ci_if_error: false`

#### `python-check.yaml` — Python Linting and Scripts Test Coverage

Triggers: `pull_request`, `push` to `release/*`, `workflow_dispatch`, `workflow_call`.

Change detection: `file-type: python, yaml` — **no `exclude-globs`**. Any `.py` or `.yaml` change
(including `scripts/*.py`) triggers `has_changes == 'true'`.

The `scripts-test` job:

```bash
uv run --with pytest --with PyYAML --with pytest-cov \
  pytest scripts/test/ -v \
  --cov=scripts \
  --cov-report="xml:${GITHUB_WORKSPACE}/coverage-scripts.xml" \
  --cov-report=term-missing
```

Uploads result with `flags: scripts`, `fail_ci_if_error: false`.

This is the **primary trigger** for scripts coverage updates: it runs on every PR that touches
Python files (regardless of whether C++ changes), and uses an isolated `uv` environment.

### 2.4 CodeCov Configuration

`codecov.yml`:

```yaml
coverage:
  status:
    project:
      default:
        flags: [unittests]   # only unittests flag governs project threshold
        target: 80%
        threshold: 2%
    patch:
      default:
        flags: [unittests]   # only unittests flag governs patch threshold
        target: 80%
        threshold: 5%

flags:
  unittests:
    carryforward: true      # no paths restriction
  scripts:
    paths: [scripts/]       # associated with scripts/ directory
    carryforward: true      # no status thresholds

ignore:
  - "test/**/*"
  - "scripts/test/**/*"
  - "**/CMakeFiles/**"
  - "build/**"
```

### 2.5 Normalization Scripts

`scripts/normalize_coverage_xml.py` and `scripts/normalize_coverage_lcov.py` rewrite source paths
in coverage reports to be repository-relative, ensuring CodeCov can map coverage data to the correct
files regardless of where the build occurred (symlinked workspace, CI container, etc.).

`scripts/export_llvm_lcov.py` wraps `llvm-cov export --format=lcov` to produce an LCOV file that
CodeCov can ingest from the Clang coverage workflow.

## 3. Correctness and Efficacy Evaluation

### 3.1 Does the Numerator/Denominator Isolation Hold?

**C++ coverage** (`coverage-llvm.info` / `coverage.xml`): gcovr and llvm-cov are C++ tools and
produce data only for C++ source files. Scripts Python files never appear in these outputs. ✅

**Production Python coverage** (`coverage-python.xml`): The `coverage-python` target runs pytest
with `--cov=test/python/` and `--cov=plugins/python/python`. The `scripts/` directory is not
listed in `--cov` and pytest-cov only measures files in the listed directories. ✅

**Scripts coverage** (`coverage-scripts.xml`): Generated by `pytest scripts/test/ --cov=scripts`.
C++ files are never measured by pytest-cov. Production Python in `plugins/python/python` is not
in `--cov`. ✅

**Flag separation**: The two CodeCov uploads are strictly tagged:

- `flags: unittests` receives only C++ + production Python reports
- `flags: scripts` receives only `coverage-scripts.xml`

**Status thresholds**: Both project and patch thresholds reference only `flags: [unittests]`.
Scripts data cannot affect CI pass/fail. ✅

**Conclusion**: The numerator/denominator isolation is correctly implemented. Scripts coverage data
does not contaminate production code numbers in any CI-relevant surface.

### 3.2 Are Scripts Being Monitored?

The `scripts` flag in CodeCov accumulates coverage from two sources (both correctly isolated):

1. `python-check.yaml` → `scripts-test` job (runs on every PR touching Python files)
1. `coverage.yaml` → "Generate scripts coverage report" step (runs alongside C++ coverage)

The `pyproject.toml` `[tool.coverage.run] omit = ["scripts/test/*"]` correctly excludes the test
helper files from the scripts coverage measurement. Both CI paths invoke pytest from the repository
root, where this config is auto-discovered. ✅

### 3.3 Deficiencies

#### Deficiency 1: Scripts coverage on `main` is stale after scripts-only merges

When a PR that contains **only scripts changes** is merged to `main`:

- `coverage.yaml` triggers on `push: branches: [main]`, but the `coverage` job is skipped because
  `exclude-globs: scripts/**` makes `has_changes == 'false'`. Carryforward is used.
- `python-check.yaml` is triggered on `pull_request` (when the PR was open) but **not on
  `push: branches: [main]`** — there is no `push: branches: [main]` trigger in that workflow.

The result: after merging a scripts-only PR, CodeCov's `main` branch shows stale `scripts` flag
data (last computed on the PR), not freshly computed from the merged main commit. While
`carryforward: true` prevents a gap, it means a regression in scripts test coverage introduced by a
PR would only be visible on the PR run, not on the post-merge main run.

#### Deficiency 2: Duplicate scripts coverage uploads on mixed PRs

When a PR changes both C++ code and scripts Python code, two independent coverage runs both upload
`coverage-scripts.xml` with `flags: scripts`:

1. `python-check.yaml` → `scripts-test` job
1. `coverage.yaml` → "Generate scripts coverage report" + upload

These run in different environments (`uv` isolated environment vs. the `phlex-ci` Docker
container's system Python), with different pytest verbosity flags (`-v` vs. `-q`), and potentially
with different installed package sets. The results should be functionally identical, but the
redundancy means CodeCov receives two uploads for the `scripts` flag from the same commit.

#### Deficiency 3: The `coverage-scripts` CMake target is not used by either CI path

`scripts/test/CMakeLists.txt` defines a `coverage-scripts` CMake target that is a properly
integrated counterpart to `coverage-python`. However, neither CI workflow invokes it. The
`coverage.yaml` workflow runs a raw `python3 -m pytest scripts/test/ --cov=scripts ...` command
directly. The `python-check.yaml` workflow uses `uv run`. Neither goes through
`cmake --build . --target coverage-scripts`.

This is not a serious problem because `pyproject.toml` provides the omit config for both CI paths.
But it means `coverage-scripts` is effectively a dead target in CI — it exists, is advertised in
the `CreateCoverageTargets.cmake` status messages and the `coverage-clean` target, but is never
exercised in CI.

#### Deficiency 4: The `pyproject.toml` omit comment is misleading

The comment in `pyproject.toml` reads:

> "pytest-cov resolves this glob relative to the measurement source root (scripts/), so the pattern
> works whether pytest is invoked from the repo root or a build directory that sets --cov=scripts."

This is inaccurate. Coverage.py resolves `omit` patterns relative to the **current working
directory** (i.e., wherever pytest is invoked), not relative to the `--cov` source root. The
pattern `scripts/test/*` works because both CI paths `cd` to the repo root before running pytest,
so coverage.py expands it to an absolute path like `/repo/scripts/test/*`. If pytest were invoked
from a different directory (e.g., a build directory), the pattern would fail to match. The mechanism
is correct, but the justification in the comment is wrong.

#### Potential Risk 5 (Minor): if a `conftest.py` is added under `test/python/`, it may appear in `coverage-python.xml`

The `coverage-python` target passes `--cov=${CMAKE_CURRENT_SOURCE_DIR}` (i.e., `test/python/`).
The `test/python/.coveragerc` omits `test_*.py` and `*/test/*`. However, `conftest.py` does not
match `test_*.py` (it does not start with `test_`), and whether it matches `*/test/*` depends on
coverage.py's fnmatch semantics — `*` in fnmatch does not cross path separator boundaries, so
`*/test/*` would match `/x/test/conftest.py` but not `/x/test/python/conftest.py`. If a
`conftest.py` is added at `test/python/conftest.py` in the future, it may therefore appear in
`coverage-python.xml` and be attributed to the `unittests` flag. CodeCov's
`ignore: ["test/**/*"]` would still filter it out of the CodeCov display, but the uploaded XML
could include it. This would be cosmetic.

### 3.4 Summary Table

| Concern | Status | Notes |
| --- | --- | --- |
| Scripts data in C++ coverage | ✅ Not possible | gcovr/llvm-cov do not measure Python |
| Scripts data in `coverage-python.xml` | ✅ Not included | `--cov` args do not reference `scripts/` |
| Scripts uploaded with `unittests` flag | ✅ Separate upload | `flags: scripts` for all scripts uploads |
| CI gate affected by scripts | ✅ Correctly isolated | Status checks use only `flags: [unittests]` |
| Scripts test helpers in scripts coverage | ✅ Omitted via `pyproject.toml` | Resolved at cwd (repo root) in both CI paths |
| Scripts coverage on main after scripts-only merge | ⚠️ Uses carryforward | `python-check.yaml` does not run on `push: main` |
| Duplicate scripts upload on mixed PRs | ⚠️ Two uploads | Both workflows upload `flags: scripts` |
| `coverage-scripts` CMake target used in CI | ⚠️ Not used | Only local use; CI uses direct pytest invocation |
| `pyproject.toml` omit comment accuracy | ⚠️ Misleading | Says "source root" but means "working directory" |
| `conftest.py` in `coverage-python.xml` | ⚠️ Minor/cosmetic | Filtered by CodeCov `ignore:` but present in the XML |

## 4. Action Items

### Action Item 1: Close the scripts coverage gap on `main` (Deficiency 1)

**Problem**: Scripts-only merges to `main` leave stale scripts coverage in CodeCov.

**Recommended fix (Option A)**: Add `push: branches: [main]` to `python-check.yaml`. This is the
cleanest fix. The workflow is already lightweight (no C++ build), so running it on every push to
`main` is inexpensive.

```yaml
"on":
  pull_request:
  push:
    branches: [main, "release/*"]   # add main here
  workflow_dispatch:
  workflow_call:
    ...
```

**Option B** (not recommended): Remove `exclude-globs: scripts/**` from `coverage.yaml`. This
would trigger the full expensive C++ coverage build just because a script changed.

**Option C** (acceptable but not recommended): Accept the current behavior. It is tolerable because
the `scripts` flag has no CI threshold and coverage was computed on the PR before merge. However,
if a developer pushes a script commit directly to `main` (bypassing a PR), coverage will be
permanently stale until the next C++ coverage run.

### Action Item 2: Eliminate the duplicate scripts coverage upload (Deficiency 2)

**Problem**: When C++ and scripts both change on a PR, `coverage-scripts.xml` is uploaded twice
with `flags: scripts`.

**Recommended fix**: Remove the "Generate scripts coverage report" step and its corresponding
upload from `coverage.yaml`. The `python-check.yaml` workflow handles this more appropriately:

- It uses an isolated `uv` environment (more reproducible)
- It runs independently of C++ build success/failure
- It has correct change detection for Python files

The `coverage.yaml` workflow should focus exclusively on C++ and production Python coverage. If
Action Item 1 is applied, `python-check.yaml` becomes the sole and canonical source of scripts
coverage.

### Action Item 3: Clarify the `coverage-scripts` CMake target (Deficiency 3)

**Problem**: The `coverage-scripts` CMake target exists but is never invoked by CI, which may
mislead developers.

**Recommended fix**: Add a comment in `scripts/test/CMakeLists.txt` making it explicit that this
target is for local developer use only, and is not invoked by CI workflows (which use
`python-check.yaml`'s `scripts-test` job instead). No behavioral change is required.

If Action Item 2 is not applied and scripts coverage is to remain in `coverage.yaml`, consider
changing that workflow's "Generate scripts coverage report" step to use
`cmake --build . --target coverage-scripts` to unify local and CI execution.

### Action Item 4: Fix the misleading comment in `pyproject.toml` (Deficiency 4)

**Problem**: The omit comment incorrectly says coverage.py resolves patterns relative to the
measurement source root.

**Fix**: Replace the existing comment block above `omit = ["scripts/test/*"]` with:

```toml
# Exclude test helpers from coverage measurement.  Coverage.py resolves
# relative omit patterns against the current working directory.  Both CI
# paths (python-check.yaml and coverage.yaml) invoke pytest from the
# repository root, so "scripts/test/*" expands to the correct absolute path.
# The CMake coverage-scripts target passes an equivalent --omit on the
# command line for local developer runs.
```

### Action Item 5 (Optional): Consider a threshold for `scripts` coverage

The `codecov.yml` comment notes "No status thresholds are set yet" for the `scripts` flag. Once
scripts coverage monitoring is stable and representative, consider adding a minimum threshold (e.g.,
70–80%) to make the monitoring actionable rather than purely informational.

### Action Item 6 (Minor): Add `conftest.py` to `.coveragerc` omit patterns (Deficiency 5)

In `test/python/.coveragerc`, add `conftest.py` patterns to the `[run]` section:

```ini
[run]
source = ../../plugins/python/python
omit =
    conftest.py
    **/conftest.py
    test_*.py
    **/test_*.py
    unit_test_*.py
    **/unit_test_*.py
    */test/*
```

This prevents test infrastructure files from appearing in `coverage-python.xml` even before CodeCov
filters them via `ignore: ["test/**/*"]`.

## Appendix: Original Prompt

> Please explain the current coverage monitoring mechanisms in this repository. Use the workflows
> and reusable actions and their supporting scripts in this repository and any internal or external
> documentation to collect necessary information (including actual CodeCov results at
> [https://app.codecov.io/gh/Framework-R-D/phlex](https://app.codecov.io/gh/Framework-R-D/phlex)).
> You may also use anything you find in the other repositories in /workspaces in this devcontainer
> to support this task.
>
> I am specifically interested in:
>
> 1. What your interpretation of the goals of coverage monitoring in this repository is.
> 2. Whether the mechanisms in place are correct and actually meet those goals, or whether there are
>    deficiencies.
>
> In particular, it was my intention that test coverage of non-production code/scripts (workflow
> helper scripts, dev utility scripts) be monitored, but not affect the coverage numbers of C++ and
> production Python code in any way (either in the numerator or the denominator).
>
> Please present your findings in the form of a detailed report with:
>
> 1. Explanation of your derived interpretation of the goals of monitoring coverage in this
>    repository.
> 2. Your explanation of the current coverage monitoring mechanisms (build systems, scripts,
>    workflows, external services).
> 3. Your evaluation of the correctness and efficacy of the current coverage monitoring mechanisms,
>    and how they achieve your interpretation of the goals (or not).
> 4. Any action items necessary to align goals with actual mechanisms, implement expanded goals,
>    and/or fix deficiencies in implementation (along with justification for each, as appropriate).
