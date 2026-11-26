# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""A USB mass storage device (``CONFIG_USB_MSC``, ``subsys/usb/msc.c``),
from the host's side.

:class:`BulkOnly` speaks Bulk-Only Transport and SCSI to it through pyusb,
which needs the interface to itself: the host's own driver is detached where
libusb can do it, as on Linux. :class:`HostDisk` goes through the host's own
storage stack instead, on macOS: the disk the device becomes, mounted with
``diskutil``.
"""

import os
import plistlib
import struct
import subprocess
import time

from harness.errors import HarnessError
from harness.helpers.usb import is_stall

MSC_CLASS = 0x08
MSC_SUBCLASS_SCSI = 0x06
MSC_PROTOCOL_BOT = 0x50

REQ_GET_MAX_LUN = 0xFE
REQ_RESET = 0xFF

CBW_SIGNATURE = 0x43425355
CSW_SIGNATURE = 0x53425355

CSW_PASSED = 0
CSW_FAILED = 1
CSW_PHASE_ERROR = 2

TEST_UNIT_READY = 0x00
REQUEST_SENSE = 0x03
INQUIRY = 0x12
MODE_SENSE_6 = 0x1A
START_STOP_UNIT = 0x1B
PREVENT_ALLOW_REMOVAL = 0x1E
READ_FORMAT_CAPACITIES = 0x23
READ_CAPACITY_10 = 0x25
READ_10 = 0x28
WRITE_10 = 0x2A
SYNCHRONIZE_CACHE_10 = 0x35
MODE_SENSE_10 = 0x5A

SENSE_NOT_READY = 0x02
SENSE_ILLEGAL_REQUEST = 0x05
SENSE_DATA_PROTECT = 0x07

ASC_INVALID_OPCODE = 0x20
ASC_LBA_OUT_OF_RANGE = 0x21
ASC_INVALID_FIELD_IN_CDB = 0x24
ASC_WRITE_PROTECTED = 0x27
ASC_MEDIUM_NOT_PRESENT = 0x3A
ASC_MEDIUM_REMOVAL = 0x53

GET_STATUS = 0x00
CLEAR_FEATURE = 0x01
ENDPOINT_HALT = 0


def interface(dev):
    """The mass storage interface of ``dev``, or None."""
    import usb.util

    cfg = dev.get_active_configuration()
    return usb.util.find_descriptor(
        cfg,
        bInterfaceClass=MSC_CLASS,
        bInterfaceSubClass=MSC_SUBCLASS_SCSI,
        bInterfaceProtocol=MSC_PROTOCOL_BOT,
    )


class Csw:
    def __init__(self, data, residue, status):
        self.data = data
        self.residue = residue
        self.status = status

    def __repr__(self):
        return (
            f"Csw(status={self.status}, residue={self.residue}, {len(self.data)} bytes)"
        )


class BulkOnly:
    """The device's mass storage interface, driven with pyusb."""

    def __init__(self, dev, timeout_ms=5000):
        import usb.core
        import usb.util

        self.dev = dev
        self.timeout_ms = timeout_ms
        self.intf = interface(dev)
        if self.intf is None:
            raise HarnessError("the device has no mass storage interface")
        self.num = self.intf.bInterfaceNumber
        self._detached = False
        try:
            if dev.is_kernel_driver_active(self.num):
                dev.detach_kernel_driver(self.num)
                self._detached = True
        except (NotImplementedError, usb.core.USBError):
            pass
        usb.util.claim_interface(dev, self.num)
        eps = list(self.intf)
        self.ep_out = next(e for e in eps if not e.bEndpointAddress & 0x80)
        self.ep_in = next(e for e in eps if e.bEndpointAddress & 0x80)
        self.mps = self.ep_in.wMaxPacketSize
        self.tag = 0

    def release(self):
        import usb.core
        import usb.util

        usb.util.dispose_resources(self.dev)
        if self._detached:
            try:
                self.dev.attach_kernel_driver(self.num)
            except usb.core.USBError:
                pass

    # --- class requests and endpoint halts ----------------------------------

    def get_max_lun(self):
        return self.dev.ctrl_transfer(
            0xA1, REQ_GET_MAX_LUN, 0, self.num, 1, self.timeout_ms
        )[0]

    def mass_storage_reset(self):
        self.dev.ctrl_transfer(0x21, REQ_RESET, 0, self.num, None, self.timeout_ms)

    def halted(self, ep):
        data = self.dev.ctrl_transfer(0x82, GET_STATUS, 0, ep, 2, self.timeout_ms)
        return bool(data[0] & 1)

    def clear_halt(self, ep):
        self.dev.ctrl_transfer(
            0x02, CLEAR_FEATURE, ENDPOINT_HALT, ep, None, self.timeout_ms
        )

    def reset_recovery(self):
        """Reset the interface and clear both halts, as after a phase error."""
        self.mass_storage_reset()
        self.clear_halt(self.ep_in.bEndpointAddress)
        self.clear_halt(self.ep_out.bEndpointAddress)

    # --- transport ----------------------------------------------------------

    def cbw(self, cb, length, data_in, lun=0, tag=None, signature=CBW_SIGNATURE):
        self.tag += 1
        flags = 0x80 if data_in else 0x00
        return struct.pack(
            "<IIIBBB16s",
            signature,
            self.tag if tag is None else tag,
            length,
            flags,
            lun,
            len(cb),
            bytes(cb),
        )

    def send_cbw(self, raw):
        self.dev.write(self.ep_out.bEndpointAddress, raw, self.timeout_ms)

    def read_csw(self):
        """The CSW, clearing the halt of the IN endpoint the device may have
        set first, as the host does after a stall."""
        import usb.core

        try:
            raw = bytes(self.dev.read(self.ep_in.bEndpointAddress, 13, self.timeout_ms))
        except usb.core.USBError as e:
            if not is_stall(e):
                raise
            self.dev.clear_halt(self.ep_in.bEndpointAddress)
            raw = bytes(self.dev.read(self.ep_in.bEndpointAddress, 13, self.timeout_ms))
        if len(raw) != 13:
            raise HarnessError(f"CSW of {len(raw)} bytes")
        sig, tag, residue, status = struct.unpack("<IIIB", raw)
        if sig != CSW_SIGNATURE or tag != self.tag:
            raise HarnessError(f"bad CSW {raw.hex()} for tag {self.tag}")
        return residue, status

    def command(self, cb, length=0, data_in=True, data=b""):
        """Run a command, moving up to ``length`` bytes: its data, the residue
        and the status of its CSW."""
        import usb.core

        self.send_cbw(self.cbw(cb, length, data_in))
        got = b""
        if length and data_in:
            try:
                got = bytes(
                    self.dev.read(self.ep_in.bEndpointAddress, length, self.timeout_ms)
                )
            except usb.core.USBError as e:
                if not is_stall(e):
                    raise
                self.dev.clear_halt(self.ep_in.bEndpointAddress)
        elif length:
            try:
                self.dev.write(self.ep_out.bEndpointAddress, data, self.timeout_ms)
            except usb.core.USBError as e:
                if not is_stall(e):
                    raise
                self.dev.clear_halt(self.ep_out.bEndpointAddress)
        residue, status = self.read_csw()
        return Csw(got, residue, status)

    # --- SCSI ---------------------------------------------------------------

    def test_unit_ready(self):
        return self.command(bytes(6), 0).status

    def request_sense(self):
        """Sense key, additional sense code and qualifier."""
        csw = self.command(bytes([REQUEST_SENSE, 0, 0, 0, 18, 0]), 18)
        if csw.status != CSW_PASSED or len(csw.data) < 14:
            raise HarnessError(f"REQUEST SENSE failed: {csw}")
        return csw.data[2] & 0x0F, csw.data[12], csw.data[13]

    def inquiry(self, alloc=36, length=None):
        return self.command(
            bytes([INQUIRY, 0, 0]) + alloc.to_bytes(2, "big") + bytes(1),
            alloc if length is None else length,
        )

    def read_capacity(self):
        """Sector count and size."""
        csw = self.command(bytes([READ_CAPACITY_10]) + bytes(9), 8)
        if csw.status != CSW_PASSED:
            raise HarnessError(f"READ CAPACITY failed: {csw}")
        last, size = struct.unpack(">II", csw.data)
        return last + 1, size

    def mode_sense_6(self, page=0x3F, alloc=192):
        return self.command(bytes([MODE_SENSE_6, 0, page, 0, alloc, 0]), alloc)

    def mode_sense_10(self, page=0x3F, alloc=192):
        return self.command(
            bytes([MODE_SENSE_10, 0, page, 0, 0, 0, 0])
            + alloc.to_bytes(2, "big")
            + bytes(1),
            alloc,
        )

    def read(self, lba, count, size=512, length=None):
        cb = bytes([READ_10, 0]) + lba.to_bytes(4, "big") + bytes(1)
        cb += count.to_bytes(2, "big") + bytes(1)
        return self.command(cb, count * size if length is None else length)

    def write(self, lba, data, size=512, length=None):
        count = len(data) // size
        cb = bytes([WRITE_10, 0]) + lba.to_bytes(4, "big") + bytes(1)
        cb += count.to_bytes(2, "big") + bytes(1)
        return self.command(
            cb, len(data) if length is None else length, data_in=False, data=data
        )


# --- through the host's storage stack (macOS) -------------------------------


def _plist(args):
    out = subprocess.run(args, capture_output=True, check=True).stdout
    return plistlib.loads(out)


def location_id(location):
    """The IOKit locationID of a device at ``<bus>-<port>[.<port>...]``."""
    bus, ports = location.split("-")
    value = int(bus) << 24
    for i, port in enumerate(ports.split(".")):
        value |= int(port) << (20 - 4 * i)
    return value


def _entry(entry, loc_id):
    """The first entry under ``entry`` at a locationID."""
    if entry.get("locationID") == loc_id:
        return entry
    for child in entry.get("IORegistryEntryChildren", ()):
        found = _entry(child, loc_id)
        if found is not None:
            return found
    return None


def _whole_disks(entry):
    if entry.get("IOObjectClass") == "IOMedia" and entry.get("Whole"):
        yield entry["BSD Name"], entry.get("Size", 0)
    for child in entry.get("IORegistryEntryChildren", ()):
        yield from _whole_disks(child)


def screen_locked():
    """Whether the console's screen is locked, which keeps macOS from
    mounting new disks."""
    root = _plist(["ioreg", "-a", "-n", "Root", "-d1"])
    root = root[0] if isinstance(root, list) else root
    return any(
        user.get("CGSSessionScreenIsLocked") for user in root.get("IOConsoleUsers", ())
    )


class HostDisk:
    """The whole disk macOS makes of the device at a USB location."""

    def __init__(self, location):
        self.location = location
        self.loc_id = location_id(location)

    def find(self, timeout=15.0):
        """The BSD name of the whole disk and its size in bytes, waiting for
        it to appear."""
        deadline = time.monotonic() + timeout
        while True:
            for top in _plist(["ioreg", "-a", "-r", "-l", "-c", "IOUSBHostDevice"]):
                dev = _entry(top, self.loc_id)
                if dev is None:
                    continue
                for name, size in _whole_disks(dev):
                    if size > 0:
                        return name, size
            if time.monotonic() >= deadline:
                return None, 0
            time.sleep(0.25)

    def gone(self, name, timeout=10.0):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if not os.path.exists(f"/dev/{name}"):
                return True
            time.sleep(0.25)
        return False

    @staticmethod
    def diskutil(*args):
        return subprocess.run(
            ["diskutil", *args], capture_output=True, text=True, check=False
        )

    def mount_point(self, name):
        info = _plist(["diskutil", "info", "-plist", name])
        return info.get("MountPoint") or None

    def mount(self, name, timeout=15.0):
        """Mount the disk's volume, returning where, or None if the host
        refuses to."""
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            point = self.mount_point(name)
            if point:
                return point
            self.diskutil("mount", name)
            time.sleep(0.5)
        return None

    def unmount(self, name):
        result = self.diskutil("unmount", name)
        if result.returncode != 0:
            raise HarnessError(f"diskutil unmount {name}: {result.stderr.strip()}")

    def eject(self, name):
        result = self.diskutil("eject", name)
        if result.returncode != 0:
            raise HarnessError(f"diskutil eject {name}: {result.stderr.strip()}")
