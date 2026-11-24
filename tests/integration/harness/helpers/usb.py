# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""The device's USB port, from the host's side, through pyusb (libusb).

:class:`UsbTarget` finds the build's device on the host's bus by its vendor
and product IDs. :class:`TestFunction` drives the USB test function
(``CONFIG_USB_TEST``, ``subsys/usb/test.c``) on it.
"""

import errno
import shlex
import struct
import subprocess
import time

from harness.errors import HarnessError

# bmRequestType of the test function's requests: vendor, to its interface.
VENDOR_OUT = 0x41
VENDOR_IN = 0xC1

REQ_WRITE = 0x01
REQ_READ = 0x02
REQ_STALL = 0x03
REQ_RESET = 0x04
REQ_STATS = 0x05

STATS = ("loop_bytes", "source_bytes", "sink_bytes", "sink_errors", "int_packets")

INTERFACE = 0
EP_LOOP_OUT = 0x01
EP_LOOP_IN = 0x82
EP_SINK = 0x03
EP_SOURCE = 0x84
EP_INT = 0x85

VENDOR_CLASS = 0xFF
#: Largest control transfer the function takes.
CONTROL_LEN = 1024
INT_MPS = 16

GET_STATUS = 0x00
CLEAR_FEATURE = 0x01
SET_FEATURE = 0x03
ENDPOINT_HALT = 0
RECIP_ENDPOINT_IN = 0x82
RECIP_ENDPOINT_OUT = 0x02
# A serial flasher misses the boot ROM now and then.
FLASH_ATTEMPTS = 3


def is_stall(e):
    """Whether a pyusb error is the device stalling."""
    import usb.core

    return isinstance(e, usb.core.USBError) and (
        e.errno == errno.EPIPE or getattr(e, "backend_error_code", None) == -9
    )


def flash(build, serial, log_path, attempts=FLASH_ATTEMPTS):
    """Flash ``build`` with ``pbl run``, on the lab's port for it or ``serial``."""
    command = ["pbl", "-b", build.path, "run"]
    if serial:
        command += ["--tty", serial]
    with open(log_path, "w") as log:
        for _ in range(attempts):
            log.write(shlex.join(command) + "\n")
            log.flush()
            result = subprocess.run(
                command,
                cwd=build.topdir,
                stdout=log,
                stderr=subprocess.STDOUT,
                check=False,
            )
            if result.returncode == 0:
                return
    raise HarnessError(f"flashing failed ({result.returncode}); see {log_path}")


class UsbTarget:
    """The build's device on the host's bus."""

    def __init__(self, vid, pid, match=None):
        self.vid = vid
        self.pid = pid
        self._match = match

    def __str__(self):
        return f"{self.vid:04x}:{self.pid:04x}"

    def find(self, timeout=10.0):
        """The device, waiting up to ``timeout`` seconds for it to enumerate."""
        import usb.core

        deadline = time.monotonic() + timeout
        while True:
            for dev in usb.core.find(
                find_all=True, idVendor=self.vid, idProduct=self.pid
            ):
                if self._match is None or self._match(dev):
                    return dev
            if time.monotonic() >= deadline:
                raise HarnessError(f"no USB device {self} on the host's bus")
            time.sleep(0.2)

    def gone(self, timeout=5.0):
        """Wait for the device to leave the bus, returning whether it did."""
        import usb.core

        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if usb.core.find(idVendor=self.vid, idProduct=self.pid) is None:
                return True
            time.sleep(0.05)
        return False


def is_test_function(dev):
    """Whether ``dev`` has the test function's interface."""
    try:
        cfg = dev.get_active_configuration()
    except Exception:  # noqa: BLE001
        cfg = dev[0]
    intf = cfg[(INTERFACE, 0)]
    return intf.bInterfaceClass == VENDOR_CLASS and intf.bNumEndpoints == 5


class TestFunction:
    """The test function on a configured device."""

    __test__ = False

    def __init__(self, dev, timeout_ms=2000):
        self.dev = dev
        self.timeout_ms = timeout_ms

    @property
    def bulk_mps(self):
        intf = self.dev.get_active_configuration()[(INTERFACE, 0)]
        return next(
            ep for ep in intf if ep.bEndpointAddress == EP_LOOP_IN
        ).wMaxPacketSize

    def configure(self):
        """Select configuration 1 and claim the interface."""
        import usb.util

        if self.configuration() != 1:
            self.dev.set_configuration(1)
        usb.util.claim_interface(self.dev, INTERFACE)

    def release(self):
        import usb.util

        usb.util.dispose_resources(self.dev)

    def configuration(self):
        return self.dev.ctrl_transfer(0x80, 0x08, 0, 0, 1, self.timeout_ms)[0]

    # --- endpoint 0 ---------------------------------------------------------

    def control_write(self, data, request=REQ_WRITE):
        return self.dev.ctrl_transfer(
            VENDOR_OUT, request, 0, INTERFACE, bytes(data), self.timeout_ms
        )

    def control_read(self, length, wlength=None, request=REQ_READ):
        """Read ``length`` bytes of what was written, asking for ``wlength``."""
        return bytes(
            self.dev.ctrl_transfer(
                VENDOR_IN,
                request,
                length,
                INTERFACE,
                length if wlength is None else wlength,
                self.timeout_ms,
            )
        )

    def reset_stats(self):
        self.dev.ctrl_transfer(
            VENDOR_OUT, REQ_RESET, 0, INTERFACE, None, self.timeout_ms
        )

    def stats(self):
        data = self.dev.ctrl_transfer(
            VENDOR_IN, REQ_STATS, 0, INTERFACE, 4 * len(STATS), self.timeout_ms
        )
        return dict(zip(STATS, struct.unpack(f"<{len(STATS)}I", bytes(data))))

    def set_halt(self, ep, halt=True):
        self.dev.ctrl_transfer(
            RECIP_ENDPOINT_OUT,
            SET_FEATURE if halt else CLEAR_FEATURE,
            ENDPOINT_HALT,
            ep,
            None,
            self.timeout_ms,
        )

    def halted(self, ep):
        data = self.dev.ctrl_transfer(
            RECIP_ENDPOINT_IN, GET_STATUS, 0, ep, 2, self.timeout_ms
        )
        return bool(data[0] & 1)

    # --- bulk and interrupt -------------------------------------------------

    def loop(self, data):
        """Send ``data`` and return what comes back."""
        mps = self.bulk_mps
        self.dev.write(EP_LOOP_OUT, data, self.timeout_ms)
        # A transfer of full packets ends with a zero-length one, both ways.
        if len(data) % mps == 0 and len(data) != 0:
            self.dev.write(EP_LOOP_OUT, b"", self.timeout_ms)
        return bytes(self.dev.read(EP_LOOP_IN, len(data) + mps, self.timeout_ms))

    def source(self, length):
        return bytes(self.dev.read(EP_SOURCE, length, self.timeout_ms))

    def sink(self, data):
        return self.dev.write(EP_SINK, data, self.timeout_ms)

    def interrupt(self):
        return bytes(self.dev.read(EP_INT, INT_MPS, self.timeout_ms))


def pattern(packets, mps, seq=0):
    """Packets as the source sends them and the sink expects them: the bytes
    of each count up from its sequence number."""
    return b"".join(
        bytes((seq + p + i) & 0xFF for i in range(mps)) for p in range(packets)
    )


def check_pattern(data, mps):
    """The packets of ``data`` that break the pattern, from the sequence
    number of the first."""
    if not data:
        return []
    seq = data[0]
    return [
        p
        for p in range(len(data) // mps)
        if data[p * mps : (p + 1) * mps] != pattern(1, mps, seq + p)
    ]
