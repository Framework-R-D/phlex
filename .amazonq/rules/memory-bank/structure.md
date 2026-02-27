# Project Structure

## Directory Organization

### Core Framework (`phlex/`)

The main framework implementation containing the execution engine and core abstractions.

- **`app/`**: Main application entry point and CLI
  - `phlex.cpp`: Main executable
  - `load_module.cpp/hpp`: Dynamic module loading
  - `run.cpp/hpp`: Workflow execution logic
  - `version.cpp.in/hpp`: Version information

- **`core/`**: Core framework abstractions and execution engine
  - Algorithm declarations: `declared_provider.cpp/hpp`, `declared_transform.cpp/hpp`, `declared_observer.cpp/hpp`, `declared_fold.cpp/hpp`, `declared_unfold.cpp/hpp`, `declared_predicate.cpp/hpp`, `declared_output.cpp/hpp`
  - Graph execution: `framework_graph.cpp/hpp`, `edge_maker.cpp/hpp`, `edge_creation_policy.cpp/hpp`
  - Data flow: `message.cpp/hpp`, `message_sender.cpp/hpp`, `consumer.cpp/hpp`
  - Product management: `product_query.cpp/hpp`, `products_consumer.cpp/hpp`
  - Registration: `registrar.cpp/hpp`, `registration_api.cpp/hpp`, `node_catalog.cpp/hpp`
  - Utilities: `filter.cpp/hpp`, `glue.cpp/hpp`, `multiplexer.cpp/hpp`
  - `fold/`: Fold operation implementations
  - `detail/`: Internal implementation details

- **`model/`**: Data model and type system
  - `product_store.cpp/hpp`: Central data product storage
  - `product_specification.cpp/hpp`, `product_matcher.cpp/hpp`: Product identification
  - `products.cpp/hpp`: Product collections
  - `data_layer_hierarchy.cpp/hpp`: Hierarchical data organization
  - `data_cell_index.cpp/hpp`, `data_cell_counter.cpp/hpp`: Data cell management
  - `handle.hpp`: Type-safe handles to data products
  - `identifier.cpp/hpp`, `algorithm_name.cpp/hpp`: Naming and identification
  - `type_id.hpp`: Type identification system

- **`graph/`**: Graph-based execution components
  - `serializer_node.cpp/hpp`: Node serialization
  - `serial_node.hpp`: Serial execution nodes

- **`metaprogramming/`**: Template metaprogramming utilities
  - `delegate.hpp`: Function delegation
  - `type_deduction.hpp`: Type deduction helpers
  - `detail/`: Internal metaprogramming utilities

- **`utilities/`**: General utilities
  - `resource_usage.cpp/hpp`: Resource monitoring
  - `hashing.cpp/hpp`: Hash functions
  - `async_driver.hpp`: Asynchronous execution
  - `max_allowed_parallelism.hpp`: Parallelism control
  - `thread_counter.hpp`: Thread tracking
  - `simple_ptr_map.hpp`, `sized_tuple.hpp`, `sleep_for.hpp`, `string_literal.hpp`: Various utilities

- **Top-level files**:
  - `configuration.cpp/hpp`: Configuration management
  - `module.hpp`: Module interface
  - `source.hpp`: Data source interface
  - `driver.hpp`: Execution driver interface
  - `concurrency.hpp`: Concurrency primitives

### Plugins (`plugins/`)

Extensibility layer for language bindings and code generation.

- **`python/`**: Python integration plugin
  - `src/`: C++ implementation of Python bindings
    - `modulewrap.cpp`: Module wrapping for Python
    - `lifelinewrap.cpp`: Lifeline management
  - `python/`: Python package code
  - `CMakeLists.txt`: Build configuration

- **Root level**:
  - `layer_generator.cpp/hpp`: Code generation for data layers
  - `generate_layers.cpp`: Layer generation tool

### FORM Integration (`form/`)

Optional data persistence layer (enabled with `PHLEX_USE_FORM`).

- **`core/`**: Core FORM abstractions
  - `placement.cpp/hpp`: Data placement
  - `token.cpp/hpp`: Token management

- **`form/`**: FORM framework integration
  - `form.cpp/hpp`: Main FORM interface
  - `config.cpp/hpp`: FORM configuration
  - `technology.hpp`: Technology abstractions

- **`persistence/`**: Persistence layer
  - `ipersistence.hpp`: Persistence interface
  - `persistence.cpp/hpp`: Persistence implementation

- **`storage/`**: Storage backends
  - `istorage.hpp`: Storage interface
  - `storage.cpp/hpp`: Storage implementation
  - `storage_file.cpp/hpp`: File-based storage
  - `storage_container.cpp/hpp`: Container storage
  - `storage_associative_container.cpp/hpp`: Associative container storage
  - `storage_association.cpp/hpp`: Storage associations

- **`root_storage/`**: ROOT file format support
  - `root_storage.hpp`: ROOT storage interface
  - `root_tfile.cpp/hpp`: ROOT TFile wrapper
  - `root_ttree_container.cpp/hpp`: ROOT TTree wrapper
  - `root_tbranch_container.cpp/hpp`: ROOT TBranch wrapper

- **`util/`**: FORM utilities
  - `factories.hpp`: Factory patterns

### Tests (`test/`)

Comprehensive test suite covering all framework components.

- **`benchmarks/`**: Performance benchmarks
- **`python/`**: Python integration tests
- **`form/`**: FORM integration tests
- **`demo-giantdata/`**: Large-scale data processing demos
- **`mock-workflow/`**: Mock workflow for testing
- **`max-parallelism/`**: Parallelism tests
- **`memory-checks/`**: Memory usage tests
- **`plugins/`**: Plugin system tests
- **`utilities/`**: Utility tests
- **Root level**: Unit tests for core components

### Build System (`Modules/`)

CMake modules for build configuration.

- `Findjsonnet.cmake`, `FindPerfetto.cmake`: Find modules for dependencies
- `NProc.cmake`: Processor count detection
- **`private/`**: Internal build utilities
  - `CreateClangTidyTargets.cmake`: Static analysis targets
  - `CreateCoverageTargets.cmake`: Coverage reporting targets
  - `PhlexTargetUtils.cmake`: Build target utilities

### Scripts (`scripts/`)

Development and CI automation scripts.

- `coverage.sh`: Coverage workflow automation
- `normalize_coverage_xml.py`, `normalize_coverage_lcov.py`: Coverage path normalization
- `export_llvm_lcov.py`: LLVM coverage export
- `create_coverage_symlinks.py`: Coverage symlink management
- `check_codeql_alerts.py`, `codeql_reset_dismissed_alerts.py`: CodeQL integration
- `sarif-alerts.py`: SARIF format handling
- `setup-env.sh`: Environment setup

### CI/CD (`.github/`)

GitHub Actions workflows and configurations.

- **`workflows/`**: CI/CD pipelines for building, testing, formatting, linting, and coverage
- **`actions/`**: Reusable GitHub Actions
- `dependabot.yml`: Dependency updates
- `CODEOWNERS`: Code ownership
- `copilot-instructions.md`: GitHub Copilot guidelines

### Development Environment (`.devcontainer/`)

VS Code devcontainer configuration for consistent development environments.

## Architectural Patterns

### Graph-Based Execution

The framework builds a directed acyclic graph (DAG) of algorithm nodes with automatic dependency resolution and parallel execution using Intel TBB.

### Product Store Pattern

Central data product storage with type-safe handles enables decoupled algorithm communication.

### Plugin Architecture

Dynamic module loading allows users to extend the framework without modifying core code.

### Hierarchical Data Model

Multi-level data organization (e.g., run → subrun → event) supports complex scientific workflows.

### Type Erasure

Template-based type erasure enables heterogeneous algorithm collections with type safety.

### Configuration as Code

Jsonnet-based configuration provides programmatic workflow definition with inheritance and composition.
