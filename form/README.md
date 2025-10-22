# FORM (Fine-grained Object Reading/Writing Model)

FORM is a prototype C++ library for I/O infrastructure, designed to support the Phlex project. It provides a fine-grained data model for reading and writing objects, with a focus on flexibility and extensibility.

## Components

The FORM library is organized into the following components:

*   **Core:** The core component provides the fundamental building blocks of the FORM data model, including tokens and placement new.
*   **FORM:** The FORM component itself, which ties together the other components and provides the main entry point for using the library.
*   **Mock Phlex:** This component provides a mock implementation of the Phlex runtime environment, which is used for testing and development.
*   **Persistence:** The persistence component is responsible for serializing and deserializing data.
*   **Storage:** The storage component provides an abstraction layer for different storage technologies.
*   **ROOT Storage:** This component provides an implementation of the storage layer that is based on the ROOT data analysis framework.
*   **Util:** The util component provides a collection of utility classes and functions that are used throughout the library.

## Building and Running

To build the FORM library and run the tests, follow these steps:

1.  **Configure the project:**

    ```bash
    cmake -DUSE_FORM_ALONE=ON -DFORM_USE_ROOT_STORAGE=ON ../phlex/
    ```

    This will configure the project to build the FORM library in standalone mode, with the ROOT storage backend.

2.  **Build the project:**

    ```bash
    make
    ```

3.  **Run the writer test:**

    ```bash
    ./test/form/phlex_writer
    ls -l toy.root
    ```

    This will run a test that writes a sample data file called `toy.root`.

4.  **Check the output file with ROOT:**

    You can use the ROOT command-line interface to inspect the contents of the output file:

    ```cpp
    TFile* file =  TFile::Open("toy.root")
    file->ls()
    TTree* tree1 = (TTree*)file->Get("<ToyAlg_Segment>");
    tree1->Print()
    TTree* tree2 = (TTree*)file->Get("<ToyAlg_Event>");
    tree2->Print()
    tree1->Scan()
    tree2->Scan()
    ```

5.  **Run the reader test:**

    ```bash
    ./test/form/phlex_reader
    ```

    This will run a test that reads the `toy.root` data file and verifies its contents.

## Development Status

The FORM library is currently under active development and is not yet considered to be production-ready. There are a number of known issues and limitations, which are documented in the source code with "FIXME" comments.
