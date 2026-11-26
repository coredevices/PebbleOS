#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Echo the pings of the CAN sample: each 0x100 frame back as 0x101.

Needs python-can and a CAN adapter it supports, e.g.:

    pip install python-can
    ./echo.py --interface socketcan --channel can0
    ./echo.py --interface pcan --channel PCAN_USBBUS1
"""

import argparse

import can

PING_ID = 0x100
ECHO_ID = 0x101


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--interface", required=True, help="python-can interface")
    parser.add_argument("--channel", required=True, help="adapter channel")
    parser.add_argument(
        "--bitrate", type=int, default=500000, help="bit rate (default: 500000)"
    )
    args = parser.parse_args()

    with can.Bus(
        interface=args.interface, channel=args.channel, bitrate=args.bitrate
    ) as bus:
        print(f"echoing 0x{PING_ID:03x} as 0x{ECHO_ID:03x}, Ctrl-C to stop", flush=True)
        try:
            for msg in bus:
                if (
                    msg.arbitration_id != PING_ID
                    or msg.is_extended_id
                    or msg.is_remote_frame
                ):
                    continue
                bus.send(
                    can.Message(
                        arbitration_id=ECHO_ID, data=msg.data, is_extended_id=False
                    )
                )
                print(f"echoed {msg.data.hex()}", flush=True)
        except KeyboardInterrupt:
            pass


if __name__ == "__main__":
    main()
