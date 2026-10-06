# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import struct
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import Mock, patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "sdk" / "tools"))

import inject_metadata
import stm32_crc


class TestInjectMetadata(unittest.TestCase):
    def inject(
        self,
        load_size,
        virtual_size,
        version=(0x10, 1),
        max_binary_size=0x20000,
        symbols="000000b0 T main\n",
    ):
        image = bytearray(load_size)
        image[:8] = b"PBLAPP\0\0"
        struct.pack_into("<BB", image, 8, *version)
        image[0x82:0x84] = b"\xaa\x55"
        note = struct.pack("<III4s20s", 4, 20, 3, b"GNU\0", bytes(range(20)))
        image[0x84 : 0x84 + len(note)] = note

        def popen(command, **kwargs):
            if command[0] == "arm-none-eabi-nm":
                output = symbols + "000000b4 D pbl_table_addr\n"
            elif isinstance(command, str):
                output = (
                    f"  [ 5] .bss NOBITS {load_size:08x} 000000 "
                    f"{virtual_size - load_size:06x} 00 WA 0 0 4\n"
                )
            elif command[1] == "-r":
                output = (
                    "Relocation section '.rel.data' contains 1 entry:\n"
                    "Offset Info Type\n000000b8 00000002 R_ARM_ABS32\n"
                    "000000bc 00000026 R_ARM_TARGET1\n000000c0 0000000a R_ARM_THM_CALL\n\n"
                )
            else:
                output = ""
            return Mock(communicate=Mock(return_value=(output.encode(), b"")))

        with tempfile.TemporaryDirectory() as directory:
            binary = Path(directory) / "app.bin"
            binary.write_bytes(image)
            with patch.object(inject_metadata, "Popen", side_effect=popen):
                inject_metadata.inject_metadata(
                    str(binary), "app.elf", None, 0, max_binary_size=max_binary_size
                )
            result = binary.read_bytes()

        self.assertEqual(result[0x84 : 0x84 + len(note)], note)
        self.assertEqual(result[load_size:], struct.pack("<II", 0xB8, 0xBC))
        self.assertEqual(
            struct.unpack_from("<I", result, 0x14)[0],
            stm32_crc.crc32(result[0x82:load_size]),
        )
        return result

    def test_wide_load_size_boundary(self):
        for load_size in (0xFFFF, 0x10000):
            with self.subTest(load_size=load_size):
                result = self.inject(load_size, 0x18000)
                self.assertEqual(
                    struct.unpack_from("<H", result, 0xE)[0], load_size & 0xFFFF
                )
                self.assertEqual(result[0x82], load_size >> 16)

    def test_wide_virtual_size_boundary_with_small_image(self):
        for virtual_size in (0xFFFF, 0x10000):
            with self.subTest(virtual_size=virtual_size):
                result = self.inject(0x1000, virtual_size)
                self.assertEqual(
                    struct.unpack_from("<H", result, 0x80)[0], virtual_size & 0xFFFF
                )
                self.assertEqual(result[0x82], 0)
                self.assertEqual(result[0x83], virtual_size >> 16)

    def test_old_header_preserves_padding_at_16_bit_limit(self):
        result = self.inject(0xFFFF, 0xFFFF, version=(0x10, 0))
        self.assertEqual(result[0x82:0x84], b"\xaa\x55")
        self.assertEqual(struct.unpack_from("<H", result, 0xE)[0], 0xFFFF)
        self.assertEqual(struct.unpack_from("<H", result, 0x80)[0], 0xFFFF)

    def test_old_header_rejects_wide_load_size(self):
        with self.assertRaisesRegex(RuntimeError, "load_size in 16 bits"):
            self.inject(0x10000, 0x10000, version=(0x10, 0))

    def test_old_header_rejects_wide_virtual_size(self):
        with self.assertRaisesRegex(RuntimeError, "virtual_size in 16 bits"):
            self.inject(0x1000, 0x10000, version=(0x10, 0))

    def test_relocations_must_fit_platform_limit(self):
        with self.assertRaisesRegex(RuntimeError, "App image size"):
            self.inject(0x10000, 0x10000, max_binary_size=0x10000)

    def test_entry_point_is_pbl_process_entry(self):
        result = self.inject(
            0x1000, 0x1000, symbols="000000b0 T main\n000000c4 T pbl_process_entry\n"
        )
        self.assertEqual(struct.unpack_from("<I", result, 0x10)[0], 0xC4)

    def test_entry_point_falls_back_to_main(self):
        result = self.inject(
            0x1000, 0x1000, symbols="000000b0 T main\n         U pbl_process_entry\n"
        )
        self.assertEqual(struct.unpack_from("<I", result, 0x10)[0], 0xB0)

    def test_missing_main_is_rejected(self):
        with self.assertRaisesRegex(RuntimeError, "Missing app entry point"):
            self.inject(0x1000, 0x1000, symbols="000000c4 T pbl_process_entry\n")


if __name__ == "__main__":
    unittest.main()
