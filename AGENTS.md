# Phlex — Kilo Agent Instructions

This file provides persistent context for AI agents (Kilo and others) working
in the `phlex` repository. It supplements the global instructions in
`~/.config/kilo/AGENTS.md`.

## Product Overview

Phlex is a framework for **P**arallel, **h**ierarchical, and **l**ayered
**ex**ecution of data-processing algorithms. It provides a graph-based
execution engine for orchestrating complex data-processing workflows with
automatic parallelization, hierarchical data organization, and layered
algorithm execution. Primary users are scientific computing researchers and
data-pipeline engineers in high-energy physics (DUNE experiment).

- Version: 0.1.0 (active development); License: Apache 2.0
- GitHub: <https://github.com/Framework-R-D/phlex>
- Coverage: <https://codecov.io/gh/Framework-R-D/phlex>

## Repository Ecosystem

All five repos are co-located in `/workspaces/` in the devcontainer:

| Path | GitHub repo | Role |
| ---- | ----------- | ---- |
| `/workspaces/phlex` | `Framework-R-D/phlex` | **Primary** — this repo |
| `/workspaces/phlex-design` | `Framework-R-D/phlex-design` | Design & documentation |
| `/workspaces/phlex-coding-guidelines` | `Framework-R-D/phlex-coding-guidelines` | Coding guidelines |
| `/workspaces/phlex-examples` | `Framework-R-D/phlex-examples` | Example user programs |
| `/workspaces/phlex-spack-recipes` | `Framework-R-D/phlex-spack-recipes` | Spack packaging recipes |

Critical build dependency: `FNALssi/cetmodules` (Fermilab CMake modules).
Container images: `phlex-ci` (CI), `phlex-dev` (devcontainer/local dev).

## Technology Stack

- **C++23** — core framework; GCC 14+ primary, Clang (sanitizers), AppleClang
- **Python 3.12+** — user algorithms, tests, scripts
- **Jsonnet** — workflow configuration (`.jsonnet` files)
- **CMake 3.31+** — build system via `cetmodules`
- **Intel TBB** — parallel execution engine
- **Boost** (json, program_options), **fmt**, **spdlog**, **jsonnet** library
- **Catch2 v3.10.0** (FetchContent), **mimicpp v8** (FetchContent) — C++ tests
- **Microsoft GSL v4.2.0** (FetchContent) — C++ Core Guidelines support
- **pytest + pytest-cov** — Python tests

## Source Layout

```text
phlex/         C++ framework: app/, core/, model/, graph/, metaprogramming/, utilities/
form/          Optional FORM/ROOT persistence: core/, form/, persistence/, storage/, root_storage/
plugins/       Extensibility: python/ (C API + NumPy), layer_generator
test/          Tests: benchmarks/, python/, form/, mock-workflow/, utilities/, *.cpp
scripts/       Dev/CI automation: setup-env.sh, coverage.sh, fix_header_guards.py, ...
Modules/       CMake modules (private/)
ci/            CI container: Dockerfile, spack.yaml
dev/jules/     Jules AI agent environment
.devcontainer/ VS Code devcontainer config
.github/       CI workflows, actions, copilot-instructions.md
.kiro/rules/   Kiro AI guidelines (product.md, structure.md, tech.md, guidelines.md)
.amazonq/      Amazon Q guidelines (equivalent content)
```

Key architectural patterns:

- **Graph-based DAG execution** with automatic dependency resolution via TBB
- **Product store**: central type-safe data sharing between algorithms
- **Plugin architecture**: dynamic module loading, users don't touch core code
- **Hierarchical data model**: multi-level (e.g. run → subrun → event)
- **Type erasure**: heterogeneous algorithm collections
- **Configuration as code**: Jsonnet workflow definitions

## Devcontainer Environment

**In the devcontainer, the full Spack environment is pre-activated** via
`/entrypoint.sh` sourced by `/root/.bashrc`. All tools (`cmake`, `ninja`,
`gcc`, `gcovr`, `prek`, etc.) are available directly — do not source
`setup-env.sh` or `/entrypoint.sh` manually.

Outside the devcontainer, source the setup script first:

```bash
. scripts/setup-env.sh               # standalone repo
. srcs/phlex/scripts/setup-env.sh    # multi-project workspace
```

`prek` (Rust drop-in for `pre-commit`) is installed in `phlex-dev`, not in
`phlex-ci`. Detect the available hook tool:

```bash
PREKCOMMAND=$(command -v prek || command -v pre-commit || echo "")
```

## Build and Test

```bash
# Configure (always pass -B explicitly; presets don't set binaryDir)
cmake --preset default -B build      # standard build, FORM enabled
cmake --preset coverage-gcc -B build-coverage-gcc
cmake --preset clang-tidy -B build-clang-tidy

# Build
ninja -C build                       # or: cmake --build build -j $(nproc)

# Test
ctest --test-dir build -j $(nproc) --output-on-failure
ctest --test-dir build -j $(nproc) --test-timeout 90   # recommended guard
ctest --test-dir build -R "regex"                       # specific tests

# Coverage
./scripts/coverage.sh all            # full workflow
cmake --build build-coverage-gcc --target coverage-xml
cmake --build build-coverage-gcc --target coverage-html

# Pre-commit
$PREKCOMMAND run --all-files         # before opening a PR
$PREKCOMMAND run                     # changed files only
```

CMake build options: `PHLEX_USE_FORM`, `ENABLE_TSAN`, `ENABLE_ASAN`,
`ENABLE_COVERAGE`, `ENABLE_CLANG_TIDY`, `ENABLE_PERFETTO`.

Environment variables: `PHLEX_PLUGIN_PATH` (plugin search), `PYTHONPATH`
(user modules only — never append system/Spack site-packages), `SPDLOG_LEVEL`.

## Code Style and Formatting

### C++

**Authoritative sources**: `.clang-format` and `.clang-tidy` are the ground
truth for formatting and naming. Documentation may lag behind them. As of
2026-04, clang-tidy is monitored but **not yet enforced** (`WarningsAsErrors`
is empty); an ongoing effort tracked in `docs/dev/clang-tidy-fixes-2026-04.md`
is resolving the ~279 `readability-identifier-naming` violations and other
outstanding findings before enforcement is tightened.

- clang-format: `.clang-format` — 100-char line limit, 2-space indent,
  `QualifierAlignment: Right` (east-const), `PointerAlignment: Left`
- clang-tidy: `.clang-tidy` — enables bugprone, cert, clang-analyzer,
  concurrency, cppcoreguidelines, misc, modernize, performance, portability,
  readability checks; see file for full list of disabled sub-checks
- Header files: `.hpp`; implementation: `.cpp`; tests: `*_test.cpp`
- Namespace: `phlex::` (core), `phlex::experimental::` (experimental)
- **Naming** (from `.clang-tidy` `readability-identifier-naming`):
  - All identifiers: `lower_case` — namespaces, classes, structs, enums,
    functions, variables, parameters, members, constants, type aliases
  - Exception: template parameters use `CamelCase`
  - Exception: macros use `UPPER_CASE`
  - Exception: executable names use hyphens (e.g. `phlex-program`)
  - Private, protected, and constant members get a trailing underscore (`_`)
  - No trailing underscore on anything else
  - *Note*: some older code uses PascalCase for classes — this was under
    discussion but `.clang-tidy` settles it as `lower_case`; new code must
    follow `.clang-tidy`, existing violations are being fixed incrementally
- Functors (agent nouns): `ModelEvaluator evaluate_model(...)`
- `east-const` style: `int const x` not `const int x`
- `enum class` preferred over plain `enum`
- Avoid boolean parameters; prefer enumerations in interfaces
- `std::shared_ptr` for shared ownership; `std::unique_ptr` for exclusive;
  raw pointers for non-owning references only

### Python

- ruff (configured in `pyproject.toml`): 99-char limit, double quotes
- Google-style docstrings; type hints recommended (mypy configured)
- Use `from __future__ import annotations` to enable deferred evaluation of
  type annotations (avoids forward-reference issues; Python >=3.12)
- Test files: `test_*.py`; do NOT name files after stdlib modules (e.g. `types.py`)
- PEP 8 naming; `CapWords` for classes, `snake_case` for everything else

### CMake

- gersemi or cmake-format: `dangle_align: "child"`, `dangle_parens: true`

### Jsonnet

- `jsonnetfmt` formatting; CI enforces this
- Used for workflow configurations in `test/` and elsewhere

### Markdown

- MD012: no multiple consecutive blank lines
- MD022: headings surrounded by exactly one blank line
- MD031: fenced code blocks surrounded by exactly one blank line
- MD032: lists surrounded by exactly one blank line
- MD034: no bare URLs — use `[text](url)` syntax
- MD036: use `#` headings, not `**Bold**` for section titles
- MD040: always specify language on fenced code blocks

## Python/C++ Integration Details

- Type conversion in `plugins/python/src/modulewrap.cpp` uses substring
  matching on type names — brittle. Ensure converters exist for every type
  used in tests. Exact matches required: `numpy.float32 != float`.
- Python test structure: C++ driver supplies data streams (e.g.
  `test/python/driver.cpp`), Jsonnet config wires the graph, Python script
  implements the algorithms.
- Python/C++ reference counting: manual `Py_INCREF`/`Py_DECREF`; use
  `PyGILRAII` RAII wrapper for GIL management.
- GC-tracked Python types: `Py_TPFLAGS_HAVE_GC`, implement `tp_traverse`
  and `tp_clear`, call `PyObject_GC_UnTrack` before deallocation.
- Error handling: `std::runtime_error` for C++ runtime failures; propagate
  Python exceptions via `PyErr_SetString`/`PyErr_Format`; return `nullptr`
  on error; `PyErr_Clear()` when recovering.

## Git and PR Workflow

- Fork-based development; PRs target `Framework-R-D/phlex` `main`.
- Both external contributors and framework developers need PR review before merge.
- PRs must pass all CI checks; keep changes minimal and focused.
- If changes break or invalidate `phlex-examples`: create an issue in
  `Framework-R-D/phlex-examples` (or `Framework-R-D/phlex` if not possible).
- If changes require documentation updates: create an issue in
  `Framework-R-D/phlex-design` (or `Framework-R-D/phlex` if not possible).
- If changes affect Spack recipes: create an issue in
  `Framework-R-D/phlex-spack-recipes` (or `Framework-R-D/phlex` if not possible).

### Working with PRs via gh CLI

`gh` is installed in the devcontainer. With a valid `GH_TOKEN` or host-level
`gh auth login`, the full PR lifecycle is available on the command line.

```bash
# Read a PR's review comments (inline code review threads)
gh api repos/Framework-R-D/phlex/pulls/<N>/comments

# Read a PR's issue-level comments (general discussion)
gh api repos/Framework-R-D/phlex/issues/<N>/comments

# Read a PR's review objects (approved/changes-requested/commented)
gh api repos/Framework-R-D/phlex/pulls/<N>/reviews

# Respond to a review comment (reply to thread)
gh api repos/Framework-R-D/phlex/pulls/<N>/comments \
  --method POST \
  --field body="..." \
  --field in_reply_to=<comment_id>

# Post a general PR comment
gh pr comment <N> --repo Framework-R-D/phlex --body "..."

# Mark a review thread as resolved (requires GraphQL)
gh api graphql -f query='
  mutation { resolveReviewThread(input: {threadId: "<thread_node_id>"}) {
    thread { isResolved } } }'

# Check PR status / CI results
gh pr view <N> --repo Framework-R-D/phlex
gh pr checks <N> --repo Framework-R-D/phlex

# List open PRs
gh pr list --repo Framework-R-D/phlex
```

To get `thread_node_id` for GraphQL resolution, use:

```bash
gh api repos/Framework-R-D/phlex/pulls/<N>/comments --jq \
  '.[] | {id, node_id, path, line, body}'
```

### Responding to PR Review Comments — Strategy

When asked to address PR review comments:

1. Fetch all review comments with `gh api` (above) to read them in full
   without loading the GitHub web UI.
2. Group comments by file/topic before starting edits — avoids redundant
   passes over the same file.
3. After pushing fixes, reply to each addressed comment thread via
   `gh api ... --field in_reply_to=<id>` explaining what was changed and
   why, rather than just "done". Reviewers get email; keep replies informative.
4. Do **not** resolve threads on behalf of the reviewer — resolving is the
   reviewer's acknowledgement that the concern is addressed. Post a reply
   instead.
5. If a review comment requests a change you disagree with, reply with
   reasoning before pushing an alternative — don't silently implement a
   different approach.
6. For `@phlexbot` auto-fix commands (formatting, etc.), prefer triggering
   them rather than manually reformatting, to keep formatting commits
   attributable to the bot and separate from logic changes.

### `@phlexbot` CI Bot Commands

Post these as PR comments (requires `OWNER`, `COLLABORATOR`, or `MEMBER`
association). The bot name is `phlexbot` (derived from the repo name).

| Comment | Effect |
| ------- | ------ |
| `@phlexbot format` | Run **all** format fixers in parallel and push a single combined commit |
| `@phlexbot clang-fix` | clang-format only |
| `@phlexbot cmake-fix` | gersemi (CMake) only |
| `@phlexbot python-fix` | ruff (Python) only |
| `@phlexbot markdown-fix` | markdownlint only |
| `@phlexbot jsonnet-fix` | jsonnetfmt only |
| `@phlexbot yaml-fix` | prettier (YAML) only |
| `@phlexbot header-guards-fix` | Header guard fixer only |
| `@phlexbot tidy-fix [check1,check2,...]` | Apply clang-tidy fixes (optionally scoped to named checks) |
| `@phlexbot tidy-check` | Run clang-tidy check (read-only, posts results) |
| `@phlexbot build` | Trigger cmake-build workflow |
| `@phlexbot coverage` | Trigger coverage workflow |

Fix workflows push commits directly to the PR branch and require
`WORKFLOW_PAT` to be set as a repository secret on the fork. Check workflows
are read-only and post results as PR comments.

## Local GitHub Actions Testing

Use `act` to run workflows locally. **Do not overwrite `.actrc`** — it
contains the full runner configuration. Inside the devcontainer, `DOCKER_HOST`
is set automatically. On the host:

```bash
systemctl --user enable --now podman.socket
export DOCKER_HOST=unix://${XDG_RUNTIME_DIR}/podman/podman.sock
act -l                    # list jobs
act -j <job_name>         # run specific job
act pull_request          # run PR event
```

## Jules AI Agent

`dev/jules/` contains a dedicated environment for the Jules AI agent. When
assigning tasks to Jules, instruct it to run
`dev/jules/prepare-environment.sh` before starting work.
