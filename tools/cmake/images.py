#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Place an image given as Intel HEX in a partition of the flash.

What the file has in the flash has to fall in the partition. What it has
elsewhere, such as RAM a programmer would load, is left out.
"""

import argparse
import os
import sys

from intelhex import IntelHex

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "devicetree"))

import dtmap
import fdt


def place(image, flash_base, flash_size, offset, size):
    """The part of ``image`` in the flash, which has to fall in the partition."""
    start, end = flash_base + offset, flash_base + offset + size
    placed = IntelHex()
    for lo, hi in image.segments():
        lo, hi = max(lo, flash_base), min(hi, flash_base + flash_size)
        if lo >= hi:
            continue
        if lo < start or hi > end:
            raise ValueError(
                f"{lo:#x}-{hi:#x} is outside the partition, {start:#x}-{end:#x}"
            )
        placed.merge(image[lo:hi])
    if not placed.segments():
        raise ValueError("nothing in the flash")
    return placed


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dtb", required=True, help="the board's compiled devicetree")
    parser.add_argument("--flash-base", type=lambda s: int(s, 0), required=True)
    parser.add_argument("--flash-size", type=lambda s: int(s, 0), required=True)
    parser.add_argument("--partition", required=True, help="label of the partition")
    parser.add_argument("--input", required=True, help="the Intel HEX of the image")
    parser.add_argument("--output", required=True, help="the Intel HEX to write")
    args = parser.parse_args()

    try:
        parts = dtmap.partitions(fdt.load(args.dtb))
    except fdt.DtError as error:
        sys.exit(f"images: {error}")
    if args.partition not in parts:
        sys.exit(f"images: no '{args.partition}' flash partition")
    offset, size = parts[args.partition]
    try:
        placed = place(
            IntelHex(args.input), args.flash_base, args.flash_size, offset, size
        )
    except ValueError as error:
        sys.exit(f"images: {args.input}: {error}")
    placed.write_hex_file(args.output)


if __name__ == "__main__":
    main()
