# Building the HTML documentation

Phlex's documentation uses Sphinx to render RST files and Breathe to incorporate
C++ declarations and comments from Doxygen XML. The `bizstyle` theme is included
with Sphinx; no separate theme package is required.

## Create a conda environment

These instructions assume conda is installed. Create a fresh environment rather
than modifying an existing environment used by the design documentation:

```bash
conda create -n for-phlex-docs --override-channels -c conda-forge \
  python=3.13 pip doxygen graphviz
```

Run the following commands from the Phlex repository root, such as
`~/work/phlex`:

```bash
conda run -n for-phlex-docs python -m pip install \
  -r docs/requirements.txt

conda run -n for-phlex-docs python -m pip check
```

Use conda for Python and native tools, and pip for the Sphinx-related packages.
The conda-forge Breathe 4.36.0 package requires Sphinx below version 9, which
conflicts with this repository's Sphinx requirement. Installing the Python
packages from PyPI avoids that packaging constraint. Do not subsequently install
Breathe or Sphinx through conda in this environment.

Graphviz is included for diagrams in the neighboring `phlex-design` repository;
it is not required by Phlex's current documentation.

## Build Phlex documentation

Run from the Phlex repository root:

```bash
conda run -n for-phlex-docs doxygen docs/Doxyfile

conda run -n for-phlex-docs sphinx-build \
  -E -W --keep-going -b html docs/source docs/build/html
```

Doxygen must run first to populate `docs/doxygen/xml`. It processes `.hpp` files
under `phlex/` and `form/`, excluding `phlex/model/type_id.hpp` and paths matching
`*/detail/*`. Implementation files are not processed.

Sphinx's `-E` option rebuilds its environment. The `-W` option converts warnings
to errors and may be omitted if warnings should not cause the build to fail.
The `--keep-going` option continues reporting problems across the documentation.

Open `docs/build/html/index.html` in a browser. On macOS:

```bash
open docs/build/html/index.html
```

Generated XML and HTML are ignored by Git. No C++ compilation is required for
this documentation build.

## Also build Phlex design documentation

The design repository uses `sphinx_needs`. Its older version 5.1.0 requires
Sphinx below version 9 and cannot satisfy Phlex's current requirements. Install
the newer version in the fresh environment, running from the Phlex repository
root:

```bash
conda run -n for-phlex-docs python -m pip install \
  -r docs/requirements.txt "sphinx-needs==9.2.0"

conda run -n for-phlex-docs python -m pip check
```

For a design checkout at `~/work/phlex-design`, render its HTML with:

```bash
conda run -n for-phlex-docs sphinx-build \
  -E -W --keep-going -b html \
  ~/work/phlex-design/doc ~/work/phlex-design/doc/build/html
```

Dependency resolution for Sphinx 9.2.0, Breathe 4.36.0, and sphinx-needs 9.2.0
has been checked. Phlex's HTML build has been verified, but the design
repository's build with the major sphinx-needs upgrade has not. Its configuration
or custom extensions may require changes.

These instructions cover HTML only. PDF builds require additional LaTeX tools.

## Update the phlexdoc container

The published `ghcr.io/framework-r-d/phlexdoc:2025-06-06` image has a build recipe
in the [phlex-design Dockerfile](https://github.com/Framework-R-D/phlex-design/blob/main/Dockerfile).
The design repository's documentation workflow consumes this image; it does not
build or publish it.

To update that Dockerfile for both repositories, retain the TeX Live base image
if PDF builds are still needed, and retain the Miniforge installation. Replace
the existing Conda initialization and documentation-package installation steps
with the following, after Miniforge has been installed:

```dockerfile
ENV PATH="/root/miniforge3/bin:$PATH"
RUN conda install --yes --name base --override-channels -c conda-forge \
      python=3.13 pip doxygen "graphviz>=12"
RUN python -m pip install \
      "sphinx>=9.1,<10" "breathe>=4.36,<5" \
      "sphinx-needs==9.2.0" graphviz2drawio \
    && python -m pip check
```

The Sphinx and Breathe constraints above mirror `docs/requirements.txt`; keep
them synchronized when updating the image. `sphinx-needs` and `graphviz2drawio`
support the design documentation. Do not subsequently install Sphinx or Breathe
through Conda.

Installing into Miniforge's `base` environment keeps `python`, `doxygen`, and
`sphinx-build` on `PATH` for existing workflows. No `conda activate`, interactive
shell initialization, or named environment is needed in the container.

Before publishing a replacement tag, verify Phlex's Doxygen and HTML builds and
the design repository's HTML and PDF builds inside the image. The dependency
check alone does not verify compatibility with the design configuration or its
custom extensions, especially after the major sphinx-needs upgrade. Update
consuming workflows to the new tag only after those builds pass.

This is not a fully reproducible dependency lock: the existing Dockerfile
downloads the latest Miniforge installer, and several package versions are
specified as ranges or left unpinned. Exact reproduction requires pinning the
installer, base image digest, and resolved package versions separately.
