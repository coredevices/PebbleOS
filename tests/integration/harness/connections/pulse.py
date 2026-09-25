# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import contextlib
import logging
import struct
import threading
import time

from libpebble2.exceptions import PebbleError
from pebble.pulse2.exceptions import SocketClosed

from harness.connections import Capability, Connection, start_protocol
from harness.errors import HarnessError, PromptError, WatchTimeout

LINK_PROBE_S = 5.0
LINK_RETRY_S = 0.5

# Unopened PULSE ports are logged as errors on every packet.
logging.getLogger("pebble.pulse2.transports").setLevel(logging.CRITICAL)


class PulseConnection(Connection):
    """PULSEv2 over the debug serial port: prompt, logs, and the Pebble
    protocol tunneled over it. The address is a tty or a ``socket://`` URL."""

    scheme = "pulse"
    capabilities = Capability.PROMPT | Capability.LOGS | Capability.PROTOCOL
    help = "PULSEv2 on a tty or socket://HOST:PORT"

    def __init__(self, address, dehasher=None):
        super().__init__(address, dehasher)
        self._interface = None
        self._link = None
        self._prompt = None
        self._prompt_lock = threading.Lock()
        self._pebble = None
        self._log_thread = None

    def open(self, timeout):
        from pebble import pulse2
        from pebble.commander import apps

        self._interface = pulse2.Interface.open_dbgserial(url=self.address)
        try:
            logs = apps.StreamingLogs(self._interface)
            self._relink(timeout)
        except BaseException:
            self.close()
            raise

        self._log_thread = threading.Thread(
            target=self._receive_logs, args=(logs,), daemon=True
        )
        self._log_thread.start()

    def close(self):
        if self._pebble is not None:
            with contextlib.suppress(OSError, PebbleError, SocketClosed):
                self._pebble.transport.disconnect()
            self._pebble = None
        if self._interface is not None:
            self._interface.close()
            self._interface = None
        self._link = None
        self._prompt = None

    def _receive_logs(self, logs):
        while True:
            try:
                msg = logs.receive(block=True)
            except SocketClosed:
                return
            except struct.error:
                continue
            self.emit_log(
                msg.message,
                level=msg.log_level,
                task=msg.task,
                source=f"{msg.file_name}:{msg.line_number}",
            )

    def _relink(self, timeout):
        """Get a link that answers the prompt. The firmware restarts the link
        when a host reconnects, which can close the first sockets opened."""
        from pebble.commander import apps
        from pebble.commander.exceptions import CommandTimedOut

        deadline = time.monotonic() + timeout
        while True:
            remaining = max(deadline - time.monotonic(), 0.1)
            self._pebble = None
            self._link = self._interface.get_link(timeout=remaining)
            if self._link is None:
                raise WatchTimeout(f"no PULSE link on {self.address} after {timeout}s")
            try:
                self._prompt = apps.Prompt(self._link)
                if self._prompt.socket is not None:
                    self._prompt.command_and_response("version", timeout=LINK_PROBE_S)
                    return
            except (SocketClosed, CommandTimedOut):
                pass
            if time.monotonic() > deadline:
                raise WatchTimeout(f"the PULSE link on {self.address} is not stable")
            time.sleep(LINK_RETRY_S)

    def prompt(self, command, timeout):
        from pebble.commander.exceptions import CommandTimedOut

        if self._prompt is None:
            raise HarnessError(f"{self!r} is not open")
        with self._prompt_lock:
            for attempt in range(2):
                try:
                    lines = self._prompt.command_and_response(command, timeout=timeout)
                    break
                except CommandTimedOut:
                    raise WatchTimeout(
                        f"prompt command {command!r} timed out"
                    ) from None
                except SocketClosed:
                    if attempt:
                        raise
                    self._relink(timeout)
        if lines and lines[0].startswith("Invalid command"):
            raise PromptError(lines[0])
        return lines

    def prompt_no_reply(self, command):
        with self._prompt_lock:
            self._prompt.socket.send(command.encode())

    @property
    def protocol(self):
        if self._pebble is None or not self._pebble.connected:
            if self._link is None:
                raise HarnessError(f"{self!r} is not open")
            if self._link.closed:
                self._relink(10)
            from libpebble2.communication.transports.pulse import PULSETransport

            self._pebble = start_protocol(PULSETransport(self._link))
        return self._pebble
