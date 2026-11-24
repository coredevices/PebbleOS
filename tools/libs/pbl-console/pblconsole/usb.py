# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""The Pebble protocol over USB, as a libpebble2 transport."""

import struct
import time

import usb.core
import usb.util
from libpebble2.communication.transports import BaseTransport, MessageTargetWatch
from libpebble2.exceptions import ConnectionError as PebbleConnectionError

# Vendor-specific interface of include/pbl/pp/transports/usb.h
IFACE_CLASS = 0xFF
IFACE_SUBCLASS = 0x50
IFACE_PROTOCOL = 0x01
REQ_OPEN = 0x01
REQ_CLOSE = 0x02
# Vendor request, host to device, to the interface
REQ_TYPE = 0x41

READ_LEN = 4096
READ_TIMEOUT_MS = 500
DRAIN_TIMEOUT_MS = 100
CTRL_TIMEOUT_MS = 1000
WRITE_RETRY_MS = 1000
WRITE_TIMEOUT_S = 30.0


def _backend():
    try:
        import libusb_package
    except ImportError:
        return None
    return libusb_package.get_libusb1_backend()


def _is_pp_interface(intf):
    return (
        intf.bInterfaceClass == IFACE_CLASS
        and intf.bInterfaceSubClass == IFACE_SUBCLASS
        and intf.bInterfaceProtocol == IFACE_PROTOCOL
    )


def find_interfaces(vid=None, pid=None):
    """The (device, interface) pairs carrying the Pebble protocol."""
    found = []
    for dev in usb.core.find(find_all=True, backend=_backend()):
        if vid is not None and dev.idVendor != vid:
            continue
        if pid is not None and dev.idProduct != pid:
            continue
        try:
            cfg = dev.get_active_configuration()
        except usb.core.USBError:
            continue
        found += [(dev, intf) for intf in cfg if _is_pp_interface(intf)]
    return found


def parse_selector(selector):
    """``VID:PID`` in hex, either may be empty, to (vid, pid)."""
    if not selector or selector == "any":
        return None, None
    vid, _, pid = selector.partition(":")
    return (int(vid, 16) if vid else None), (int(pid, 16) if pid else None)


class StreamReassembler:
    """Splits a byte stream into Pebble protocol messages."""

    def __init__(self):
        self._buffer = b""

    def feed(self, data):
        self._buffer += data

    def next(self):
        if len(self._buffer) < 4:
            return None
        length = struct.unpack("!H", self._buffer[:2])[0] + 4
        if len(self._buffer) < length:
            return None
        packet, self._buffer = self._buffer[:length], self._buffer[length:]
        return packet


class UsbTransport(BaseTransport):
    """A session on the first USB device with the Pebble protocol interface, or the one
    ``selector``, ``VID:PID``, names."""

    must_initialise = True

    def __init__(self, selector=None):
        self._vid, self._pid = parse_selector(selector)
        self._dev = None
        self._intf = None
        self._ep_in = None
        self._ep_out = None
        self._stream = StreamReassembler()
        self._open = False

    def _request(self, request):
        self._dev.ctrl_transfer(
            REQ_TYPE,
            request,
            0,
            self._intf.bInterfaceNumber,
            None,
            timeout=CTRL_TIMEOUT_MS,
        )

    def _drain(self):
        while True:
            try:
                self._ep_in.read(READ_LEN, timeout=DRAIN_TIMEOUT_MS)
            except usb.core.USBTimeoutError:
                return

    def connect(self):
        found = find_interfaces(self._vid, self._pid)
        if not found:
            raise PebbleConnectionError("no USB device with the Pebble protocol")
        self._dev, self._intf = found[0]
        try:
            usb.util.claim_interface(self._dev, self._intf)
            self._ep_in = usb.util.find_descriptor(
                self._intf,
                custom_match=lambda e: (
                    usb.util.endpoint_direction(e.bEndpointAddress)
                    == usb.util.ENDPOINT_IN
                ),
            )
            self._ep_out = usb.util.find_descriptor(
                self._intf,
                custom_match=lambda e: (
                    usb.util.endpoint_direction(e.bEndpointAddress)
                    == usb.util.ENDPOINT_OUT
                ),
            )
            # What the device sent for an earlier session is dropped
            self._request(REQ_CLOSE)
            self._drain()
            self._request(REQ_OPEN)
        except usb.core.USBError as e:
            raise PebbleConnectionError(str(e)) from e
        self._stream = StreamReassembler()
        self._open = True

    def disconnect(self):
        if not self._open:
            return
        self._open = False
        try:
            self._request(REQ_CLOSE)
        except usb.core.USBError:
            pass
        usb.util.dispose_resources(self._dev)

    @property
    def connected(self):
        return self._open

    def read_packet(self):
        while True:
            packet = self._stream.next()
            if packet is not None:
                return MessageTargetWatch(), packet
            if not self._open:
                raise PebbleConnectionError("the session ended")
            try:
                data = self._ep_in.read(READ_LEN, timeout=READ_TIMEOUT_MS)
            except usb.core.USBTimeoutError:
                continue
            except usb.core.USBError as e:
                self._open = False
                raise PebbleConnectionError(str(e)) from e
            self._stream.feed(bytes(data))

    def _write(self, data):
        # The device takes the next bytes once it has handled the ones before, which may take a
        # while: a write that times out may have sent part of them.
        deadline = time.monotonic() + WRITE_TIMEOUT_S
        sent = 0
        while True:
            try:
                sent += self._ep_out.write(data[sent:], timeout=WRITE_RETRY_MS)
            except usb.core.USBTimeoutError:
                pass
            if sent >= len(data):
                return
            if time.monotonic() >= deadline:
                raise PebbleConnectionError("the device stopped taking data")

    def send_packet(self, message, target=None):
        try:
            self._write(message)
            # A transfer that fills its last packet ends with a zero-length one
            if len(message) % self._ep_out.wMaxPacketSize == 0:
                self._ep_out.write(b"", timeout=WRITE_RETRY_MS)
        except (usb.core.USBError, PebbleConnectionError) as e:
            self._open = False
            raise PebbleConnectionError(str(e)) from e
