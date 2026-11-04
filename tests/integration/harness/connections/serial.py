# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

from pblconsole import ConsoleLink, ConsoleTimeout
from pblconsole.link import DEFAULT_BAUDRATE
from pblconsole.protocol import FrameTransport

from harness.connections import Capability, Connection, start_protocol
from harness.errors import HarnessError, PromptError, WatchTimeout

PROMPT_READY_S = 5


class SerialConnection(Connection):
    """The text console: log lines mixed with the shell, and the Pebble
    protocol in frames between them. The address is ``TTY[@BAUDRATE]`` or a
    ``socket://`` URL."""

    scheme = "serial"
    capabilities = Capability.PROMPT | Capability.LOGS | Capability.PROTOCOL
    help = "the text console on TTY[@BAUDRATE] or socket://HOST:PORT"

    def __init__(self, address, dehasher=None):
        super().__init__(address, dehasher)
        url, _, baudrate = address.partition("@")
        self._link = ConsoleLink(
            url, int(baudrate or DEFAULT_BAUDRATE), on_log=self._on_log
        )
        self._pebble = None
        self._transport = None

    def _on_log(self, line, match):
        if match is None:
            self.emit_log(line)
            return
        level, task, _time, source, message = match
        source = "" if source.startswith(":") else source
        self.emit_log(message, level=level, task=task, source=source)

    def open(self, timeout):
        self._link.open()

    def end_protocol_session(self):
        if self._transport is not None:
            self._transport.disconnect()
            self._transport = None
            self._pebble = None

    def close(self):
        self.end_protocol_session()
        self._link.close()

    @property
    def protocol(self):
        if self._pebble is None or not self._pebble.connected:
            if not self._link.is_open:
                raise HarnessError(f"{self!r} is not open")
            self._transport = FrameTransport(self._link)
            self._pebble = start_protocol(
                self._transport, self.platform, self.settings_sync
            )
        return self._pebble

    def prompt_no_reply(self, command):
        try:
            self._link.command_no_reply(command, PROMPT_READY_S)
        except ConsoleTimeout as e:
            raise WatchTimeout(str(e)) from e

    def prompt(self, command, timeout):
        if not self._link.is_open:
            raise HarnessError(f"{self!r} is not open")
        try:
            lines = self._link.command(command, timeout, PROMPT_READY_S)
        except ConsoleTimeout as e:
            raise WatchTimeout(str(e)) from e
        # Other errors are the command's output, for the caller to read.
        if lines and "command not found" in lines[0]:
            raise PromptError(lines[0])
        return lines
