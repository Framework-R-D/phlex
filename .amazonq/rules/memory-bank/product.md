# Product Overview

## Project Purpose

Phlex is a framework for **P**arallel, **h**ierarchical, and **l**ayered **ex**ecution of data-processing algorithms. It provides a sophisticated execution engine designed to orchestrate complex data processing workflows with automatic parallelization, hierarchical data organization, and layered algorithm execution.

## Value Proposition

- **Automatic Parallelization**: Executes data-processing algorithms in parallel without requiring users to manage threading or concurrency
- **Hierarchical Data Organization**: Supports multi-level data hierarchies for complex scientific workflows
- **Layered Execution**: Enables algorithms to be organized in layers with automatic dependency resolution
- **Framework-Based Architecture**: Provides a plugin system where users write algorithms as modules that integrate seamlessly
- **Python Integration**: Supports both C++ and Python algorithms through a unified interface

## Key Features

### Core Capabilities

- Graph-based workflow execution with automatic parallelization using Intel TBB
- Support for multiple algorithm types: providers, transforms, observers, filters, folds, and unfolds
- Hierarchical data processing with configurable data layer hierarchies
- Product store system for managing data products between algorithms
- Configuration via Jsonnet files for flexible workflow definition

### Plugin System

- Dynamic module loading for user algorithms
- Python plugin support using C API for Python and NumPy for seamless C++/Python interoperability
- Optional cppyy technology for alternative C++/Python interoperability
- Type-safe data passing between C++ and Python algorithms
- Support for NumPy arrays and Python data structures

### Data Persistence

- Optional FORM (Framework for Output and Reconstruction Management) integration
- ROOT file format support for scientific data storage
- Configurable storage backends for different use cases

### Development Tools

- Comprehensive test suite with Catch2 framework
- Code coverage reporting with gcov/lcov integration
- Clang-tidy integration for static analysis
- Thread and address sanitizer support
- Perfetto profiling integration for performance analysis

## Target Users

### Primary Users

- **Scientific Computing Researchers**: Processing large-scale experimental data with complex workflows
- **Data Pipeline Engineers**: Building parallel data processing systems
- **Algorithm Developers**: Creating reusable data processing components

### Use Cases

- High-energy physics data processing workflows
- Multi-stage data transformation pipelines
- Event-based data analysis systems
- Scientific simulation post-processing
- Any domain requiring hierarchical, parallel data processing

## Project Status

- Version: 0.1.0 (active development)
- License: Apache 2.0 (see LICENSE and NOTICE files)
- Repository: [https://github.com/Framework-R-D/phlex](https://github.com/Framework-R-D/phlex)
- Example Code: [https://github.com/Framework-R-D/phlex-examples](https://github.com/Framework-R-D/phlex-examples)
- Code Coverage: Tracked via Codecov with automated CI integration
