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


def _coredump_threads(data):
    threads = []
    offset = 92  # image header
    while True:
        key, size = struct.unpack_from("<II", data, offset)
        offset += 8
        if key == 0xFFFFFFFF:
            return threads
        if key == 2:
            name, _, running, *regs = struct.unpack_from("<16sIB17I", data, offset)
            threads.append((name.split(b"\0")[0].decode(), bool(running), regs))
        offset += size


@pytest.mark.device_types("qemu", "hardware")
def test_coredump_has_fault_registers(dut):
    from libpebble2.exceptions import GetBytesError
    from libpebble2.services.getbytes import GetBytesService

    # A recent dump that was not read would be kept instead of the new one
    try:
        GetBytesService(dut.protocol).get_coredump(True)
    except GetBytesError:
        pass

    try:
        dut.prompt("sys fault", timeout=2)
    except (WatchTimeout, LinkClosed):
        pass
    dut.disconnect()
    dut.connect()
    dut.wait_ready()

    data = GetBytesService(dut.protocol).get_coredump(False)
    running = [regs for _, is_running, regs in _coredump_threads(data) if is_running]
    assert len(running) == 1
    regs = running[0]

    # r0-r12 as set before the fault, so neither the fault handler's nor unknown
    assert regs[:13] == [0x10101010 + i * 0x01010101 for i in range(13)]
    # sp, back to where it was before stacking the frame, in RAM and aligned
    assert regs[13] & 0x3 == 0
    assert regs[13] >> 24 == 0x20
    # pc, thumb code
    assert regs[15] not in (0, 0xA5A5A5A5)
