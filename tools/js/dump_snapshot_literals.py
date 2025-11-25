#! /usr/bin/env python
# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0

import sys
import struct

ROCKY_HEADER_SIZE = 8
ROCKY_HEADER_FMT = "4sB3x"

JERRY_HEADER_SIZE = 16
JERRY_HEADER_FMT = "<4I"

LIT_TABLE_HEADER_SIZE = 8
LIT_TABLE_HEADER_FMT = "<2I"

LIT_STRING_HEADER_SIZE = 2
LIT_STRING_HEADER_FMT = "<H"


def _get_aligned_literal_length(length):
    LITERAL_ALIGNMENT = (1 << 2)
    return ((length) + (LITERAL_ALIGNMENT - 1)) & ~(LITERAL_ALIGNMENT - 1)


def main(snapshot_path):
    with open(snapshot_path, 'rb') as fin:
        # TODO: This should be generalized to a prefix string, not a Rocky header
        rocky_header = struct.unpack(ROCKY_HEADER_FMT, fin.read(ROCKY_HEADER_SIZE))
        print "Rocky Header"
        print "  Signature: {}".format(rocky_header[0])
        print "  Version:   0x{:02x}".format(rocky_header[1])
        print

        jerry_header = struct.unpack(JERRY_HEADER_FMT, fin.read(JERRY_HEADER_SIZE))
        print "Jerry Header"
        print "  Version: {}".format(jerry_header[0])
        print "  lit_table_offset: {}".format(jerry_header[1])
        print "  lit_table_size:   {}".format(jerry_header[2])
        print "  is_run_global:    {}".format(jerry_header[3])
        print

        fin.seek(jerry_header[1] + ROCKY_HEADER_SIZE, 0)  # position to beginning of literal table

        lit_header = struct.unpack(LIT_TABLE_HEADER_FMT, fin.read(LIT_TABLE_HEADER_SIZE))
        lit_string_count = lit_header[0]
        lit_number_count = lit_header[1]
        print "Literal Header"
        print "  String Count: {}".format(lit_string_count)
        print "  Number Count: {}".format(lit_number_count)
        print

        # Now dump the literal tables
        offset = 0
        for i in xrange(lit_string_count):
            length = struct.unpack(LIT_STRING_HEADER_FMT, fin.read(LIT_STRING_HEADER_SIZE))[0]
            aligned_length = _get_aligned_literal_length(2 + length)
            s = struct.unpack("{}s".format(length), fin.read(length))[0]
            print "0x{:08x}: String: '{}'".format(offset, s)

            move = aligned_length - length - 2  # - 2 since the header was already read
            if move > 0:
                fin.seek(move, 1)
            offset += aligned_length

        remaining_length = jerry_header[2] - offset - LIT_TABLE_HEADER_SIZE
        number_length = remaining_length / float(lit_number_count)
        if not number_length.is_integer():
            print "Invalid offset for start of number literals"
            return 1
        number_length = int(number_length)

        for i in xrange(lit_number_count):
            length = number_length
            aligned_length = _get_aligned_literal_length(length)
            n = struct.unpack("d" if length == 8 else "f", fin.read(length))[0]
            print "0x{:08x}: Number: {}".format(offset, n)

            move = aligned_length - length
            if move > 0:
                fin.seek(move, 1)
            offset += aligned_length

        print "Numbers are {}-bit".format(number_length * 8)
        return 0


if __name__ == "__main__":
    rc = main(sys.argv[1])
    sys.exit(rc)
