# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import sys
from pathlib import Path

import pytest
from intelhex import IntelHex

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "cmake"))

import images


def hexfile(*segments):
    image = IntelHex()
    for address, data in segments:
        image.frombytes(data, offset=address)
    return image


def test_keeps_the_flash_and_drops_the_rest():
    image = hexfile((0x0, b"\x01" * 16), (0x20000000, b"\x02" * 16))
    placed = images.place(image, 0x0, 0x100000, 0x0, 0x8000)
    assert placed.segments() == [(0x0, 0x10)]


def test_flash_outside_the_partition_fails():
    image = hexfile((0x12010000, b"\x01" * 16), (0x12020000, b"\x02" * 16))
    with pytest.raises(ValueError, match="outside the partition"):
        images.place(image, 0x12000000, 0x2000000, 0x10000, 0x10000)


def test_nothing_in_the_flash_fails():
    image = hexfile((0x20000000, b"\x02" * 16))
    with pytest.raises(ValueError, match="nothing in the flash"):
        images.place(image, 0x0, 0x100000, 0x0, 0x8000)
