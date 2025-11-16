# mypy: ignore-errors
"""Sphinx configuration for Phlex documentation."""

# -- Project information -----------------------------------------------------

project = "Phlex"
copyright = "2025-2026, The Phlex Developers"
author = "The Phlex Developers"
release = "0.1.0"

# -- General configuration ---------------------------------------------------

extensions = [
    "breathe",
]

templates_path = ["_templates"]
exclude_patterns = []

# -- Options for HTML output -------------------------------------------------

html_theme = "sphinx_rtd_theme"

# -- Breathe configuration -------------------------------------------------

breathe_projects = {
    "Phlex": "../doxygen/xml/",
}
breathe_default_project = "Phlex"
