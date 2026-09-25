# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import contextlib
import time

from harness.connections import Capability, Connection, start_protocol
from harness.errors import WatchTimeout


class QemuProtocolConnection(Connection):
    """The emulator's Pebble protocol serial port, where the phone would be.
    The port serves one client at a time."""

    scheme = "qemu"
    capabilities = Capability.PROTOCOL
    help = "the emulator's Pebble protocol port, HOST:PORT"

    def __init__(self, address, dehasher=None):
        super().__init__(address, dehasher)
        self._pebble = None

    def open(self, timeout):
        from libpebble2.communication.transports.qemu import QemuTransport
        from libpebble2.exceptions import ConnectionError as PebbleConnectionError

        host, _, port = self.address.rpartition(":")
        deadline = time.monotonic() + timeout
        while True:
            try:
                self._pebble = start_protocol(
                    QemuTransport(host or "127.0.0.1", int(port))
                )
                return
            except PebbleConnectionError as e:
                if time.monotonic() > deadline:
                    raise WatchTimeout(f"cannot reach {self.address}: {e}") from e
                time.sleep(0.5)

    def close(self):
        if self._pebble is not None:
            with contextlib.suppress(OSError):
                self._pebble.transport.socket.close()
            self._pebble = None

    @property
    def protocol(self):
        return self._pebble

    def send_to_qemu(self, packet):
        """Send a packet of libpebble2's QEMU protocol to the emulator."""
        from libpebble2.communication.transports.qemu import MessageTargetQemu

        self._pebble.transport.send_packet(packet, target=MessageTargetQemu())
