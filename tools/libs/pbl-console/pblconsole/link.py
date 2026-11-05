# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""One port shared by log lines, the shell and binary frames."""

import base64
import re
import socket
import struct
import threading
import time
import zlib

DEFAULT_BAUDRATE = 1000000
PROMPT = "pebble> "
CTRL_C = b"\x03"
# Printed by the firmware to clear the input line before a log line.
CLEAR_LINE = "\x1b[K"
PROMPT_RETRY_S = 0.5

# ESC _ <channel> <base64(seq, payload, crc32)> ESC backslash
FRAME_START = b"\x1b_"
FRAME_END = b"\x1b\\"
FRAME_MTU = 512
FRAME_MAX_ENCODED = 1 + (1 + FRAME_MTU + 4 + 2) // 3 * 4
CHANNEL_ACK = b"A"
ACK_TIMEOUT_S = 1.0
SEND_TIMEOUT_S = 20.0

# "<level> <task> <time> <file>:<line>> <message>"; hashed lines have no file.
_LOG_LINE = re.compile(r"^(\S) (\S+) (\S+) (\S*:\d+)> (.*)$")


class ConsoleError(Exception):
    pass


class ConsoleTimeout(ConsoleError):
    pass


def frame_crc(channel, body):
    return zlib.crc32(body, zlib.crc32(channel))


def encode_frame(channel, seq, payload):
    body = bytes([seq]) + payload
    encoded = base64.b64encode(body + struct.pack("<I", frame_crc(channel, body)))
    return FRAME_START + channel + encoded + FRAME_END


class ConsoleLink:
    """A text console. ``on_log(line, match)`` gets the lines that are not
    shell output, with ``match`` the groups (level, task, time, source,
    message) of those that are formatted log lines, else None."""

    def __init__(self, url, baudrate=DEFAULT_BAUDRATE, on_log=None):
        self.url = url
        self.baudrate = baudrate
        self.on_log = on_log
        self._channels = {}
        self._serial = None
        self._reader = None
        self._closing = False
        self._cond = threading.Condition()
        self._shell_lock = threading.Lock()
        self._write_lock = threading.Lock()
        self._frame_lock = threading.Lock()
        # The line being received, which is the prompt when waiting at it.
        self._partial = ""
        self._command = None
        self._echoed = False
        self._output = []
        # Bytes held back for being, or maybe being, part of a frame.
        self._raw = b""
        self._seq = 0
        self._acked = None
        # The last frame handled per channel, as a lost acknowledgment repeats it.
        self._last_seq = {}

    def open(self):
        import serial

        port = serial.serial_for_url(
            self.url, baudrate=self.baudrate, timeout=0.1, do_not_open=True
        )
        # RTS resets the SoC on some boards.
        port.rts = False
        port.open()
        tcp = getattr(port, "_socket", None)
        if tcp is not None:
            # Each frame waits for the last one's acknowledgement.
            tcp.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        self._serial = port
        self._closing = False
        self._raw = b""
        self._partial = ""
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

    @property
    def is_open(self):
        return self._serial is not None

    def set_channel(self, channel, handler):
        """Have ``handler(payload)`` get the frames of ``channel``, from the
        reader thread; None stops it."""
        self._channels[channel] = handler
        self._last_seq.pop(channel, None)

    def _read(self):
        while not self._closing:
            try:
                # Whatever is there, not waiting for more: frames are short.
                data = self._serial.read(1)
                data += self._serial.read(self._serial.in_waiting)
            except (OSError, TypeError, AttributeError):
                # Closing the port under a pending read.
                return
            if data:
                self.feed(data)

    def feed(self, data):
        """Take bytes received from the firmware."""
        with self._cond:
            self._receive(data)
            self._cond.notify_all()

    def _receive(self, data):
        self._raw += data
        while True:
            start = self._raw.find(FRAME_START)
            if start < 0:
                # A trailing ESC may start a frame.
                held = 1 if self._raw.endswith(b"\x1b") else 0
                self._handle_text(self._raw[: len(self._raw) - held])
                self._raw = self._raw[len(self._raw) - held :]
                return
            self._handle_text(self._raw[:start])
            self._raw = self._raw[start:]
            end = self._raw.find(FRAME_END)
            if end < 0:
                if len(self._raw) >= (
                    len(FRAME_START) + FRAME_MAX_ENCODED + len(FRAME_END)
                ):
                    self._raw = self._raw[len(FRAME_START) :]
                    continue
                return
            self._handle_frame(self._raw[len(FRAME_START) : end])
            self._raw = self._raw[end + len(FRAME_END) :]

    def _handle_text(self, data):
        self._partial += data.decode(errors="replace")
        *lines, self._partial = self._partial.split("\n")
        for line in lines:
            self._handle_line(line)

    def _handle_line(self, line):
        # A carriage return restarts the line: the input line was cleared.
        line = line.rstrip("\r").rpartition("\r")[2].replace(CLEAR_LINE, "").strip()
        if not line:
            return
        match = _LOG_LINE.match(line)
        if match is not None:
            self._emit(line, match.groups())
        elif self._command is None:
            if not line.startswith(PROMPT.strip()):
                self._emit(line, None)
        elif not self._echoed:
            self._echoed = line == f"{PROMPT}{self._command}".strip()
        else:
            self._output.append(line)

    def _emit(self, line, match):
        if self.on_log is not None:
            self.on_log(line, match)

    def _handle_frame(self, frame):
        channel = frame[:1]
        try:
            raw = base64.b64decode(frame[1:], validate=True)
        except ValueError:
            return
        if len(raw) < 5 or struct.unpack("<I", raw[-4:])[0] != frame_crc(
            channel, raw[:-4]
        ):
            return
        seq = raw[0]
        payload = raw[1:-4]
        if channel == CHANNEL_ACK:
            if payload:
                self._acked = payload[0]
            return
        if seq != 0:
            try:
                self._write(encode_frame(CHANNEL_ACK, 0, bytes([seq])))
            except OSError:
                return
            if self._last_seq.get(channel) == seq:
                return
            self._last_seq[channel] = seq
        handler = self._channels.get(channel)
        if handler is not None:
            handler(payload)

    def _write(self, data):
        with self._write_lock:
            self._serial.write(data)

    def send_frame(self, channel, payload, seq=None, timeout=SEND_TIMEOUT_S):
        """Send a frame and wait for its acknowledgment, repeating it as
        needed. The firmware has handled a frame when acknowledging it, and
        what it sent meanwhile has arrived. ``seq`` 0 is for frames that do
        no harm if handled twice."""
        with self._frame_lock:
            if seq is None:
                self._seq = self._seq % 255 + 1
                seq = self._seq
            frame = encode_frame(channel, seq, payload)
            deadline = time.monotonic() + timeout
            with self._cond:
                self._acked = None
            while True:
                self._write(frame)
                with self._cond:
                    if self._cond.wait_for(lambda: self._acked == seq, ACK_TIMEOUT_S):
                        return
                if time.monotonic() > deadline:
                    raise ConsoleTimeout(f"frame not acknowledged on {self.url}")

    def _at_prompt(self):
        return self._partial.rpartition("\r")[2].replace(CLEAR_LINE, "") == PROMPT

    def wait_prompt(self, timeout):
        """Get to an empty prompt, which needs the firmware to have booted."""
        deadline = time.monotonic() + timeout
        while True:
            self._write(CTRL_C)
            with self._cond:
                self._cond.wait_for(self._at_prompt, PROMPT_RETRY_S)
                if self._at_prompt():
                    return
            if time.monotonic() > deadline:
                raise ConsoleTimeout("the shell prompt did not appear")

    def command_no_reply(self, command, ready_timeout=5):
        """Run a command after which the firmware stops listening."""
        with self._shell_lock:
            self.wait_prompt(ready_timeout)
            self._write(command.encode() + b"\r")

    def command(self, command, timeout=20, ready_timeout=5):
        """Run a shell command, returning its output lines."""
        with self._shell_lock:
            self.wait_prompt(ready_timeout)
            with self._cond:
                self._command = command
                self._echoed = False
                self._output = []
            try:
                self._write(command.encode() + b"\r")
                with self._cond:
                    done = self._cond.wait_for(
                        lambda: self._echoed and self._at_prompt(), timeout
                    )
                    if not done:
                        raise ConsoleTimeout(f"command {command!r} timed out")
                    return self._output
            finally:
                with self._cond:
                    self._command = None
