# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import re
import threading
import time

from harness.connections import Capability, Connection
from harness.errors import HarnessError, PromptError, WatchTimeout

DEFAULT_BAUDRATE = 115200
# "<level> <task> <time> <file>:<line>> <message>"; hashed lines have no file.
LOG_LINE = re.compile(r"^(\S) (\S+) (\S+) (\S*:\d+)> (.*)$")
PROMPT = "pebble> "
CTRL_C = b"\x03"
# Printed by the firmware to clear the input line before a log line.
CLEAR_LINE = "\x1b[K"
PROMPT_RETRY_S = 0.5
PROMPT_READY_S = 5


class SerialConnection(Connection):
    """The text console: log lines mixed with the shell. The address is
    ``TTY[@BAUDRATE]`` or a ``socket://`` URL."""

    scheme = "serial"
    capabilities = Capability.PROMPT | Capability.LOGS
    help = "the text console on TTY[@BAUDRATE] or socket://HOST:PORT"

    def __init__(self, address, dehasher=None):
        super().__init__(address, dehasher)
        self._serial = None
        self._reader = None
        self._closing = False
        self._cond = threading.Condition()
        self._prompt_lock = threading.Lock()
        # The line being received, which is the prompt when waiting at it.
        self._partial = ""
        self._command = None
        self._echoed = False
        self._output = []

    def open(self, timeout):
        import serial

        url, _, baudrate = self.address.partition("@")
        port = serial.serial_for_url(
            url,
            baudrate=int(baudrate or DEFAULT_BAUDRATE),
            timeout=0.1,
            do_not_open=True,
        )
        # RTS resets the SoC on some boards.
        port.rts = False
        port.open()
        self._serial = port
        self._closing = False
        self._reader = threading.Thread(target=self._read, daemon=True)
        self._reader.start()

    def close(self):
        self._closing = True
        if self._reader is not None:
            self._reader.join(timeout=2)
            self._reader = None
        if self._serial is not None:
            self._serial.close()
            self._serial = None

    def _read(self):
        while not self._closing:
            try:
                data = self._serial.read(256)
            except (OSError, TypeError, AttributeError):
                # Closing the port under a pending read.
                return
            if not data:
                continue
            with self._cond:
                self._partial += data.decode(errors="replace").replace(CLEAR_LINE, "")
                *lines, self._partial = self._partial.split("\n")
                for line in lines:
                    self._handle_line(line)
                self._cond.notify_all()

    def _handle_line(self, line):
        # A carriage return restarts the line: the input line was cleared.
        line = line.rstrip("\r").rpartition("\r")[2].strip()
        if not line:
            return
        match = LOG_LINE.match(line)
        if match is not None:
            level, task, _time, source, message = match.groups()
            source = "" if source.startswith(":") else source
            self.emit_log(message, level=level, task=task, source=source)
        elif self._command is None:
            if not line.startswith(PROMPT.strip()):
                self.emit_log(line)
        elif not self._echoed:
            self._echoed = line == f"{PROMPT}{self._command}".strip()
        else:
            self._output.append(line)

    def _at_prompt(self):
        return self._partial.rpartition("\r")[2] == PROMPT

    def _wait_prompt(self, timeout):
        """Get to an empty prompt, which needs the firmware to have booted."""
        deadline = time.monotonic() + timeout
        while True:
            self._serial.write(CTRL_C)
            with self._cond:
                self._cond.wait_for(self._at_prompt, PROMPT_RETRY_S)
                if self._at_prompt():
                    return
            if time.monotonic() > deadline:
                raise WatchTimeout("the shell prompt did not appear")

    def prompt_no_reply(self, command):
        with self._prompt_lock:
            self._wait_prompt(PROMPT_READY_S)
            self._serial.write(command.encode() + b"\r")

    def prompt(self, command, timeout):
        if self._serial is None:
            raise HarnessError(f"{self!r} is not open")
        with self._prompt_lock:
            self._wait_prompt(PROMPT_READY_S)
            with self._cond:
                self._command = command
                self._echoed = False
                self._output = []
            try:
                self._serial.write(command.encode() + b"\r")
                with self._cond:
                    done = self._cond.wait_for(
                        lambda: self._echoed and self._at_prompt(), timeout
                    )
                    if not done:
                        raise WatchTimeout(f"prompt command {command!r} timed out")
                    lines = self._output
            finally:
                with self._cond:
                    self._command = None

        # Other errors are the command's output, for the caller to read.
        if lines and "command not found" in lines[0]:
            raise PromptError(lines[0])
        return lines
