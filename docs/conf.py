# SPDX-FileCopyrightText: 2025 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "_ext"))

# -- Project information -----------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#project-information

project = "PebbleOS"
copyright = "2026, The PebbleOS Contributors"
author = "The PebbleOS Contributors"
release = "0.1.0"

# -- General configuration ---------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#general-configuration

extensions = [
    "myst_parser",
    "sphinx_copybutton",
    "sphinx_design",
    "sphinxcontrib.mermaid",
    "matplotlib.sphinxext.plot_directive",
    "pbl_docs",
    "pbl_playground",
    "kconfig",
]

exclude_patterns = ["_build", "_apidoc", "_ext", "Thumbs.db", ".DS_Store"]

nitpicky = True

# -- Options for HTML output -------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#options-for-html-output

html_theme = "shibuya"
templates_path = ["_templates"]
html_static_path = ["_static"]

# Doxygen output built by the RTD pre_build job (see .readthedocs.yaml),
# published verbatim under /apidoc/
if os.path.isdir(os.path.join(os.path.dirname(__file__), "_apidoc")):
    html_extra_path = ["_apidoc"]

html_favicon = "_static/images/favicon.svg"
html_css_files = ["css/custom.css"]
html_theme_options = {
    "light_logo": "_static/images/logo.svg",
    "dark_logo": "_static/images/logo.svg",
    "accent_color": "tomato",
    "github_url": "https://github.com/coredevices/pebbleos",
    "nav_links": [
        {"title": "Getting started", "url": "getting_started/index"},
        {"title": "Architecture", "url": "architecture/index"},
        {"title": "Boards", "url": "boards/index"},
        {
            "title": "API reference",
            "url": "https://pebbleos.readthedocs.io/en/latest/apidoc/index.html",
        },
    ],
}
html_context = {
    "source_type": "github",
    "source_user": "coredevices",
    "source_repo": "pebbleos",
    "source_version": "main",
    "source_docs_path": "/docs/",
}

# The firmware build behind the playground, left out of Read the Docs pull
# request previews unless labelled ci:docs-playground (see .readthedocs.yaml)
pbl_playground = os.environ.get("READTHEDOCS_VERSION_TYPE") != "external" or (
    os.path.exists(os.path.join(os.path.dirname(__file__), "..", ".pbl_playground"))
)

# Boards the playground offers, with their label; the first one is the default
pbl_playground_boards = {
    "native_emery/wasm": "Pebble Time 2",
    "native_gabbro/wasm": "Pebble Round 2",
    "native_flint/wasm": "Pebble 2 Duo",
}

# Pages that moved, served as redirects from their old URL
pbl_redirects = {
    "development/quickstart": "getting_started/index",
    "development/getting_started": "getting_started/index",
    "development/options": "pbl/configure",
    "development/building_fw": "pbl/run",
    "pbl/flash": "pbl/run",
    "development/pbl": "pbl/index",
    "development/build_system": "development/build_system/index",
    "development/qemu": "development/emulation/emulator",
    "development/native": "development/emulation/native_simulator",
    "development/testing": "development/tests/unit",
    "development/integration_tests": "development/tests/integration",
    "development/sdk_export": "sdk/exporting",
    "reference/recognizers": "sdk/recognizers",
    "development/sbom": "third_party/sbom",
    "development/moddable": "third_party/moddable",
    "development/contributing": "community/contributing",
    "architecture/kernel": "kernel/index",
    "architecture/kernel_internals": "kernel/internals",
    "architecture/crc": "subsystems/crc",
    "architecture/input": "subsystems/input",
    "architecture/shell": "subsystems/shell",
    "architecture/task_watchdog": "subsystems/task_watchdog",
    "architecture/device": "drivers/device_model",
    "architecture/activity/index": "services/activity/index",
}

# -- Options for the matplotlib plot directive ---------------------------------

# Plots are drawn at build time from code; show only the figure, as SVG on a
# transparent background with neutral colors that read in both themes
plot_include_source = False
plot_html_show_source_link = False
plot_html_show_formats = False
plot_formats = ["svg"]
plot_pre_code = "import matplotlib.pyplot as plt"
plot_rcparams = {
    "savefig.transparent": True,
    "savefig.bbox": "tight",
    "svg.fonttype": "none",
    "font.family": "sans-serif",
    "font.size": 10,
    "text.color": "#8a8a8a",
    "axes.labelcolor": "#8a8a8a",
    "axes.edgecolor": "#8a8a8a",
    "xtick.color": "#8a8a8a",
    "ytick.color": "#8a8a8a",
    "grid.color": "#8a8a8a",
    "legend.labelcolor": "#8a8a8a",
}

# -- Options for the mermaid extension ---------------------------------------

# Draw diagrams at their natural size, shrunk only to fit the page
mermaid_height = "auto"
mermaid_init_config = {
    "startOnLoad": False,
    "flowchart": {"useMaxWidth": False},
    "block": {"useMaxWidth": False},
    "sequence": {"useMaxWidth": False},
    "class": {"useMaxWidth": False},
}

# -- Options for the kconfig extension ---------------------------------------

kconfig_generate_db = True
kconfig_gh_link_base_url = "https://github.com/coredevices/pebbleos"
kconfig_gh_link_revision = "main"

# -- Options for the linkcheck builder ----------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#options-for-the-linkcheck-builder

# Sites that are alive but block automated requests
linkcheck_ignore = [
    r"https?://reveng\.sourceforge\.net/.*",
    r"https://https://www\.itu\.int/rec/T-REC-X\.25-199610-I/en",
]

# -- Options for myst_parser extension -----------------------------------------
# https://myst-parser.readthedocs.io/en/latest/configuration.html

# Leave shell prompts and console output out of the copied text
copybutton_exclude = ".linenos, .gp, .go"

myst_enable_extensions = [
    "amsmath",
    "colon_fence",
    "deflist",
    "dollarmath",
    "fieldlist",
]
