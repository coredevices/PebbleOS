# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""A libpebble2 transport over a :class:`~harness.ble.BleLink`."""

import queue
import struct

from libpebble2.communication.transports import BaseTransport, MessageTargetWatch
from libpebble2.exceptions import ConnectionError as PebbleConnectionError

_CLOSED = object()


class BleTransport(BaseTransport):
    # The watch asks for the phone's version; libpebble2 answers it.
    must_initialise = True

    def __init__(self, link):
        self.link = link
        self._frames = queue.Queue()
        self._buffer = b""

    def connect(self):
        self.link.on_data = self._on_data
        self.link.on_disconnect = lambda: self._frames.put(_CLOSED)

    @property
    def connected(self):
        return self.link.is_connected

    def _on_data(self, data):
        # PPoGATT carries a byte stream; split it into Pebble Protocol frames.
        self._buffer += data
        while len(self._buffer) >= 4:
            (length,) = struct.unpack(">H", self._buffer[:2])
            if len(self._buffer) < length + 4:
                break
            self._frames.put(self._buffer[: length + 4])
            self._buffer = self._buffer[length + 4 :]

    def read_packet(self):
        frame = self._frames.get()
        if frame is _CLOSED:
            raise PebbleConnectionError("BLE link closed")
        return MessageTargetWatch(), frame

    def send_packet(self, message, target=None):
        self.link.send(message)
