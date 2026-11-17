#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Puts what the `pebble` tool emulates a platform with into its SDK
directory. See docs/sdk/building.md."""

import argparse
import bz2
import os
import shutil


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--platform", required=True, help="SDK platform name")
    parser.add_argument("--elf", required=True, help="The firmware")
    parser.add_argument("--spi-flash", required=True, help="The SPI flash image")
    parser.add_argument("--layouts", required=True, help="The timeline layouts")
    parser.add_argument("--output", required=True, help="The platform's qemu directory")
    args = parser.parse_args()

    shutil.rmtree(args.output, ignore_errors=True)
    os.makedirs(args.output)

    # The emulator is given the ELF as its kernel, under the name the flat
    # image used to have: it takes the vector table from it.
    shutil.copy2(args.elf, os.path.join(args.output, "qemu_micro_flash.bin"))
    shutil.copy2(args.elf, os.path.join(args.output, f"{args.platform}_sdk_debug.elf"))
    shutil.copy2(args.layouts, os.path.join(args.output, "layouts.json"))
    with (
        open(args.spi_flash, "rb") as src,
        bz2.open(os.path.join(args.output, "qemu_spi_flash.bin.bz2"), "wb") as dst,
    ):
        shutil.copyfileobj(src, dst)


if __name__ == "__main__":
    main()
