# Phlex Python Plugin Architecture

This directory contains the C++ source code for the Phlex Python plugin, which enables Phlex to execute Python code as part of its computation graph.

## Architecture Overview

The integration is built on the **Python C API** (not `pybind11`) to maintain strict control over the interpreter lifecycle and memory management.

### 1. The "Type Bridge" (`modulewrap.cpp`)

The core of the integration is the type conversion layer in `src/modulewrap.cpp`. This layer is responsible for:
- Converting Phlex `Product` objects (C++) into Python objects (e.g., `PyObject*`, `numpy.ndarray`).
- Converting Python return values back into Phlex `Product` objects.

**Critical Implementation Detail:**
The type mapping relies on **string comparison** of type names.

- **Mechanism**: The C++ code checks whether `type_name()` contains `"float64]]"` to identify a 2D array of doubles.
- **Brittleness**: This is a fragile contract. If the type name changes (e.g., `numpy` changes its string representation) or if a user provides a slightly different type (e.g., `float` vs `np.float32`), the bridge may fail.
- **Extension**: When adding support for new types, you must explicitly add converters in `modulewrap.cpp` for both scalar and vector/array versions.

### 2. Hybrid Configuration

Phlex uses a hybrid configuration model involving three languages:

1. **Jsonnet** (`*.jsonnet`): Defines the computation graph structure. It specifies:
    - The nodes in the graph.
    - The Python module/class to load for specific nodes.
    - Configuration parameters passed to the Python object.
2. **C++ Driver**: The executable that:
    - Parses the Jsonnet configuration.
    - Initializes the Phlex core.
    - Loads the Python interpreter and the specified plugin.
3. **Python Code** (`*.py`): Implements the algorithmic logic.

### 3. Environment & Testing

Because the Python interpreter is embedded within the C++ application, the runtime environment is critical.

- **PYTHONPATH**: Must be set correctly to include:
  - The build directory (for generated modules).
  - The source directory (for user scripts).
  - Do not append system/Spack `site-packages`; `pymodule.cpp` adjusts `sys.path` based on `CMAKE_PREFIX_PATH` and active virtual environments.
- **Naming Collisions**:
    - **Warning**: Do not name test files `types.py`, `test.py`, `code.py`, or other names that shadow standard library modules.
    - **Consequence**: Shadowing can cause obscure failures in internal libraries (e.g., `numpy` failing to import because it tries to import `types` from the standard library but gets your local file instead).

## Development Guidelines

1.  **Adding New Types**:
    - Update `src/modulewrap.cpp` to handle the new C++ type.
    - Add a corresponding test case in `test/python/` to verify the round-trip conversion.
2.  **Testing**:
    - Use `ctest` to run tests.
    - Tests are integration tests: they run the full C++ application which loads the Python script.
    - Debugging: Use `ctest --output-on-failure` to see Python exceptions.
