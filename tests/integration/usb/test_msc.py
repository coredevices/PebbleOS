# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""USB mass storage, on the RAM disk of samples/subsys/usb/msc: the transport
and the SCSI commands through pyusb, where the host lets it take the
interface, and the disk through the host's own storage stack on macOS."""

import fcntl
import os
import random
import struct
import time

import pytest
from harness.helpers import msc

pytestmark = [
    pytest.mark.device_types("hardware"),
    pytest.mark.requires_config("CONFIG_USB_MSC"),
]

SECTOR = 512
README = b"Hello from PebbleOS!\n"
# Far below full speed's bulk bandwidth, as for the test function.
MIN_THROUGHPUT_KBPS = 200
HOST_FILE_BYTES = 128 * 1024


def _sectors(build):
    return build.config["CONFIG_DISK_RAM_SIZE"] * 1024 // SECTOR


def _read_only(build):
    return bool(build.config.get("CONFIG_SAMPLE_READ_ONLY"))


def _payload(length, seed):
    return random.Random(seed).randbytes(length)


def _expect_sense(dev, key, asc):
    assert dev.request_sense()[:2] == (key, asc)


# --- the transport and SCSI, through pyusb ----------------------------------


def test_interface(usb_msc):
    import usb.util

    intf = usb_msc.intf
    assert intf.bNumEndpoints == 2
    mps = 512 if usb_msc.dev.speed == usb.util.SPEED_HIGH else 64
    for ep in (usb_msc.ep_in, usb_msc.ep_out):
        assert usb.util.endpoint_type(ep.bmAttributes) == usb.util.ENDPOINT_TYPE_BULK
        assert ep.wMaxPacketSize == mps
    assert usb_msc.get_max_lun() == 0


def test_inquiry(usb_msc, build):
    csw = usb_msc.inquiry()
    assert (csw.status, csw.residue, len(csw.data)) == (msc.CSW_PASSED, 0, 36)
    data = csw.data
    assert data[0] == 0x00
    assert data[1] & 0x80, "removable"
    assert data[8:16].decode().rstrip() == build.config["CONFIG_USB_MSC_VENDOR"]
    assert data[16:32].decode().rstrip() == build.config["CONFIG_USB_MSC_PRODUCT"]


def test_inquiry_longer_host_length(usb_msc):
    """The host expects more than the device has: what it has, then a stall
    and the difference as the residue."""
    csw = usb_msc.inquiry(alloc=36, length=100)
    assert (csw.status, csw.residue, len(csw.data)) == (msc.CSW_PASSED, 64, 36)
    assert usb_msc.test_unit_ready() == msc.CSW_PASSED


def test_capacity(usb_msc, build):
    assert usb_msc.test_unit_ready() == msc.CSW_PASSED
    assert usb_msc.read_capacity() == (_sectors(build), SECTOR)

    csw = usb_msc.command(
        bytes([msc.READ_FORMAT_CAPACITIES]) + bytes(6) + b"\x00\xfc\x00", 252
    )
    assert csw.status == msc.CSW_PASSED
    assert len(csw.data) == 12
    assert struct.unpack(">I", csw.data[4:8])[0] == _sectors(build)
    assert csw.data[8] & 0x03 == 0x02, "formatted media"
    assert int.from_bytes(csw.data[9:12], "big") == SECTOR


@pytest.mark.parametrize("ten", [False, True])
def test_mode_sense(usb_msc, build, ten):
    sense = usb_msc.mode_sense_10 if ten else usb_msc.mode_sense_6
    hdr = 8 if ten else 4
    for page in (0x3F, 0x08):
        csw = sense(page)
        assert csw.status == msc.CSW_PASSED
        data = csw.data
        wp = data[3 if ten else 2] & 0x80
        assert bool(wp) == _read_only(build)
        assert data[hdr] & 0x3F == 0x08, "caching page"
        assert data[hdr + 2] & 0x04 == 0, "write cache off"

    csw = sense(0x1C)
    assert csw.status == msc.CSW_FAILED
    _expect_sense(usb_msc, msc.SENSE_ILLEGAL_REQUEST, msc.ASC_INVALID_FIELD_IN_CDB)


def test_file_system(usb_msc):
    boot = usb_msc.read(0, 1).data
    assert boot[510:512] == b"\x55\xaa"
    assert boot[43:54] == b"PEBBLEOS   "
    reserved, fats, root, fat = (
        struct.unpack("<H", boot[14:16])[0],
        boot[16],
        struct.unpack("<H", boot[17:19])[0],
        struct.unpack("<H", boot[22:24])[0],
    )
    data = reserved + fats * fat + root * 32 // SECTOR
    assert usb_msc.read(data, 1).data.startswith(README)


@pytest.mark.parametrize(
    "lba_from_end, count", [(1, 1), (16, 7), (32, 8), (64, 9), (128, 64)]
)
def test_write_read(usb_msc, build, lba_from_end, count):
    """Writes and reads back, in and across the two transfer buffers, then
    puts the sectors back."""
    if _read_only(build):
        pytest.skip("read-only disk")
    lba = _sectors(build) - lba_from_end
    old = usb_msc.read(lba, count)
    assert old.status == msc.CSW_PASSED
    payload = _payload(count * SECTOR, lba)
    try:
        csw = usb_msc.write(lba, payload)
        assert (csw.status, csw.residue) == (msc.CSW_PASSED, 0)
        csw = usb_msc.read(lba, count)
        assert (csw.status, csw.residue) == (msc.CSW_PASSED, 0)
        assert csw.data == payload
        sync = usb_msc.command(bytes([msc.SYNCHRONIZE_CACHE_10]) + bytes(9))
        assert sync.status == msc.CSW_PASSED
    finally:
        assert usb_msc.write(lba, old.data).status == msc.CSW_PASSED


def test_write_protected(usb_msc, build):
    if not _read_only(build):
        pytest.skip("writable disk")
    lba = _sectors(build) - 1
    old = usb_msc.read(lba, 1).data
    csw = usb_msc.write(lba, _payload(SECTOR, 1))
    assert (csw.status, csw.residue) == (msc.CSW_FAILED, SECTOR)
    _expect_sense(usb_msc, msc.SENSE_DATA_PROTECT, msc.ASC_WRITE_PROTECTED)
    assert usb_msc.read(lba, 1).data == old


def test_out_of_range(usb_msc, build):
    sectors = _sectors(build)
    for lba, count in ((sectors, 1), (sectors - 1, 2), (0xFFFFFFFF, 1)):
        csw = usb_msc.read(lba, count)
        assert (csw.status, csw.residue, csw.data) == (
            msc.CSW_FAILED,
            count * SECTOR,
            b"",
        )
        _expect_sense(usb_msc, msc.SENSE_ILLEGAL_REQUEST, msc.ASC_LBA_OUT_OF_RANGE)
    if not _read_only(build):
        csw = usb_msc.write(sectors, _payload(SECTOR, 2))
        assert (csw.status, csw.residue) == (msc.CSW_FAILED, SECTOR)
        _expect_sense(usb_msc, msc.SENSE_ILLEGAL_REQUEST, msc.ASC_LBA_OUT_OF_RANGE)
    assert usb_msc.test_unit_ready() == msc.CSW_PASSED


@pytest.mark.parametrize("length", [0, 8])
def test_unknown_command(usb_msc, length):
    csw = usb_msc.command(bytes([0xC0]) + bytes(9), length)
    assert (csw.status, csw.residue) == (msc.CSW_FAILED, length)
    _expect_sense(usb_msc, msc.SENSE_ILLEGAL_REQUEST, msc.ASC_INVALID_OPCODE)


def test_zero_length_read(usb_msc):
    csw = usb_msc.read(0, 0)
    assert (csw.status, csw.residue) == (msc.CSW_PASSED, 0)


@pytest.mark.parametrize(
    "length, data_in",
    [
        # Less than the command moves, or none at all
        (SECTOR, True),
        (0, True),
        # The other direction
        (2 * SECTOR, False),
    ],
)
def test_phase_error(usb_msc, length, data_in):
    """The host and the device disagree on the data: a phase error, after
    which the host resets the interface."""
    cb = bytes([msc.READ_10, 0, 0, 0, 0, 0, 0, 0, 2, 0])
    csw = usb_msc.command(cb, length, data_in, data=bytes(length))
    assert (csw.status, csw.residue, csw.data) == (msc.CSW_PHASE_ERROR, length, b"")
    usb_msc.reset_recovery()
    assert usb_msc.test_unit_ready() == msc.CSW_PASSED


@pytest.mark.parametrize(
    "raw_len, signature", [(31, 0x12345678), (30, msc.CBW_SIGNATURE)]
)
def test_invalid_cbw(usb_msc, raw_len, signature):
    """Both endpoints stay halted, whatever the host clears, until it resets
    the interface."""
    import usb.core

    raw = usb_msc.cbw(bytes(6), 0, True, signature=signature)[:raw_len]
    usb_msc.send_cbw(raw)
    ep_in = usb_msc.ep_in.bEndpointAddress
    ep_out = usb_msc.ep_out.bEndpointAddress
    with pytest.raises(usb.core.USBError) as e:
        usb_msc.dev.read(ep_in, 13, usb_msc.timeout_ms)
    assert msc.is_stall(e.value)
    for ep in (ep_in, ep_out):
        assert usb_msc.halted(ep)
        usb_msc.clear_halt(ep)
        assert usb_msc.halted(ep)

    usb_msc.reset_recovery()
    assert not usb_msc.halted(ep_in)
    assert not usb_msc.halted(ep_out)
    assert usb_msc.test_unit_ready() == msc.CSW_PASSED


def test_reset_mid_read(usb_msc):
    """A reset ends a command the host gave up on: nothing of it comes after."""
    cb = bytes([msc.READ_10, 0, 0, 0, 0, 1, 0, 0, 64, 0])
    usb_msc.send_cbw(usb_msc.cbw(cb, 64 * SECTOR, True))
    usb_msc.dev.read(usb_msc.ep_in.bEndpointAddress, SECTOR, usb_msc.timeout_ms)
    usb_msc.reset_recovery()
    csw = usb_msc.read(0, 1)
    assert (csw.status, csw.residue) == (msc.CSW_PASSED, 0)
    assert csw.data[510:512] == b"\x55\xaa"


def test_eject_and_load(usb_msc):
    assert (
        usb_msc.command(bytes([msc.PREVENT_ALLOW_REMOVAL, 0, 0, 0, 1, 0])).status == 0
    )
    csw = usb_msc.command(bytes([msc.START_STOP_UNIT, 0, 0, 0, 0x02, 0]))
    assert csw.status == msc.CSW_FAILED
    assert usb_msc.request_sense()[:2] == (
        msc.SENSE_ILLEGAL_REQUEST,
        msc.ASC_MEDIUM_REMOVAL,
    )

    assert (
        usb_msc.command(bytes([msc.PREVENT_ALLOW_REMOVAL, 0, 0, 0, 0, 0])).status == 0
    )
    assert usb_msc.command(bytes([msc.START_STOP_UNIT, 0, 0, 0, 0x02, 0])).status == 0
    assert usb_msc.test_unit_ready() == msc.CSW_FAILED
    _expect_sense(usb_msc, msc.SENSE_NOT_READY, msc.ASC_MEDIUM_NOT_PRESENT)
    assert usb_msc.read(0, 1).status == msc.CSW_FAILED
    assert usb_msc.inquiry().status == msc.CSW_PASSED

    assert usb_msc.command(bytes([msc.START_STOP_UNIT, 0, 0, 0, 0x03, 0])).status == 0
    assert usb_msc.test_unit_ready() == msc.CSW_PASSED


def test_throughput(usb_msc, build, record_property):
    sectors = _sectors(build)
    chunk = 64
    rounds = max(1, 1024 * 1024 // (sectors * SECTOR))
    start = time.monotonic()
    image = b""
    for _ in range(rounds):
        image = b""
        for lba in range(0, sectors, chunk):
            csw = usb_msc.read(lba, min(chunk, sectors - lba))
            assert csw.status == msc.CSW_PASSED
            image += csw.data
    elapsed = time.monotonic() - start
    kbps = rounds * len(image) / 1024 / elapsed
    record_property("msc_read_kbps", round(kbps, 1))
    print(f"read: {rounds * len(image)} bytes in {elapsed:.3f} s, {kbps:.1f} KiB/s")
    assert kbps >= MIN_THROUGHPUT_KBPS

    if _read_only(build):
        return
    start = time.monotonic()
    for _ in range(rounds):
        for lba in range(0, sectors, chunk):
            part = image[lba * SECTOR : (lba + chunk) * SECTOR]
            assert usb_msc.write(lba, part).status == msc.CSW_PASSED
    elapsed = time.monotonic() - start
    kbps = rounds * len(image) / 1024 / elapsed
    record_property("msc_write_kbps", round(kbps, 1))
    print(f"write: {rounds * len(image)} bytes in {elapsed:.3f} s, {kbps:.1f} KiB/s")
    assert kbps >= MIN_THROUGHPUT_KBPS


# --- the disk, through the host's storage stack -----------------------------


def _full_sync(f):
    f.flush()
    os.fsync(f.fileno())
    fcntl.fcntl(f.fileno(), fcntl.F_FULLFSYNC)


def test_host_disk(host_disk, build, record_property):
    name, size = host_disk.find()
    assert name, f"no disk for the device at {host_disk.location}"
    assert size == _sectors(build) * SECTOR

    point = host_disk.mount(name)
    if point is None:
        pytest.skip("the host does not mount the disk, as while the screen is locked")
    with open(os.path.join(point, "README.TXT"), "rb") as f:
        assert f.read() == README

    path = os.path.join(point, "TEST.BIN")
    if _read_only(build):
        with pytest.raises(OSError), open(path, "wb"):
            pass
        host_disk.unmount(name)
        return

    payload = _payload(HOST_FILE_BYTES, 42)
    start = time.monotonic()
    with open(path, "wb") as f:
        f.write(payload)
        _full_sync(f)
    elapsed = time.monotonic() - start
    kbps = len(payload) / 1024 / elapsed
    record_property("host_write_kbps", round(kbps, 1))
    print(f"host write: {len(payload)} bytes in {elapsed:.3f} s, {kbps:.1f} KiB/s")

    # Remounted, the file comes from the device, not the host's cache.
    host_disk.unmount(name)
    point = host_disk.mount(name)
    assert point
    start = time.monotonic()
    with open(path, "rb") as f:
        data = f.read()
    elapsed = time.monotonic() - start
    kbps = len(data) / 1024 / elapsed
    record_property("host_read_kbps", round(kbps, 1))
    print(f"host read: {len(data)} bytes in {elapsed:.3f} s, {kbps:.1f} KiB/s")
    assert data == payload

    os.remove(path)
    host_disk.unmount(name)


def test_host_eject(host_disk, usb_target):
    """Ejected, the disk is gone until the device connects again."""
    import usb.core

    name, _ = host_disk.find()
    assert name
    host_disk.eject(name)
    assert host_disk.gone(name)

    try:
        usb_target.find().reset()
    except usb.core.USBError:
        pass
    name, _ = host_disk.find(timeout=20.0)
    assert name, "the disk did not come back after a bus reset"
