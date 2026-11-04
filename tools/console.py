#!/usr/bin/env python
# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""A plain terminal on the console, which only dehashes log lines."""

import argparse
import os
import select
import sys
import termios
import tty

import serial

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "log_hashing"))
from logdehash import LogDehash

QUIT = 0x1C  # Ctrl-\


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("url", help="serial port or socket://HOST:PORT")
    parser.add_argument("--baudrate", type=int, default=1000000)
    parser.add_argument("--dict", help="log hash dictionary, if the build has one")
    args = parser.parse_args()

    dehasher = None
    if args.dict and os.path.exists(args.dict):
        dehasher = LogDehash(args.dict, justify="small", color=True)

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
    line = b""
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
                continue
            sys.stdout.buffer.write(data)
            sys.stdout.buffer.flush()
            if dehasher is None:
                continue
            # Lines show as they arrive; hashed ones are replaced when complete.
            *lines, line = (line + data).split(b"\n")
            for raw in lines:
                text = raw.decode(errors="replace").replace("\x1b[K", "").strip()
                text = text.rpartition("\r")[2]
                if "NL:" in text:
                    formatted = dehasher.minicom_format_line(dehasher.dehash(text))
                    sys.stdout.write(f"\x1b[A\r{formatted}")
                    sys.stdout.flush()
    finally:
        if saved is not None:
            termios.tcsetattr(stdin, termios.TCSADRAIN, saved)
        port.close()


if __name__ == "__main__":
    main()
