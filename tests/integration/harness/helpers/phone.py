# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""The phone app, played by the harness: a Bumble host that pairs with the
watch and speaks the Pebble protocol over reversed PPoGATT."""

import logging
import struct

from harness.ble import AUTO, HOST_ADDRESS, HOST_NAME, REVERSED, BleLink
from harness.errors import HarnessError

logger = logging.getLogger(__name__)

RESET_ENDPOINT = 2003
RESET_INTO_RECOVERY = 0xFF


def send_raw(pebble, endpoint, payload):
    """Send ``payload`` to ``endpoint``, for messages libpebble2 cannot
    encode."""
    pebble.send_raw(struct.pack(">HH", len(payload), endpoint) + payload)


class Phone:
    def __init__(
        self,
        dut,
        keystore,
        address=HOST_ADDRESS,
        name=HOST_NAME,
        watch=AUTO,
        ppogatt=REVERSED,
    ):
        if not dut.ble_controller:
            raise HarnessError("no Bluetooth controller for the phone")
        self.dut = dut
        self.link = BleLink(
            watch,
            dut.ble_controller,
            keystore=keystore,
            confirm_pairing=dut.confirm_pairing,
            address=address,
            name=name,
            ppogatt=ppogatt,
        )
        self.pebble = None

    @property
    def name(self):
        return self.link.name

    def connect(self, timeout=90.0):
        from harness.ble.transport import BleTransport
        from harness.connections import start_protocol

        self.link.open(timeout)
        self.pebble = start_protocol(BleTransport(self.link))
        return self

    def disconnect(self):
        self.pebble = None
        self.link.close()

    def watch_version(self, timeout=15):
        from libpebble2.protocol.system import WatchVersion, WatchVersionRequest

        response = self.pebble.send_and_read(
            WatchVersion(data=WatchVersionRequest()), WatchVersion, timeout=timeout
        )
        return response.data.running

    def reset_into_recovery(self):
        """What the app's 'Reset to PRF' sends."""
        send_raw(self.pebble, RESET_ENDPOINT, bytes([RESET_INTO_RECOVERY]))

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.disconnect()
