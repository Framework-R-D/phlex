# mypy: ignore-errors
"""Sphinx configuration for Phlex documentation."""

# -- Project information -----------------------------------------------------

project = "Phlex"
copyright = "2025-2026, The Phlex Developers"
author = "The Phlex Developers"
release = "0.4.3"

# -- General configuration ---------------------------------------------------

extensions = [
    "breathe",
]

templates_path = []
exclude_patterns = []

# -- Options for HTML output -------------------------------------------------

html_theme = "bizstyle"
html_sidebars = {
    "**": [
        "globaltoc.html",
        "localtoc.html",
        "relations.html",
        "sourcelink.html",
        "searchbox.html",
    ],
}
html_logo = "_static/phlex-logo.svg"
html_static_path = ["_static"]
html_style = "phlexframework.css"
html_last_updated_fmt = ""

# -- Breathe configuration -------------------------------------------------

breathe_projects = {
    "Phlex": "../doxygen/xml/",
}
breathe_default_project = "Phlex"
