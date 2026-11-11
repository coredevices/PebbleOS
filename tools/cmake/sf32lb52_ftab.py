#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""SF32LB52 flash table (FTAB), which tells the boot ROM what image to boot.

The table goes in the ptable partition. Only the entries the ROM needs to boot
from flash are filled: the table itself and the image it boots, in the
secondary bootloader slot. See the SiFli SDK documentation on the SF32LB52x
bootloader for the layout.
"""

import argparse
import os
import struct
import sys

from intelhex import IntelHex

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "devicetree"))

import fdt

MAGIC = 0x53454346
# The factory calibration data follows the table
TABLE_SIZE = 0x8000

# Flash table entries, then image headers for every entry but the first two
ENTRIES = 16
ENTRY = struct.Struct("<IIII")
ENTRY_TABLE = 0
ENTRY_BOOT = 3
IMAGE_FIRST_ENTRY = 2
IMAGES_OFFSET = 4096
IMAGE_SIZE = 512
IMAGE_HEADER = struct.Struct("<IHH")
IMAGE_FLAG_AUTO = 2
IMAGE_BLOCK_SIZE = 512
RUNNING_IMAGES = 4
SIZE = IMAGES_OFFSET + (ENTRIES - IMAGE_FIRST_ENTRY) * IMAGE_SIZE + RUNNING_IMAGES * 4


def generate(table_base, image_base, image_size):
    """The FTAB at ``table_base`` booting the image at ``image_base``."""
    data = bytearray(SIZE)
    struct.pack_into("<I", data, 0, MAGIC)
    ENTRY.pack_into(data, 4 + ENTRY_TABLE * ENTRY.size, table_base, TABLE_SIZE, 0, 0)
    ENTRY.pack_into(
        data, 4 + ENTRY_BOOT * ENTRY.size, image_base, image_size, image_base, 0
    )

    for i in range(ENTRIES - IMAGE_FIRST_ENTRY):
        offset = IMAGES_OFFSET + i * IMAGE_SIZE
        if i + IMAGE_FIRST_ENTRY == ENTRY_BOOT:
            header = (image_size, IMAGE_BLOCK_SIZE, IMAGE_FLAG_AUTO)
        else:
            header = (0xFFFFFFFF, 0, 0)
        IMAGE_HEADER.pack_into(data, offset, *header)

    running = [0xFFFFFFFF] * RUNNING_IMAGES
    boot = ENTRY_BOOT - IMAGE_FIRST_ENTRY
    running[boot] = table_base + IMAGES_OFFSET + boot * IMAGE_SIZE
    struct.pack_into(f"<{RUNNING_IMAGES}I", data, SIZE - RUNNING_IMAGES * 4, *running)
    return bytes(data)


def partition(tree, label):
    """``(offset, size)`` of the flash partition ``label``."""
    for node in tree.nodes():
        if node.parent is None or "fixed-partitions" not in node.parent.compatibles:
            continue
        name = node.string("label") if node.has("label") else node.basename
        if name != label or not node.status_okay:
            continue
        cells = node.u32s("reg")
        if len(cells) != 2:
            raise fdt.DtError(f"{node.path}: 'reg' must be one 32-bit offset and size")
        return cells[0], cells[1]
    raise fdt.DtError(f"no '{label}' flash partition")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dtb", required=True, help="the board's compiled devicetree")
    parser.add_argument("--flash-base", type=lambda s: int(s, 0), required=True)
    parser.add_argument(
        "--image-offset",
        type=lambda s: int(s, 0),
        required=True,
        help="offset of the image the ROM boots, from the flash base",
    )
    parser.add_argument("--image-size", type=lambda s: int(s, 0), required=True)
    parser.add_argument("--output", required=True, help="the Intel HEX to write")
    args = parser.parse_args()

    try:
        offset, size = partition(fdt.load(args.dtb), "ptable")
    except fdt.DtError as error:
        sys.exit(f"ftab: {error}")
    if size < TABLE_SIZE:
        sys.exit(f"ftab: the ptable partition is smaller than {TABLE_SIZE:#x} bytes")

    table_base = args.flash_base + offset
    data = generate(table_base, args.flash_base + args.image_offset, args.image_size)
    image = IntelHex()
    image.frombytes(data, offset=table_base)
    image.write_hex_file(args.output)


if __name__ == "__main__":
    main()
