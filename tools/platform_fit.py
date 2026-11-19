# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Which binaries of a pbw a watch runs (docs/sdk/platforms.md).

The rule, as whatever installs a pbw applies it: the phone app, pebble-tool.
Each has its own implementation; tests/platform_fit_cases.json is the table
of cases all of them are held to, this one included.

A platform is described as in the `platform` object of a pbw's
manifest.json: name, abi, display, features and limits.app_memory. A
flexible binary is marked `flexible`, has a display without a size, and
lists the features it cannot run without as `requires`.
"""

# Oldest first. A platform that is not listed is newer than these.
AGE = ("aplite", "basalt", "chalk", "diorite", "emery", "flint", "gabbro")

SHAPES = ("rect", "round")


def fits(watch, binary):
    """Whether a watch runs a binary built for another platform, or a
    flexible one built for every display of a shape and colour depth."""
    display, built_for = watch["display"], binary["display"]
    if not (
        binary["abi"] == watch["abi"]
        and built_for["shape"] in SHAPES
        and built_for["shape"] == display["shape"]
        and (display["color"] or not built_for["color"])
        and binary["limits"]["app_memory"] <= watch["limits"]["app_memory"]
    ):
        return False
    if binary.get("flexible"):
        # A feature this does not know is one the watch does not have.
        return set(binary.get("requires", [])) <= set(watch.get("features", []))
    return (
        built_for["width"] <= display["width"]
        and built_for["height"] <= display["height"]
    )


def _rank(watch, binary):
    display, built_for = watch["display"], binary["display"]
    name = binary["name"]
    flexible = binary.get("flexible", False)
    # A flexible binary draws to the whole display.
    drawn = display if flexible else built_for
    area = drawn["width"] * drawn["height"]
    return (
        name != watch["name"],
        flexible or built_for != display,
        not (flexible and built_for["color"] == display["color"]),
        -len(binary.get("requires", [])) if flexible else 0,
        -area,
        built_for["color"] != display["color"],
        -(AGE.index(name) if name in AGE else len(AGE)),
        name,
    )


def best_first(watch, binaries):
    """The binaries a watch runs, the one to install first."""
    return sorted(
        (b for b in binaries if fits(watch, b)), key=lambda b: _rank(watch, b)
    )
