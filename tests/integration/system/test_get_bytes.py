# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Reading coredumps and flash from a watch."""

import struct

import pytest
from harness.errors import LinkClosed, WatchTimeout

CORE_DUMP_MAGIC = 0xF00DCAFE


def test_flash_range(dut):
    from libpebble2.services.getbytes import GetBytesService

    service = GetBytesService(dut.protocol)
    data = service.get_flash_region(0, 8192)
    assert len(data) == 8192
    assert data == service.get_flash_region(0, 8192)


@pytest.mark.device_types("qemu", "hardware")
def test_coredump_after_panic(dut):
    from libpebble2.services.getbytes import GetBytesService

    try:
        dut.prompt("sys panic", timeout=2)
    except (WatchTimeout, LinkClosed):
        pass
    dut.disconnect()
    dut.connect()
    dut.wait_ready()

    data = GetBytesService(dut.protocol).get_coredump(False)
    assert struct.unpack_from("<I", data)[0] == CORE_DUMP_MAGIC
