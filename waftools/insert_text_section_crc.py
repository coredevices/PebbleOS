#!/usr/bin/env python
# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0


from waflib import Logs
from zlib import crc32
from shutil import copyfile
import struct
from elftools.elf.elffile import ELFFile

TEXT_SECTION_NAME = ".text"
TEXT_CRC32_SECTION_NAME = ".text_crc32"

def wafrule(task):
    in_file = task.inputs[0].abspath()
    out_file = task.outputs[0].abspath()

    text_data = get_text_section_data_from_file(in_file)
    if not text_data:
        error = 'Unable to get {} section from {}'.format(TEXT_SECTION_NAME, in_file)
        Logs.pprint('RED', error)
        return error

    crc = crc32(text_data) & 0xFFFFFFFF

    offset = get_text_crc32_section_offset_from_file(in_file)
    if not offset:
        error = 'Unable to get {} section from {}'.format(TEXT_CRC32_SECTION_NAME, in_file)
        Logs.pprint('RED', error)
        return error

    copyfile(in_file, out_file)

    with open(out_file, 'rb+') as file:
        file.seek(offset)
        file.write(struct.pack('<I', crc))


def get_text_section_data_from_file(filename):
    with open(filename, 'rb') as file:
        section = ELFFile(file).get_section_by_name(TEXT_SECTION_NAME)
        return section.data() if section is not None else None

def get_text_crc32_section_offset_from_file(filename):
    with open(filename, 'rb') as file:
        section = ELFFile(file).get_section_by_name(TEXT_CRC32_SECTION_NAME)
        return section['sh_offset'] if section is not None else None

