#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""CANopen master for the device sample.

Resets the node, reads its identity and label over SDO, writes a new label,
reads its PDO configuration, starts it and checks the counter TPDO and the
output RPDO, then optionally sends SYNC for a while.
"""

import argparse
import sys
import time

import canopen

NODE_ID = 5


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "--interface", required=True, help="python-can interface, e.g. socketcan"
    )
    parser.add_argument(
        "--channel", required=True, help="python-can channel, e.g. can0"
    )
    parser.add_argument("--bitrate", type=int, default=500000)
    parser.add_argument(
        "--sync",
        type=float,
        default=0,
        help="send SYNC at this rate, in Hz, at the end",
    )
    parser.add_argument(
        "--duration", type=float, default=10, help="how long to send SYNC, in seconds"
    )
    parser.add_argument(
        "--eds", required=True, help="od.eds from the sample's build directory"
    )
    args = parser.parse_args()

    network = canopen.Network()
    network.connect(
        interface=args.interface, channel=args.channel, bitrate=args.bitrate
    )
    node = canopen.RemoteNode(NODE_ID, args.eds)
    network.add_node(node)

    try:
        node.nmt.state = "RESET"
        node.nmt.wait_for_bootup(5)
        print("boot-up received")

        print(f"device name: {node.sdo[0x1008].raw}")
        print(f"product code: 0x{node.sdo[0x1018][2].raw:x}")
        print(f"label: {node.sdo['label'].raw}")
        node.sdo["label"].raw = "written by master.py"
        print(f"label: {node.sdo['label'].raw}")

        node.tpdo.read()
        node.rpdo.read()

        node.nmt.state = "OPERATIONAL"
        node.nmt.wait_for_heartbeat(2)
        print(f"state: {node.nmt.state}")

        values = []
        for _ in range(3):
            node.tpdo[1].wait_for_reception(2)
            values.append(node.tpdo[1]["counter"].raw)
        print(f"counter: {values}")
        if values != sorted(values) or len(set(values)) != len(values):
            sys.exit("counter not increasing")

        for value in (0x5A, 0xA5):
            node.rpdo[1]["output"].raw = value
            node.rpdo[1].transmit()
            time.sleep(0.1)
            if node.sdo["output"].raw != value:
                sys.exit(f"output not 0x{value:x}")
        print("output written")

        if args.sync:
            print(f"SYNC at {args.sync:g} Hz for {args.duration:g} s")
            network.sync.start(1 / args.sync)
            time.sleep(args.duration)
            network.sync.stop()

        node.nmt.state = "PRE-OPERATIONAL"
        print("done")
    finally:
        network.disconnect()


if __name__ == "__main__":
    main()
