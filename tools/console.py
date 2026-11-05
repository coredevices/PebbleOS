#!/usr/bin/env python
# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""A plain terminal on the console, which only dehashes log lines."""

import argparse
import os
import re
import select
import sys
import termios
import time
import tty

import serial

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "log_hashing"))
from logdehash import LogDehash

QUIT = 0x1C  # Ctrl-\
# How long a line that may be a log line waits for its end.
HOLD_S = 0.2
ERASE = b"\x1b[K"
LINE_END = re.compile(rb"[\r\n]")
LOG_START = re.compile(rb"\x1b\[?|[*DIWE]( (-( .*)?)?)?", re.DOTALL)


def _text(raw):
    return raw.replace(ERASE, b"").replace(b"\0", b"").strip().rpartition(b"\r")[2]


class Dehasher:
    """Replaces hashed log lines; anything else passes through as it arrives."""

    def __init__(self, dehasher):
        self._dehasher = dehasher
        self._held = b""
        self._passing = False

    def _line(self, raw):
        text = _text(raw)
        if b"NL:" not in text:
            return raw + b"\n"
        fields = self._dehasher.dehash(text.decode(errors="replace"))
        return (
            b"\r"
            + ERASE
            + self._dehasher.commander_format_line(fields).encode()
            + b"\r\n"
        )

    def feed(self, data):
        out = b""
        if self._passing:
            # The firmware starts a log line with a carriage return, even
            # with the prompt on the screen.
            end = LINE_END.search(data)
            if end is None:
                return data
            cut = end.start() + (data[end.start()] == 0x0A)
            out, data = data[:cut], data[cut:]
            self._passing = False
        *lines, tail = data.split(b"\n")
        for line in lines:
            out += self._line(self._held + line)
            self._held = b""
        self._held += tail
        if not self._may_be_log():
            start = self._held.rfind(b"\r")
            if start > 0:
                out += self._held[:start]
                self._held = self._held[start:]
            if not self._may_be_log():
                out += self.flush()
        return out

    def _may_be_log(self):
        text = _text(self._held)
        return not text or LOG_START.fullmatch(text)

    def flush(self):
        if not _text(self._held):
            return b""
        out, self._held = self._held, b""
        self._passing = True
        return out


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("url", help="serial port or socket://HOST:PORT")
    parser.add_argument("--baudrate", type=int, default=1000000)
    parser.add_argument("--dict", help="log hash dictionary, if the build has one")
    args = parser.parse_args()

    dehasher = None
    if args.dict and os.path.exists(args.dict):
        dehasher = Dehasher(LogDehash(args.dict, justify="small", color=True))

    port = serial.serial_for_url(
        args.url, baudrate=args.baudrate, timeout=0, do_not_open=True
    )
    # RTS resets the SoC on some boards.
    port.rts = False
    port.open()

    print(f"--- {args.url}; Ctrl-\\ quits ---\r")
    stdin = sys.stdin.fileno()
    saved = termios.tcgetattr(stdin) if os.isatty(stdin) else None
    if saved is not None:
        tty.setraw(stdin)
    last = time.monotonic()
    try:
        while True:
            readable, _, _ = select.select([stdin], [], [], 0.01)
            if readable:
                data = os.read(stdin, 256)
                if not data or QUIT in data:
                    return
                port.write(data)
            data = port.read(4096)
            if not data:
                if dehasher and time.monotonic() - last > HOLD_S:
                    data = dehasher.flush()
                    sys.stdout.buffer.write(data)
                    sys.stdout.buffer.flush()
                    last = time.monotonic()
                continue
            last = time.monotonic()
            sys.stdout.buffer.write(dehasher.feed(data) if dehasher else data)
            sys.stdout.buffer.flush()
    finally:
        if saved is not None:
            termios.tcsetattr(stdin, termios.TCSADRAIN, saved)
        port.close()


if __name__ == "__main__":
    main()
