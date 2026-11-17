#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""The SDK platform of a build, out of Kconfig.

With a platform name, evaluates Kconfig.platform for that platform, without
a board, and prints its CONFIG_PLATFORM_* symbols as a CMake list of
defines. With --dotconfig, reads the platform a board configured instead.
--json writes the platform's description, the SDK's platform.json.
"""

import argparse
import json
import os
import re
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(REPO_ROOT, "tools"))

import kconfiglib
from pebble_sdk_platform import (
    PlatformError,
    descriptor_from_kconfig,
    kconfig_platform,
    kconfig_symbols,
)

KCONFIG = os.path.join(REPO_ROOT, "pebble", "Kconfig.platform")


def dotconfig_symbols(path):
    symbols = {}
    with open(path) as f:
        for line in f:
            match = re.match(r"CONFIG_(PLATFORM_\w+)=(.*)", line.strip())
            if match:
                value = match.group(2)
                symbols[match.group(1)] = "1" if value == "y" else value.strip('"')
    return symbols


def _write_if_changed(path, content):
    try:
        with open(path) as f:
            if f.read() == content:
                return
    except OSError:
        pass
    with open(path, "w") as f:
        f.write(content)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("platform", nargs="?", help="a platform of Kconfig.platform")
    parser.add_argument("--dotconfig", help="the .config of a configured build")
    parser.add_argument("--json", help="write the platform's description here")
    args = parser.parse_args()

    if bool(args.platform) == bool(args.dotconfig):
        parser.error("give a platform or --dotconfig")

    kconf = None
    try:
        if args.dotconfig:
            symbols = dotconfig_symbols(args.dotconfig)
        else:
            kconf = kconfig_platform(KCONFIG, f"PLATFORM_{args.platform.upper()}")
            symbols = kconfig_symbols(kconf)
        descriptor = descriptor_from_kconfig(symbols)
    except (PlatformError, KeyError) as e:
        sys.exit(f"Kconfig does not describe an SDK platform: {e}")

    if args.json:
        _write_if_changed(args.json, json.dumps(descriptor, indent=2) + "\n")
    if kconf is None:
        return

    def define(name, value):
        # Strings are quoted, as in autoconf.h.
        if kconf.syms[name].type == kconfiglib.STRING:
            value = f'"{value}"'
        return f"CONFIG_{name}={value}"

    print(";".join(define(n, v) for n, v in sorted(symbols.items())), end="")


if __name__ == "__main__":
    main()
