#!/usr/bin/env python3
# Copyright (c) 2025 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import argparse
import datetime
import struct
import zlib

from intelhex import IntelHex


MAGIC = 0x96F3B83D


def _insert_header_hex(fin, fout, offset):
    # Load the hex file
    ih = IntelHex(fin)

    # Get the binary content from the hex file
    content = ih.tobinarray().tobytes()
    base_addr = ih.minaddr() - offset

    # Prepare the header
    crc = zlib.crc32(content)
    now = datetime.datetime.now(datetime.timezone.utc)
    timestamp = int(now.timestamp())

    fwdesc = struct.pack("<LLQLLL", MAGIC, 28, timestamp, offset, len(content), crc)

    # Create new IntelHex object for output
    out_ih = IntelHex()

    # Add header at the beginning
    for i, byte in enumerate(fwdesc):
        out_ih[base_addr + i] = byte

    # Add padding
    pad_start = len(fwdesc)
    pad_end = offset
    for i in range(pad_start, pad_end):
        out_ih[base_addr + i] = 0xFF

    # Add original content at offset
    for i, byte in enumerate(content):
        out_ih[base_addr + offset + i] = byte

    # Write output hex file
    out_ih.write_hex_file(fout)


def _insert_header_bin(fin, fout, offset):
    # Read the input binary file
    with open(fin, "rb") as f:
        content = f.read()

    # Prepare the header
    crc = zlib.crc32(content)
    now = datetime.datetime.now(datetime.timezone.utc)
    timestamp = int(now.timestamp())

    fwdesc = struct.pack("<LLQLLL", MAGIC, 28, timestamp, offset, len(content), crc)

    # Write output binary file
    with open(fout, "wb") as f:
        f.write(fwdesc)
        f.write(b"\xFF" * (offset - len(fwdesc)))
        f.write(content)


def insert_header_hex(task):
    _insert_header_hex(
        task.inputs[0].abspath(),
        task.outputs[0].abspath(),
        task.generator.bld.env.FIRMWARE_OFFSET,
    )


def insert_header_bin(task):
    _insert_header_bin(
        task.inputs[0].abspath(),
        task.outputs[0].abspath(),
        task.generator.bld.env.FIRMWARE_OFFSET,
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Generate a firmware with pblboot header"
    )
    parser.add_argument("input", help="Input firmware hex file")
    parser.add_argument("output", help="Output firmware hex file")
    parser.add_argument(
        "--offset", type=int, default=512, help="Offset to apply to input file"
    )
    args = parser.parse_args()

    if args.input.endswith('.bin'):
        _insert_header_bin(args.input, args.output, args.offset)
    else:
        _insert_header_hex(args.input, args.output, args.offset)
