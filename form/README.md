# FORM (Fine-grained Object Reading/Writing Model)

FORM is a prototype C++ I/O stack that underpins Phlex data handling. It offers a fine-grained object model, multiple storage backends, and utilities for testing and experimentation.

## Component Overview

- **Core** — fundamental building blocks such as token definitions and placement helpers.
- **FORM** — the top-level library that exposes the public API.
- **Mock Phlex** — lightweight stand-ins for the Phlex runtime used during FORM testing.
- **Persistence** — serialisation and deserialisation helpers.
- **Storage** — abstraction layer for persistence backends.
- **ROOT storage** — optional storage implementation backed by CERN ROOT I/O.
- **Utilities** — shared helpers used throughout the FORM codebase.

## Building and Testing

FORM builds as part of the main Phlex project. The standard developer workflow looks like:

```bash
# 1. Initialise the environment (Spack/system toolchains)
. scripts/setup-env.sh

# 2. Configure using the default preset
cmake --preset default -S . -B build

# 3. Build the FORM libraries and tests
cmake --build build --target form mock_phlex

# 4. Run the FORM integration tests
ctest --test-dir build --output-on-failure -R "form|ReadVector|WriteVector"
```

Key notes:

- The CMake option `FORM_USE_ROOT_STORAGE` (enabled by default) controls whether ROOT-backed storage components and tests are compiled.
- Executables such as `phlex_writer` and `phlex_reader` are placed under `build/test/form/`.
- Tests produce the sample file `toy.root`; re-running the reader test validates both serialisation and deserialisation.

To inspect the generated ROOT file interactively:

```cpp
TFile* file = TFile::Open("toy.root");
file->ls();
auto* tree_segment = static_cast<TTree*>(file->Get("<ToyAlg_Segment>"));
tree_segment->Print();
auto* tree_event = static_cast<TTree*>(file->Get("<ToyAlg_Event>"));
tree_event->Print();
tree_segment->Scan();
tree_event->Scan();
```

## Development Status

FORM remains under active development. Functionality, APIs, and storage adapters may change; refer to in-source `TODO`/`FIXME` notes for known limitations and planned enhancements.
