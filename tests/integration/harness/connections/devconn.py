# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import contextlib
import time

from websocket import WebSocketException

from harness.connections import Capability, Connection
from harness.errors import WatchTimeout


class DeveloperConnection(Connection):
    """The phone app's developer connection, relaying the Pebble protocol to
    the watch over Bluetooth. App logs are streamed through it; system logs
    and the prompt are not."""

    scheme = "devconn"
    capabilities = Capability.PROTOCOL | Capability.LOGS
    help = "the phone app's developer connection, HOST[:PORT] (default port 9000)"

    DEFAULT_PORT = 9000

    def __init__(self, address, dehasher=None):
        super().__init__(address, dehasher)
        self._pebble = None

    def _url(self):
        if self.address.startswith(("ws://", "wss://")):
            return self.address
        host, sep, port = self.address.partition(":")
        return f"ws://{host}:{port if sep else self.DEFAULT_PORT}/"

    def open(self, timeout):
        from libpebble2.communication import PebbleConnection
        from libpebble2.communication.transports.websocket import WebsocketTransport
        from libpebble2.exceptions import ConnectionError as PebbleConnectionError
        from libpebble2.protocol.logs import AppLogMessage, AppLogShippingControl

        deadline = time.monotonic() + timeout
        while True:
            try:
                pebble = PebbleConnection(WebsocketTransport(self._url()))
                pebble.connect()
                break
            except PebbleConnectionError as e:
                if time.monotonic() > deadline:
                    raise WatchTimeout(f"cannot reach {self._url()}: {e}") from e
                time.sleep(1)
        pebble.run_async()
        pebble.register_endpoint(AppLogMessage, self._on_app_log)
        pebble.send_packet(AppLogShippingControl(enable=True))
        self._pebble = pebble

    def _on_app_log(self, packet):
        self.emit_log(
            packet.message,
            level=str(packet.level),
            task="app",
            source=f"{packet.filename}:{packet.line_number}",
        )

    def close(self):
        if self._pebble is not None:
            with contextlib.suppress(OSError, WebSocketException):
                self._pebble.transport.ws.close()
            self._pebble = None

    @property
    def protocol(self):
        return self._pebble
