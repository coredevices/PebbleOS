# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0

import os
import sys
import unittest

# Allow us to run even if not at the `tools` directory.
root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))
sys.path.insert(0, root_dir)

from app_header import PebbleAppHeader
from uuid import UUID

V1_APP_HEADER = b"\x50\x42\x4C\x41\x50\x50\x00\x00\x08\x01\x03\x01\x03\x00" \
                b"\xD8\x1A\x34\x0A\x00\x00\xC6\xF1\x2E\x8B\x57\x46\x47\x20" \
                b"\x44\x65\x6D\x6F\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
                b"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
                b"\x57\x61\x74\x63\x68\x66\x61\x63\x65\x20\x47\x65\x6E\x65" \
                b"\x72\x61\x74\x6F\x72\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
                b"\x00\x00\x00\x00\x01\x00\x00\x00\x00\x13\x00\x00\x01\x00" \
                b"\x00\x00\xD8\x1A\x00\x00\x22\x00\x00\x00\xC9\x5A\x9A\x75" \
                b"\x6E\x8C\x01\x59\xD3\xE0\x2F\x94\x1F\xA6\xB9\x75"

V2_APP_HEADER = b"\x50\x42\x4C\x41\x50\x50\x00\x00\x10\x00\x05\x00\x01\x00" \
                b"\xA1\x0C\x08\x05\x00\x00\x06\x3E\x92\x94\x57\x46\x47\x20" \
                b"\x44\x65\x6D\x6F\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
                b"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
                b"\x57\x61\x74\x63\x68\x66\x61\x63\x65\x2D\x47\x65\x6E\x65" \
                b"\x72\x61\x74\x6F\x72\x2E\x64\x65\x00\x00\x00\x00\x00\x00" \
                b"\x00\x00\x00\x00\x01\x00\x00\x00\x94\x00\x00\x00\x01\x00" \
                b"\x00\x00\x04\x00\x00\x00\x13\x37\x13\x37\xD7\xAA\x1F\xEB" \
                b"\xB8\x78\x99\x91\x62\x89\xCD\x1E\x07\x60\x88\xCF\xCE\x4A" \
                b"\xD0\x52\xC8\x0D"


class TestAppHeader(unittest.TestCase):
    def test_deserialize_v1_header(self):
        h = PebbleAppHeader(V1_APP_HEADER)
        self.assertEqual(h.sentinel, b"PBLAPP\x00\x00")
        self.assertEqual(h.struct_version_major,
                          PebbleAppHeader.V1_STRUCT_VERSION[0])
        self.assertEqual(h.struct_version_minor,
                          PebbleAppHeader.V1_STRUCT_VERSION[1])
        self.assertEqual(h.sdk_version_major, 0x03)
        self.assertEqual(h.sdk_version_minor, 0x01)
        self.assertEqual(h.app_version_major, 0x03)
        self.assertEqual(h.app_version_minor, 0x00)
        self.assertEqual(h.app_size, 6872)
        self.assertEqual(h.offset, 2612)
        self.assertEqual(h.crc, 0x8b2ef1c6)
        self.assertEqual(h.app_name, b"WFG Demo")
        self.assertEqual(h.company_name, b"Watchface Generator")
        self.assertEqual(h.icon_resource_id, 1)
        self.assertEqual(h.symbol_table_addr, 4864)
        self.assertEqual(h.flags, 1)
        self.assertEqual(h.relocation_list_index, 6872)
        self.assertEqual(h.num_relocation_entries, 34)
        self.assertEqual(h.uuid, UUID('c95a9a75-6e8c-0159-d3e0-2f941fa6b975'))

    def test_deserialize_v2_header(self):
        h = PebbleAppHeader(V2_APP_HEADER)
        self.assertEqual(h.sentinel, b"PBLAPP\x00\x00")
        self.assertEqual(h.struct_version_major,
                          PebbleAppHeader.V2_STRUCT_VERSION[0])
        self.assertEqual(h.struct_version_minor,
                          PebbleAppHeader.V2_STRUCT_VERSION[1])
        self.assertEqual(h.sdk_version_major, 0x05)
        self.assertEqual(h.sdk_version_minor, 0x00)
        self.assertEqual(h.app_version_major, 0x01)
        self.assertEqual(h.app_version_minor, 0x00)
        self.assertEqual(h.app_size, 3233)
        self.assertEqual(h.offset, 1288)
        self.assertEqual(h.crc, 0x94923e06)
        self.assertEqual(h.app_name, b"WFG Demo")
        self.assertEqual(h.company_name, b"Watchface-Generator.de")
        self.assertEqual(h.icon_resource_id, 1)
        self.assertEqual(h.symbol_table_addr, 148)
        self.assertEqual(h.flags, 1)
        self.assertEqual(h.num_relocation_entries, 4)
        self.assertEqual(h.uuid, UUID('13371337-d7aa-1feb-b878-99916289cd1e'))
        self.assertEqual(h.resource_crc, 0xcf886007)
        self.assertEqual(h.resource_timestamp, 1389382350)
        self.assertEqual(h.virtual_size, 3528)

    def test_deserialize_serialize_v1(self):
        h = PebbleAppHeader(V1_APP_HEADER)
        bytes = h.serialize()
        self.assertEqual(bytes, V1_APP_HEADER)

    def test_deserialize_serialize_v2(self):
        h = PebbleAppHeader(V2_APP_HEADER)
        bytes = h.serialize()
        self.assertEqual(bytes, V2_APP_HEADER)


if __name__ == '__main__':
    unittest.main()
