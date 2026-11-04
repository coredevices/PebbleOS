# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import base64
import queue
import re
import struct
import threading
import time
import zlib

from libpebble2.communication.transports import BaseTransport, MessageTargetWatch
from libpebble2.exceptions import ConnectionError as PebbleConnectionError

from harness.connections import Capability, Connection, start_protocol
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

# ESC _ <channel> <base64(seq, payload, crc32)> ESC \\
FRAME_START = b"\x1b_"
FRAME_END = b"\x1b\\"
FRAME_MTU = 512
FRAME_MAX_ENCODED = 1 + (1 + FRAME_MTU + 4 + 2) // 3 * 4
CHANNEL_ACK = b"A"
CHANNEL_PP = b"P"
ACK_TIMEOUT_S = 1.0
SEND_TIMEOUT_S = 20.0

PP_DATA = 1
PP_OPEN = 2
PP_CLOSE = 3
PP_OPEN_TIMEOUT_S = 10.0


def _frame_crc(channel, body):
    return zlib.crc32(body, zlib.crc32(channel))


class FrameTransport(BaseTransport):
    """The Pebble protocol in frames of a :class:`SerialConnection`, as a
    libpebble2 transport."""

    must_initialise = True

    def __init__(self, connection):
        self._connection = connection
        self._received = queue.Queue()
        self._buffer = b""
        self._open = False

    def connect(self):
        self._connection.send_frame(CHANNEL_PP, bytes([PP_OPEN]), seq=0)
        deadline = time.monotonic() + PP_OPEN_TIMEOUT_S
        while True:
            try:
                item = self._received.get(timeout=max(deadline - time.monotonic(), 0))
            except queue.Empty:
                raise PebbleConnectionError("no answer to the session open") from None
            if item is not None and item[0] == PP_OPEN:
                self._open = True
                return

    def disconnect(self):
        if self._open:
            self._open = False
            try:
                self._connection.send_frame(CHANNEL_PP, bytes([PP_CLOSE]), seq=0)
            except (OSError, WatchTimeout, HarnessError):
                pass
            self._received.put(None)

    def lost(self):
        """The session ended without the host asking."""
        self._open = False
        self._received.put(None)

    def feed(self, payload):
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
            if item[0] == PP_DATA:
                self._buffer += item[1]
            elif item[0] == PP_CLOSE:
                self._open = False
                raise PebbleConnectionError("the firmware ended the session")

    def send_packet(self, message, target=None):
        try:
            for i in range(0, len(message), FRAME_MTU - 1):
                self._connection.send_frame(
                    CHANNEL_PP, bytes([PP_DATA]) + message[i : i + FRAME_MTU - 1]
                )
        except WatchTimeout as e:
            self.lost()
            raise PebbleConnectionError(str(e)) from e


class SerialConnection(Connection):
    """The text console: log lines mixed with the shell, and the Pebble
    protocol in frames between them. The address is ``TTY[@BAUDRATE]`` or a
    ``socket://`` URL."""

    scheme = "serial"
    capabilities = Capability.PROMPT | Capability.LOGS | Capability.PROTOCOL
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
        # Bytes held back for being, or maybe being, part of a frame.
        self._raw = b""
        self._write_lock = threading.Lock()
        self._frame_lock = threading.Lock()
        self._seq = 0
        self._acked = None
        self._pebble = None
        self._transport = None

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
        self._raw = b""
        self._reader = threading.Thread(target=self._read, daemon=True)
        self._reader.start()

    def end_protocol_session(self):
        if self._transport is not None:
            self._transport.disconnect()
            self._transport = None
            self._pebble = None

    def close(self):
        self.end_protocol_session()
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
                # Whatever is there, not waiting for more: frames are short.
                data = self._serial.read(1)
                data += self._serial.read(self._serial.in_waiting)
            except (OSError, TypeError, AttributeError):
                # Closing the port under a pending read.
                return
            if not data:
                continue
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
                    self._handle_bad_frame()
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

    def _handle_frame(self, frame):
        channel = frame[:1]
        try:
            raw = base64.b64decode(frame[1:], validate=True)
        except ValueError:
            self._handle_bad_frame()
            return
        if len(raw) < 5 or struct.unpack("<I", raw[-4:])[0] != _frame_crc(
            channel, raw[:-4]
        ):
            self._handle_bad_frame()
            return
        payload = raw[1:-4]
        if channel == CHANNEL_ACK and payload:
            self._acked = payload[0]
        elif channel == CHANNEL_PP and self._transport is not None:
            self._transport.feed(payload)

    def _handle_bad_frame(self):
        # Frames from the firmware are not repeated, so the stream has a hole.
        if self._transport is not None:
            self._transport.lost()

    def send_frame(self, channel, payload, seq=None):
        """Send a frame and wait for its acknowledgment, repeating it as
        needed. ``seq`` 0 is for frames that do no harm if handled twice."""
        with self._frame_lock:
            if seq is None:
                self._seq = self._seq % 255 + 1
                seq = self._seq
            body = bytes([seq]) + payload
            encoded = base64.b64encode(
                body + struct.pack("<I", _frame_crc(channel, body))
            )
            frame = FRAME_START + channel + encoded + FRAME_END
            deadline = time.monotonic() + SEND_TIMEOUT_S
            with self._cond:
                self._acked = None
            while True:
                self._write(frame)
                with self._cond:
                    if self._cond.wait_for(lambda: self._acked == seq, ACK_TIMEOUT_S):
                        return
                if time.monotonic() > deadline:
                    raise WatchTimeout(f"frame not acknowledged on {self!r}")

    def _write(self, data):
        with self._write_lock:
            self._serial.write(data)

    @property
    def protocol(self):
        if self._pebble is None or not self._pebble.connected:
            if self._serial is None:
                raise HarnessError(f"{self!r} is not open")
            self._transport = FrameTransport(self)
            self._pebble = start_protocol(self._transport, self.platform)
        return self._pebble

    def _handle_line(self, line):
        # A carriage return restarts the line: the input line was cleared.
        line = line.rstrip("\r").rpartition("\r")[2].replace(CLEAR_LINE, "").strip()
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
        return self._partial.rpartition("\r")[2].replace(CLEAR_LINE, "") == PROMPT

    def _wait_prompt(self, timeout):
        """Get to an empty prompt, which needs the firmware to have booted."""
        deadline = time.monotonic() + timeout
        while True:
            self._write(CTRL_C)
            with self._cond:
                self._cond.wait_for(self._at_prompt, PROMPT_RETRY_S)
                if self._at_prompt():
                    return
            if time.monotonic() > deadline:
                raise WatchTimeout("the shell prompt did not appear")

    def prompt_no_reply(self, command):
        with self._prompt_lock:
            self._wait_prompt(PROMPT_READY_S)
            self._write(command.encode() + b"\r")

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
                self._write(command.encode() + b"\r")
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
