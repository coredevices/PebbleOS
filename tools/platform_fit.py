# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Which binaries of a pbw a watch runs (docs/sdk/platforms.md).

The rule, as whatever installs a pbw applies it: the phone app, pebble-tool.
Each has its own implementation; tests/platform_fit_cases.json is the table
of cases all of them are held to, this one included.

A platform is described as in the `platform` object of a pbw's
manifest.json: name, abi, display, features and limits.app_memory.
"""

# Oldest first. A platform that is not listed is newer than these.
AGE = ("aplite", "basalt", "chalk", "diorite", "emery", "flint", "gabbro")

SHAPES = ("rect", "round")


def fits(watch, binary):
    """Whether a watch runs a binary built for another platform."""
    display, built_for = watch["display"], binary["display"]
    return (
        binary["abi"] == watch["abi"]
        and built_for["shape"] in SHAPES
        and built_for["shape"] == display["shape"]
        and built_for["width"] <= display["width"]
        and built_for["height"] <= display["height"]
        and (display["color"] or not built_for["color"])
        and binary["limits"]["app_memory"] <= watch["limits"]["app_memory"]
    )


def _rank(watch, binary):
    display, built_for = watch["display"], binary["display"]
    name = binary["name"]
    return (
        name != watch["name"],
        built_for != display,
        -(built_for["width"] * built_for["height"]),
        built_for["color"] != display["color"],
        -(AGE.index(name) if name in AGE else len(AGE)),
        name,
    )


def best_first(watch, binaries):
    """The binaries a watch runs, the one to install first."""
    return sorted(
        (b for b in binaries if fits(watch, b)), key=lambda b: _rank(watch, b)
    )
