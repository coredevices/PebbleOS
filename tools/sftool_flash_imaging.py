#!/usr/bin/env python
# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0
#
# Flash imaging for SF32LB52 (Obelix/Getafix): writes firmware or resources
# directly via sftool to the correct flash addresses, so recovery (PRF) stays
# in PBLBOOT format for the bootloader.
#
# Same CLI as console_flash_imaging.


import argparse
import subprocess
import sys


def run_sftool(tty, path, address, chip="SF32LB52"):
    cmd = ["sftool", "-c", chip, "-p", tty, "write_flash", f"{path}@0x{address:x}"]
    r = subprocess.call(cmd)
    return r == 0


def main():
    parser = argparse.ArgumentParser(
        description="Load firmware or resources into flash via sftool (SF32LB52)."
    )
    parser.add_argument(
        "-t", "--tty", required=True, help="Serial port (e.g. /dev/ttyACM0)"
    )
    parser.add_argument("-c", "--chip", default="SF32LB52", help="Chip for sftool -c")
    parser.add_argument(
        "-v", "--verbose", action="store_true", help="(ignored, for CLI compatibility)"
    )
    parser.add_argument(
        "-p", "--progress", action="store_true", help="(ignored, for CLI compatibility)"
    )
    parser.add_argument(
        "--address",
        required=True,
        type=lambda text: int(text, 0),
        help="Address to write to, where the chip maps its flash",
    )
    subparsers = parser.add_subparsers(dest="which", required=True)

    fw = subparsers.add_parser(
        "firmware", help="load recovery firmware into SAFE_FIRMWARE"
    )
    fw.add_argument("file", help="path to PRF .bin")
    res = subparsers.add_parser(
        "resources", help="load resources into SYSTEM_RESOURCES"
    )
    res.add_argument("file", help="path to .pbpack")

    args = parser.parse_args()

    addr = args.address
    if args.which == "firmware":
        name = "recovery (SAFE_FIRMWARE)"
    else:
        name = "resources (SYSTEM_RESOURCES)"

    print(f"Writing {name} to 0x{addr:x} via sftool...")
    if run_sftool(args.tty, args.file, addr, chip=args.chip):
        print("Success!")
    else:
        print("Fail!")
        sys.exit(1)


if __name__ == "__main__":
    main()
