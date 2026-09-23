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
CTRL_C = b"\x03"
CTRL_D = b"\x04"


class SerialConnection(Connection):
    """The legacy text console (firmware without PULSE): log lines, and the
    prompt entered with Ctrl-C. The address is ``TTY[@BAUDRATE]`` or a
    ``socket://`` URL."""

    scheme = "serial"
    capabilities = Capability.PROMPT | Capability.LOGS
    help = "the legacy text console on TTY[@BAUDRATE] or socket://HOST:PORT"

    def __init__(self, address, dehasher=None):
        super().__init__(address, dehasher)
        self._serial = None
        self._reader = None
        self._closing = False
        self._in_prompt = False
        self._prompt_buf = ""
        self._cond = threading.Condition()
        self._prompt_lock = threading.Lock()

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
        line = ""
        while not self._closing:
            try:
                data = self._serial.read(256)
            except (OSError, TypeError, AttributeError):
                # Closing the port under a pending read.
                return
            if not data:
                continue
            text = data.decode(errors="replace")
            with self._cond:
                if self._in_prompt:
                    self._prompt_buf += text
                    self._cond.notify_all()
                    continue
            for char in text:
                if char == "\n":
                    line = line.strip()
                    if line and line != "^D":
                        self._emit_line(line)
                    line = ""
                elif char != "\r":
                    line += char

    def _emit_line(self, line):
        match = LOG_LINE.match(line)
        if match is None:
            self.emit_log(line)
            return
        level, task, _time, source, message = match.groups()
        source = "" if source.startswith(":") else source
        self.emit_log(message, level=level, task=task, source=source)

    def _wait_prompt_until(self, done, timeout, what):
        deadline = time.monotonic() + timeout
        with self._cond:
            while not done(self._prompt_buf):
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    raise WatchTimeout(f"{what} timed out")
                self._cond.wait(remaining)
            return self._prompt_buf

    def prompt_no_reply(self, command):
        with self._prompt_lock:
            with self._cond:
                self._in_prompt = True
                self._prompt_buf = ""
            self._serial.write(CTRL_C)
            self._wait_prompt_until(lambda b: ">" in b, 5, "entering the prompt")
            self._serial.write(command.encode() + b"\r")

    def prompt(self, command, timeout):
        if self._serial is None:
            raise HarnessError(f"{self!r} is not open")
        with self._prompt_lock:
            with self._cond:
                self._in_prompt = True
                self._prompt_buf = ""
            try:
                self._serial.write(CTRL_C)
                self._wait_prompt_until(lambda b: ">" in b, 5, "entering the prompt")
                with self._cond:
                    self._prompt_buf = ""
                self._serial.write(command.encode() + b"\r")
                # The next prompt is a '>' at the start of a line.
                output = self._wait_prompt_until(
                    lambda b: b.endswith("\n>"), timeout, f"prompt command {command!r}"
                )
            finally:
                self._serial.write(CTRL_D)
                with self._cond:
                    self._in_prompt = False

        lines = [line.strip() for line in output.split("\n")[1:-1]]
        lines = [line for line in lines if line]
        if lines and lines[0].startswith("Invalid command"):
            raise PromptError(lines[0])
        return lines
