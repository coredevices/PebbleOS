#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""CANopen master for the drive sample.

Resets the drive, starts it, runs it in cyclic synchronous velocity mode with
SYNC and a target velocity in RPDO 2, quick stops it, then runs it in profile
velocity mode and disables it.
"""

import argparse
import sys
import time

import canopen

NODE_ID = 6

SW_STATE = 0x6F
SW_SWITCH_ON_DISABLED = 0x40
SW_OPERATION_ENABLED = 0x27
SW_TARGET_REACHED = 1 << 10

MODE_PV = 3
MODE_CSV = 9


def wait_status(node, mask, value, timeout=2):
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        sw = node.sdo["statusword"].raw
        if sw & mask == value:
            return sw
        time.sleep(0.02)
    sys.exit(f"statusword 0x{sw:04x}, expected 0x{value:04x} in 0x{mask:04x}")


def enable(node, mode):
    node.sdo["modes_of_operation"].raw = mode
    node.sdo["controlword"].raw = 0x06
    node.sdo["controlword"].raw = 0x0F
    wait_status(node, SW_STATE, SW_OPERATION_ENABLED)
    print(f"mode {node.sdo['modes_of_operation_display'].raw}: operation enabled")


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--interface", required=True, help="python-can interface")
    parser.add_argument("--channel", required=True, help="python-can channel")
    parser.add_argument("--bitrate", type=int, default=500000)
    parser.add_argument("--eds", required=True, help="od.eds from the build directory")
    parser.add_argument("--sync", type=float, default=100, help="SYNC rate in Hz")
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
        node.tpdo.read()
        node.rpdo.read()
        node.nmt.state = "OPERATIONAL"
        wait_status(node, SW_STATE, SW_SWITCH_ON_DISABLED)

        network.sync.start(1 / args.sync)
        enable(node, MODE_CSV)
        for target in (1000, -500):
            node.rpdo[2]["target_velocity"].raw = target
            node.rpdo[2].start(1 / args.sync)
            time.sleep(1.5)
            node.tpdo[2].wait_for_reception(1)
            actual = node.tpdo[2]["velocity_actual_value"].raw
            print(f"target {target}: actual {actual}")
            if abs(actual - target) > 10:
                sys.exit("velocity not reached")
            node.rpdo[2].stop()

        node.sdo["controlword"].raw = 0x0B
        wait_status(node, SW_STATE, SW_SWITCH_ON_DISABLED)
        print(f"quick stop: actual {node.sdo['velocity_actual_value'].raw}")

        enable(node, MODE_PV)
        node.sdo["target_velocity"].raw = 2000
        wait_status(node, SW_TARGET_REACHED, SW_TARGET_REACHED, timeout=3)
        print(f"target reached: actual {node.sdo['velocity_actual_value'].raw}")

        node.sdo["controlword"].raw = 0x06
        network.sync.stop()
        node.nmt.state = "PRE-OPERATIONAL"
        print("done")
    finally:
        network.disconnect()


if __name__ == "__main__":
    main()
