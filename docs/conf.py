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
copyright = "2025, The PebbleOS Contributors"
author = "The PebbleOS Contributors"
release = "0.1.0"

# -- General configuration ---------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#general-configuration

extensions = [
    "myst_parser",
    "sphinx_copybutton",
    "sphinx_design",
    "pbl_docs",
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
        {"title": "pbl CLI", "url": "pbl/index"},
        {"title": "Architecture", "url": "architecture/index"},
        {"title": "Boards", "url": "boards/index"},
        {
            "title": "API reference",
            "url": "https://pebbleos-core.readthedocs.io/en/latest/apidoc/index.html",
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

# Pages that moved, served as redirects from their old URL
pbl_redirects = {
    "development/quickstart": "getting_started/index",
    "development/getting_started": "getting_started/index",
    "development/options": "pbl/configure",
    "development/building_fw": "pbl/flash",
    "development/pbl": "pbl/index",
    "development/build_system": "development/build_system/index",
    "development/qemu": "development/emulator",
    "development/native": "development/native_simulator",
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
    "colon_fence",
    "deflist",
    "fieldlist",
]
