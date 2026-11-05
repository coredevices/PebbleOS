#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Evaluate the Kconfig of an SDK platform, without a board.

Prints the platform's CONFIG_PLATFORM_* symbols as a CMake list of
defines, after checking them against the SDK's platform table.
"""

import argparse
import os
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(REPO_ROOT, "tools"))

import kconfiglib
from pebble_sdk_platform import pebble_platforms


def platform_symbols(platform):
    os.environ["srctree"] = REPO_ROOT
    kconf = kconfiglib.Kconfig(os.path.join(REPO_ROOT, "Kconfig.platform"), warn=False)

    choice = kconf.syms.get(f"PLATFORM_{platform.upper()}")
    if choice is None or choice.choice is None:
        sys.exit(f"Unknown platform '{platform}'")
    choice.set_value(2)

    symbols = {}
    for sym in kconf.unique_defined_syms:
        if not sym.name.startswith("PLATFORM_"):
            continue
        if sym.type == kconfiglib.BOOL:
            if sym.tri_value == 2:
                symbols[sym.name] = "1"
        elif sym.str_value:
            symbols[sym.name] = sym.str_value

    return symbols


def check_sdk(platform, symbols):
    defines = set(pebble_platforms[platform]["DEFINES"])
    expected = {
        "PBL_COLOR" if "PLATFORM_COLOR" in symbols else "PBL_BW",
        "PBL_ROUND" if "PLATFORM_ROUND" in symbols else "PBL_RECT",
        f"PBL_DISPLAY_WIDTH={symbols['PLATFORM_DISPLAY_WIDTH']}",
        f"PBL_DISPLAY_HEIGHT={symbols['PLATFORM_DISPLAY_HEIGHT']}",
    }
    missing = sorted(expected - defines)
    if missing:
        sys.exit(
            f"Kconfig.platform and tools/pebble_sdk_platform.py disagree on {platform}: "
            f"the SDK lacks {', '.join(missing)}"
        )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("platform")
    args = parser.parse_args()

    symbols = platform_symbols(args.platform)
    check_sdk(args.platform, symbols)
    print(
        ";".join(f"CONFIG_{name}={value}" for name, value in sorted(symbols.items())),
        end="",
    )


if __name__ == "__main__":
    main()
