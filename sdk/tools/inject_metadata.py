#!/usr/bin/env python
# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0


import os
import os.path
import sys
from shutil import copy2
from struct import pack, unpack
from subprocess import PIPE, Popen

import stm32_crc
from pbpack import ResourcePack

# Pebble App Metadata Struct
# These are offsets of the PebbleProcessInfo struct in fw/app_management/pebble_process_info.h
HEADER_ADDR = 0x0  # 8 bytes
STRUCT_VERSION_ADDR = 0x8  # 2 bytes
SDK_VERSION_ADDR = 0xA  # 2 bytes
APP_VERSION_ADDR = 0xC  # 2 bytes
LOAD_SIZE_ADDR = 0xE  # 2 bytes
OFFSET_ADDR = 0x10  # 4 bytes
CRC_ADDR = 0x14  # 4 bytes
NAME_ADDR = 0x18  # 32 bytes
COMPANY_ADDR = 0x38  # 32 bytes
ICON_RES_ID_ADDR = 0x58  # 4 bytes
JUMP_TABLE_ADDR = 0x5C  # 4 bytes
FLAGS_ADDR = 0x60  # 4 bytes
NUM_RELOC_ENTRIES_ADDR = 0x64  # 4 bytes
UUID_ADDR = 0x68  # 16 bytes
RESOURCE_CRC_ADDR = 0x78  # 4 bytes
RESOURCE_TIMESTAMP_ADDR = 0x7C  # 4 bytes
VIRTUAL_SIZE_ADDR = 0x80  # 2 bytes
LOAD_SIZE_HI_ADDR = 0x82  # 1 byte
VIRTUAL_SIZE_HI_ADDR = 0x83  # 1 byte

# The app CRC starts at the end of the 0x10.0x00 header, so it takes in the size high bytes
CRC_START_ADDR = 0x82

# Pebble App Flags
# These are PebbleAppFlags from fw/app_management/pebble_process_info.h
PROCESS_INFO_STANDARD_APP = 0
PROCESS_INFO_WATCH_FACE = 1 << 0
PROCESS_INFO_VISIBILITY_HIDDEN = 1 << 1
PROCESS_INFO_VISIBILITY_SHOWN_ON_COMMUNICATION = 1 << 2
PROCESS_INFO_ALLOW_JS = 1 << 3
PROCESS_INFO_HAS_WORKER = 1 << 4

# Max app size, including the struct and reloc table. Fallback for callers that pass no limit; the
# build passes the platform's MAX_APP_BINARY_SIZE from tools/pebble_sdk_platform.py.
# Note that even if the app is smaller than this, it still may be too big, as it needs to share this
# space with applib/ which changes in size from release to release.
MAX_APP_BINARY_SIZE = 0x10000

# From struct version 0x10.0x01, PebbleProcessInfo carries load_size and virtual_size in 24 bits:
# the low 16 in the original fields and the high 8 in load_size_hi and virtual_size_hi. Older
# headers have padding there and stop at 16 bits.
FIRST_WIDE_SIZE_STRUCT_VERSION = (0x10, 0x01)
MAX_PROCESS_INFO_SIZE_FIELD = 0xFFFFFF
MAX_PROCESS_INFO_SIZE_FIELD_16 = 0xFFFF

# This number is a rough estimate, but should not be less than the available space.
# Currently, app_state uses up a small part of the app space.
# See also APP_RAM in stm32f2xx_flash_fw.ld and APP in pebble_app.ld.
MAX_APP_MEMORY_SIZE = 24 * 1024

# This number is a rough estimate, but should not be less than the available space.
# Currently, worker_state uses up a small part of the worker space.
# See also WORKER_RAM in stm32f2xx_flash_fw.ld
MAX_WORKER_MEMORY_SIZE = 10 * 1024

ENTRY_PT_SYMBOL = "main"
JUMP_TABLE_ADDR_SYMBOL = "pbl_table_addr"
ABS_RELOC_TYPES = ("R_ARM_ABS32", "R_ARM_TARGET1")
DEBUG = False


class InvalidBinaryError(Exception):
    pass


def inject_metadata(
    target_binary,
    target_elf,
    resources_file,
    timestamp,
    allow_js=False,
    has_worker=False,
    max_binary_size=None,
):
    if max_binary_size is None:
        max_binary_size = MAX_APP_BINARY_SIZE

    if target_binary[-4:] != ".bin":
        raise RuntimeError(
            f"Invalid filename <{target_binary}>! The filename should end in .bin"
        )

    def get_nm_output(elf_file):
        nm_process = Popen(["arm-none-eabi-nm", elf_file], stdout=PIPE)
        # Popen.communicate returns a tuple of (stdout, stderr)
        nm_output = nm_process.communicate()[0].decode("utf8")

        if not nm_output:
            raise InvalidBinaryError()

        nm_output = [line.split() for line in nm_output.splitlines()]
        return nm_output

    def get_symbol_addr(nm_output, symbol):
        # nm output looks like the following...
        #
        #          U _ITM_registerTMCloneTable
        # 00000084 t jump_to_pbl_function
        #          U _Jv_RegisterClasses
        # 0000009c T main
        # 00000130 T memset
        #
        # We don't care about the lines that only have two columns, they're not functions.

        for sym in nm_output:
            if symbol == sym[-1] and len(sym) == 3:
                return int(sym[0], 16)

        raise RuntimeError(
            f"Could not locate symbol <{symbol}> in binary! Failed to inject app metadata"
        )

    def get_virtual_size(elf_file):
        """returns the virtual size (static memory usage, .text + .data + .bss) in bytes"""

        readelf_bss_process = Popen(
            f"arm-none-eabi-readelf -S '{elf_file}'", shell=True, stdout=PIPE
        )
        readelf_bss_output = readelf_bss_process.communicate()[0].decode("utf8")

        # readelf -S output looks like the following...
        #
        # [Nr] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al
        # [ 0]                   NULL            00000000 000000 000000 00      0   0  0
        # [ 1] .header           PROGBITS        00000000 008000 000082 00   A  0   0  1
        # [ 2] .text             PROGBITS        00000084 008084 0006be 00  AX  0   0  4
        # [ 3] .rel.text         REL             00000000 00b66c 0004d0 08     23   2  4
        # [ 4] .data             PROGBITS        00000744 008744 000004 00  WA  0   0  4
        # [ 5] .bss              NOBITS          00000748 008748 000054 00  WA  0   0  4

        last_section_end_addr = 0

        # Find the .bss section and calculate the size based on the end of the .bss section
        for line in readelf_bss_output.splitlines():
            if len(line) < 10:
                continue

            # Carve off the first column, since it sometimes has a space in it which screws up the
            # split.
            if "]" not in line:
                continue
            line = line[line.index("]") + 1 :]

            columns = line.split()
            if len(columns) < 6:
                continue

            if (
                columns[0] == ".bss"
                or columns[0] == ".data"
                and last_section_end_addr == 0
            ):
                addr = int(columns[2], 16)
                size = int(columns[4], 16)
                last_section_end_addr = addr + size

        if last_section_end_addr != 0:
            return last_section_end_addr

        sys.stderr.writeline(
            "Failed to parse ELF sections while calculating the virtual size\n"
        )
        sys.stderr.write(readelf_bss_output)
        raise RuntimeError(
            "Failed to parse ELF sections while calculating the virtual size"
        )

    def get_relocate_entries(elf_file):
        """returns a list of all the locations requiring an offset"""
        entries = []

        # Non-PIC libraries also embed absolute pointers in .text literal pools.
        readelf_relocs_process = Popen(
            ["arm-none-eabi-readelf", "-r", elf_file], stdout=PIPE
        )
        readelf_relocs_output = readelf_relocs_process.communicate()[0].decode("utf8")
        lines = readelf_relocs_output.splitlines()

        reading_section = False
        for line in lines:
            if line.startswith("Relocation section '"):
                reading_section = line.startswith(
                    ("Relocation section '.rel.text", "Relocation section '.rel.data")
                )
                continue
            columns = line.split()
            # PC-relative relocations are already resolved by the linker. R_ARM_TARGET1 (used by
            # .init_array and .fini_array) is linked as absolute on arm-none-eabi.
            if reading_section and len(columns) >= 3 and columns[2] in ABS_RELOC_TYPES:
                entries.append(int(columns[0], 16))

        # get any Global Offset Table (.got) entries
        readelf_relocs_process = Popen(
            ["arm-none-eabi-readelf", "--sections", elf_file], stdout=PIPE
        )
        readelf_relocs_output = readelf_relocs_process.communicate()[0].decode("utf8")
        lines = readelf_relocs_output.splitlines()
        for line in lines:
            # We shouldn't need to do anything with the Procedure Linkage Table since we don't
            # actually export functions
            if ".got" in line and ".got.plt" not in line:
                words = line.split(" ")
                while "" in words:
                    words.remove("")
                section_label_idx = words.index(".got")
                addr = int(words[section_label_idx + 2], 16)
                length = int(words[section_label_idx + 4], 16)
                entries.extend(range(addr, addr + length, 4))
                break

        return entries

    nm_output = get_nm_output(target_elf)

    try:
        app_entry_address = get_symbol_addr(nm_output, ENTRY_PT_SYMBOL)
    except RuntimeError as e:
        raise RuntimeError(
            "Missing app entry point! Must be `int main(void) { ... }` "
        ) from e
    jump_table_address = get_symbol_addr(nm_output, JUMP_TABLE_ADDR_SYMBOL)

    reloc_entries = get_relocate_entries(target_elf)

    statinfo = os.stat(target_binary)
    app_load_size = statinfo.st_size

    if resources_file is not None:
        with open(resources_file, "rb") as f:
            pbpack = ResourcePack.deserialize(f, is_system=False)
            resource_crc = pbpack.get_content_crc()
    else:
        resource_crc = 0

    if DEBUG:
        copy2(target_binary, target_binary + ".orig")

    with open(target_binary, "r+b") as f:
        total_app_image_size = app_load_size + (len(reloc_entries) * 4)
        if total_app_image_size > max_binary_size:
            raise RuntimeError(
                f"App image size is {total_app_image_size:d} (app {app_load_size:d} relocation table {len(reloc_entries) * 4:d}). Must be smaller "
                f"than {max_binary_size:d} bytes"
            )

        def read_value_at_offset(offset, format_str, size):
            f.seek(offset)
            return unpack(format_str, f.read(size))

        def write_value_at_offset(offset, format_str, value):
            f.seek(offset)
            f.write(pack(format_str, value))

        # The firmware only reads the high bytes from 0x10.0x01, so a header compiled from an older
        # pebble_process_info.h keeps the 16-bit limit rather than failing its CRC on the watch
        struct_version = read_value_at_offset(STRUCT_VERSION_ADDR, "<BB", 2)
        wide_sizes = struct_version >= FIRST_WIDE_SIZE_STRUCT_VERSION
        max_size_field = (
            MAX_PROCESS_INFO_SIZE_FIELD if wide_sizes else MAX_PROCESS_INFO_SIZE_FIELD_16
        )
        size_bits = 24 if wide_sizes else 16

        # Checked here so the pack() calls below cannot raise a bare struct.error.
        if app_load_size > max_size_field:
            raise RuntimeError(
                f"App load size is {app_load_size:d} bytes. The loaded image must be {max_size_field:d} bytes or smaller, "
                f"because PebbleProcessInfo {struct_version[0]:#04x}.{struct_version[1]:#04x} carries load_size in {size_bits} bits. "
                "The relocation table is stored past the loaded image and does not count towards this."
            )

        app_virtual_size = get_virtual_size(target_elf)

        # Same ceiling as load_size, on the .text + .data + .bss total this time.
        if app_virtual_size > max_size_field:
            raise RuntimeError(
                f"App virtual size is {app_virtual_size:d} bytes (.text + .data + .bss). Must be {max_size_field:d} bytes or "
                f"smaller, because PebbleProcessInfo {struct_version[0]:#04x}.{struct_version[1]:#04x} carries virtual_size in {size_bits} bits."
            )

        # The high bytes sit inside the CRC range, so they go in before the CRC is taken
        if wide_sizes:
            write_value_at_offset(LOAD_SIZE_HI_ADDR, "<B", app_load_size >> 16)
            write_value_at_offset(VIRTUAL_SIZE_HI_ADDR, "<B", app_virtual_size >> 16)

        f.seek(0)
        app_bin = f.read()
        app_crc = stm32_crc.crc32(app_bin[CRC_START_ADDR:])

        [app_flags] = read_value_at_offset(FLAGS_ADDR, "<L", 4)

        if allow_js:
            app_flags = app_flags | PROCESS_INFO_ALLOW_JS

        if has_worker:
            app_flags = app_flags | PROCESS_INFO_HAS_WORKER

        struct_changes = {
            "load_size": app_load_size,
            "entry_point": f"0x{app_entry_address:08x}",
            "symbol_table": f"0x{jump_table_address:08x}",
            "flags": app_flags,
            "crc": f"0x{app_crc:08x}",
            "num_reloc_entries": f"0x{len(reloc_entries):08x}",
            "resource_crc": f"0x{resource_crc:08x}",
            "timestamp": timestamp,
            "virtual_size": app_virtual_size,
        }

        write_value_at_offset(LOAD_SIZE_ADDR, "<H", app_load_size & 0xFFFF)
        write_value_at_offset(OFFSET_ADDR, "<L", app_entry_address)
        write_value_at_offset(CRC_ADDR, "<L", app_crc)

        write_value_at_offset(RESOURCE_CRC_ADDR, "<L", resource_crc)
        write_value_at_offset(RESOURCE_TIMESTAMP_ADDR, "<L", timestamp)

        write_value_at_offset(JUMP_TABLE_ADDR, "<L", jump_table_address)

        write_value_at_offset(FLAGS_ADDR, "<L", app_flags)

        write_value_at_offset(NUM_RELOC_ENTRIES_ADDR, "<L", len(reloc_entries))

        write_value_at_offset(VIRTUAL_SIZE_ADDR, "<H", app_virtual_size & 0xFFFF)

        # Write the reloc_entries past the end of the binary. This expands the size of the binary,
        # but this new stuff won't actually be loaded into ram.
        f.seek(app_load_size)
        f.writelines(pack("<L", entry) for entry in reloc_entries)

        f.flush()

    return struct_changes
