# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""The Pebble protocol in frames, as a libpebble2 transport."""

import queue
import struct
import time

from libpebble2.communication.transports import BaseTransport, MessageTargetWatch
from libpebble2.exceptions import ConnectionError as PebbleConnectionError

from .link import FRAME_MTU, ConsoleError, ConsoleLink

CHANNEL = b"P"
DATA = 1
OPEN = 2
CLOSE = 3
OPEN_TIMEOUT_S = 10.0
OPEN_RETRY_S = 1.0
CLOSE_TIMEOUT_S = 2.0


class FrameTransport(BaseTransport):
    """Sessions on an open :class:`ConsoleLink`."""

    must_initialise = True

    def __init__(self, link):
        self._link = link
        self._received = queue.Queue()
        self._buffer = b""
        self._open = False

    def connect(self):
        self._link.set_channel(CHANNEL, self._feed)
        deadline = time.monotonic() + OPEN_TIMEOUT_S
        while time.monotonic() < deadline:
            # Repeats do no harm: seq 0. So can its answer get lost, though.
            self._link.send_frame(CHANNEL, bytes([OPEN]), seq=0)
            try:
                item = self._received.get(timeout=OPEN_RETRY_S)
            except queue.Empty:
                continue
            if item is not None and item[0] == OPEN:
                self._open = True
                return
        raise PebbleConnectionError("no answer to the session open")

    def disconnect(self):
        if self._open:
            self._open = False
            try:
                self._link.send_frame(
                    CHANNEL, bytes([CLOSE]), seq=0, timeout=CLOSE_TIMEOUT_S
                )
            except (OSError, ConsoleError):
                pass
            self._received.put(None)

    def lost(self):
        """The session ended without the host asking."""
        self._open = False
        self._received.put(None)

    def _feed(self, payload):
        if payload:
            self._received.put((payload[0], payload[1:]))

    @property
    def connected(self):
        return self._open

    def read_packet(self):
        while True:
            if len(self._buffer) >= 2:
                length = struct.unpack("!H", self._buffer[:2])[0] + 4
                if len(self._buffer) >= length:
                    packet = self._buffer[:length]
                    self._buffer = self._buffer[length:]
                    return MessageTargetWatch(), packet
            item = self._received.get()
            if item is None:
                raise PebbleConnectionError("the session ended")
            if item[0] == DATA:
                self._buffer += item[1]
            elif item[0] == CLOSE:
                self._open = False
                raise PebbleConnectionError("the firmware ended the session")

    def send_packet(self, message, target=None):
        try:
            for i in range(0, len(message), FRAME_MTU - 1):
                self._link.send_frame(
                    CHANNEL, bytes([DATA]) + message[i : i + FRAME_MTU - 1]
                )
        except ConsoleError as e:
            self.lost()
            raise PebbleConnectionError(str(e)) from e


class ConsoleTransport(FrameTransport):
    """A :class:`FrameTransport` with a link of its own, on a port or URL."""

    def __init__(self, url):
        link = ConsoleLink(url)
        link.open()
        super().__init__(link)
