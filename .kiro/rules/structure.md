# Project Structure

## Source Layout

```text
phlex/                    # C++ framework source
  app/                    # Main executable and CLI
  core/                   # Execution engine, graph, algorithm declarations
    fold/                 # Fold operation implementations
    detail/               # Internal implementation details
  model/                  # Data model and type system
  graph/                  # Graph execution nodes
  metaprogramming/        # Template metaprogramming utilities
  utilities/              # Resource monitoring, hashing, async helpers
form/                     # Optional FORM/ROOT data persistence layer
  core/                   # Placement, token management
  form/                   # Main FORM interface and config
  persistence/            # Persistence layer
  storage/                # Storage backends
  root_storage/           # ROOT TFile/TTree/TBranch wrappers
  util/                   # Factory patterns
plugins/                  # Extensibility layer
  python/                 # Python plugin (C API + NumPy)
    src/                  # C++ implementation (modulewrap.cpp, lifelinewrap.cpp)
    python/               # Python package code
  layer_generator.cpp/hpp # Code generation for data layers
  generate_layers.cpp     # Layer generation tool
test/                     # Test suite
  benchmarks/             # Performance benchmarks
  demo-giantdata/         # Large-scale data processing demos
  form/                   # FORM integration tests
  max-parallelism/        # Parallelism tests
  memory-checks/          # Memory usage tests
  mock-workflow/          # Mock workflow for testing
  plugins/                # Plugin system tests
  python/                 # Python integration tests
  tbb-preview/            # TBB preview feature tests
  utilities/              # Utility tests
  *.cpp                   # Core unit tests
scripts/                  # Development and CI automation
  setup-env.sh            # Environment setup (multi-mode: standalone/workspace/Spack)
  coverage.sh             # Coverage workflow automation
  fix_header_guards.py    # C++ header guard fixer
  normalize_coverage_xml.py
  normalize_coverage_lcov.py
  export_llvm_lcov.py
  create_coverage_symlinks.py
  check_codeql_alerts.py
  README.md               # Detailed setup documentation
  QUICK_REFERENCE.md      # Quick-start commands
Modules/                  # CMake modules
  private/                # Internal build utilities
ci/                       # CI container definitions
  Dockerfile              # phlex-ci image
  spack.yaml              # Spack environment for CI
dev/
  jules/                  # Jules AI agent environment
    Dockerfile            # Ubuntu 24.04 test image
    prepare-environment.sh
    README.md
.devcontainer/            # VS Code devcontainer configuration
.github/
  workflows/              # CI/CD pipelines
  actions/                # Reusable GitHub Actions
  copilot-instructions.md # GitHub Copilot guidelines
.kiro/
  rules/                  # Kiro AI agent guidelines (this tree)
.amazonq/
  rules/                  # Amazon Q guidelines
```

## Codespace / Devcontainer Layout

In a GitHub Codespace or devcontainer, companion repositories are cloned
automatically alongside the primary repository:

```text
/workspaces/phlex                    # primary repository (workspace root)
/workspaces/phlex-design             # design documentation
/workspaces/phlex-examples           # example programs
/workspaces/phlex-coding-guidelines  # contributor coding guidelines
/workspaces/phlex-spack-recipes      # Spack recipes
```

Open `.devcontainer/codespace.code-workspace` for a multi-root VS Code window.
Git hooks are installed automatically via `postCreateCommand` (`prek install`).

## Key Files

| File | Purpose |
| ---- | ------- |
| `CMakeLists.txt` | Top-level build definition |
| `CMakePresets.json` | Build presets (default, coverage-gcc, coverage-clang) |
| `pyproject.toml` | Python tool configuration (ruff, mypy) |
| `.clang-format` | C++ formatting rules (100-char limit, 2-space indent) |
| `.clang-tidy` | C++ static analysis rules |
| `.gersemirc` | CMake formatting rules |
| `.cmake-format.json` | cmake-format rules |
| `.markdownlint.jsonc` | Markdown linting rules |
| `.prettierrc.yaml` | YAML formatting rules |
| `.pre-commit-config.yaml` | Pre-commit hook configuration |
| `.actrc` | Local `act` configuration for GitHub Actions |
| `codecov.yml` | Codecov configuration |
| `scripts/setup-env.sh` | Environment setup script |

## Architectural Patterns

- **Graph-based execution**: DAG of algorithm nodes, automatic dependency
  resolution, parallel execution via Intel TBB
- **Product store**: Central type-safe data product storage; algorithms
  communicate through it rather than directly
- **Plugin architecture**: Dynamic module loading; users extend the framework
  without modifying core code
- **Hierarchical data model**: Multi-level data organization
  (e.g., run → subrun → event)
- **Type erasure**: Template-based heterogeneous algorithm collections
- **Configuration as code**: Jsonnet-based workflow definition with
  inheritance and composition
