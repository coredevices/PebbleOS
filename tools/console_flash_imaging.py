#!/usr/bin/env python
# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Writes a recovery firmware or resources over the console."""

import argparse
import sys

from pblconsole import ConsoleError, ConsoleLink
from pblconsole.imaging import Imager


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("-t", "--tty", required=True, help="the target serial port")
    parser.add_argument(
        "-p", "--partition", required=True, choices=["firmware", "resources"]
    )
    parser.add_argument("file", help="a .bin firmware or a .pbpack")
    args = parser.parse_args()

    with open(args.file, "rb") as f:
        data = f.read()

    def progress(done, total):
        print(f"\rwriting {100 * done // total}%", end="", flush=True)

    link = ConsoleLink(args.tty)
    link.open()
    try:
        imager = Imager(link, progress)
        if args.partition == "firmware":
            imager.load_recovery(data)
        else:
            imager.load_resources(data)
    except ConsoleError as e:
        print(f"\nimaging failed: {e}")
        sys.exit(1)
    finally:
        link.close()
    print("\ndone")


if __name__ == "__main__":
    main()
