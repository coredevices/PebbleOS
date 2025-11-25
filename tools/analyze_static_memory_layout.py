#!/usr/bin/env python
# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0


import argparse
import re
import sh

class SectionInfo(object):
    def __init__(self, name, begin, end):
        self.name = name
        self.begin = begin
        self.end = end

def read_section_info(elf_file):
    section_headers_output = sh.arm_none_eabi_readelf('-S', elf_file)

    line_pattern = re.compile(r"""\[\s*\d+\]\s+    # Number
                                   (\S+)\s+        # Name
                                   \S+\s+          # Type
                                   ([0-9a-f]+)\s+  # Virtual Address
                                   [0-9a-f]+\s+    # Load Address
                                   ([0-9a-f]+)\s+  # Size
                                   """, flags=re.VERBOSE)

    sections = []

    for line in section_headers_output:
        match = line_pattern.search(line)

        if match is None:
            continue

        name = match.group(1)
        addr = int(match.group(2), 16)
        size = int(match.group(3), 16)

        if (addr < 0x20000000 or addr > 0x20030000) and (addr < 0x10000000 or addr > 0x10010000):
            # We only care about stuff that goes into ram or CCM
            continue

        if (name == 'DISCARD'):
            continue

        sections.append(SectionInfo(name, addr, addr + size))

    return sections

def find_symbol(word):
    return re.compile(r"""\b({0})$""".format(word)).search

def read_layout_symbols(elf_file):
    symbols_output = sh.arm_none_eabi_objdump('-t', elf_file)

    desired_symbols = [ 'system_stm32f4xx.c',
                        '_heap_start',
                        '_heap_end' ]

    symbols = {}

    line_pattern = re.compile(r"""^(\S+)""")

    for line in symbols_output:
        for s in desired_symbols:
            if find_symbol(s)(line):
                match = line_pattern.search(line)
                symbols[s] = int(match.group(1), 16)

    return symbols

def analyze_layout(elf_file):
    sections = read_section_info(elf_file)
    symbols = read_layout_symbols(elf_file)

    ram_start_address = 0x20000000
    ram_end_address = 0x20020000

    if 'system_stm32f4xx.c' in symbols:
        # We have a snowy! Adjust the RAM size to be larger
        ram_end_address = 0x20030000

    # Add a dummy section that spans where the kernel heap is
    sections.append(SectionInfo('<KERNEL HEAP>', symbols['_heap_start'], symbols['_heap_end']))

    sections = sorted(sections, key=lambda x: x.begin)

    last_end = 0
    padding_total = 0

    for s in sections:
        if last_end != 0 and last_end != s.begin:
            # There's a gap between sections! Use the last section's name to identify what it is.

            padding_end = s.begin

            # Don't insert a padding after CCM and before RAM
            if padding_end != ram_start_address:
                if last_name == '.worker_bss' or last_name == '.workerlib_paddin':
                    name = 'WORKER CODE + HEAP'
                else:
                    name = 'PADDING'
                    padding_total += padding_end - last_end

                print "0x%x - 0x%x %6u bytes <%s>" % \
                        (last_end, padding_end, padding_end - last_end, name)

        print "0x%x - 0x%x %6u bytes %s" % (s.begin, s.end, s.end - s.begin, s.name)

        last_end = s.end
        last_name = s.name

    # The app code + heap region doesn't have a section for it, it just takes up everything at the
    # end of the address space.
    print "0x%x - 0x%x %6u bytes <APP CODE + HEAP>" % (last_end, ram_end_address, ram_end_address - last_end)

    print 'Total padding: %u bytes' % padding_total

if (__name__ == '__main__'):
    parser = argparse.ArgumentParser()
    parser.add_argument('elf_file')
    args = parser.parse_args()

    sections = analyze_layout(args.elf_file)

