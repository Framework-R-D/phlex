# Product Overview

## Purpose

Phlex is a framework for **P**arallel, **h**ierarchical, and **l**ayered
**ex**ecution of data-processing algorithms. It provides a graph-based
execution engine for orchestrating complex data-processing workflows with
automatic parallelization, hierarchical data organization, and layered
algorithm execution.

## Repository Ecosystem

- **Primary**: `Framework-R-D/phlex` — this repository
- **Design & docs**: `Framework-R-D/phlex-design`
- **Coding guidelines**: `Framework-R-D/phlex-coding-guidelines`
- **Examples**: `Framework-R-D/phlex-examples`
- **Spack recipes**: `Framework-R-D/phlex-spack-recipes`
- **Build system dependency**: `FNALssi/cetmodules`
- **Container images**: `phlex-ci` (CI), `phlex-dev` (devcontainer / local dev)

## Key Features

- Graph-based workflow execution with automatic parallelization via Intel TBB
- Algorithm types: providers, transforms, observers, filters, folds, unfolds
- Hierarchical data processing with configurable data-layer hierarchies
- Product store for type-safe data sharing between algorithms
- Jsonnet-based workflow configuration
- Python plugin support via C API (and optional cppyy)
- Optional FORM integration for ROOT-based data persistence

## Target Users

Scientific computing researchers and data-pipeline engineers building
parallel, hierarchical data-processing workflows — primarily in high-energy
physics.

## Status

- Version: 0.1.0 (active development)
- License: Apache 2.0
- Repository: <https://github.com/Framework-R-D/phlex>
- Coverage: tracked via Codecov
