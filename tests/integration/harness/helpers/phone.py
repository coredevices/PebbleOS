# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""The phone app. Tests use :class:`Phone`; :func:`make_phone` gives the one
the setup has: the harness itself on a Bluetooth controller (Bumble), or,
eventually, a phone running CoreApp."""

import logging
import os
import struct

from harness.ble import AUTO, HOST_ADDRESS, HOST_NAME, REVERSED, BleLink
from harness.errors import HarnessError, Unsupported
from harness.lab import PHONE_BUMBLE, PHONE_COREAPP

logger = logging.getLogger(__name__)

RESET_ENDPOINT = 2003
RESET_INTO_RECOVERY = 0xFF


def send_raw(pebble, endpoint, payload):
    """Send ``payload`` to ``endpoint``, for messages libpebble2 cannot
    encode."""
    pebble.send_raw(struct.pack(">HH", len(payload), endpoint) + payload)


class Phone:
    """A phone with the Pebble app: it connects to the watch and carries the
    Pebble protocol (``pebble``, a libpebble2 connection)."""

    name = None
    address = None
    pebble = None

    def connect(self, timeout=90.0):
        raise NotImplementedError

    def disconnect(self):
        raise NotImplementedError

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


class BumblePhone(Phone):
    """The harness as the phone, through a Bluetooth controller."""

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

    @property
    def name(self):
        return self.link.name

    @property
    def address(self):
        return self.link.address

    def connect(self, timeout=90.0):
        from harness.ble.transport import BleTransport
        from harness.connections import start_protocol

        self.link.open(timeout)
        self.pebble = start_protocol(BleTransport(self.link))
        return self

    def disconnect(self):
        self.pebble = None
        self.link.close()


class CoreAppPhone(Phone):
    """A phone running CoreApp, driven by the harness."""

    def __init__(self, dut):
        raise Unsupported("CoreApp phones are not supported yet")


def make_phone(dut, phone_setup, results_dir, **options):
    """The setup's phone. ``options`` other than the defaults (another
    identity, forward PPoGATT) need a Bumble phone."""
    if phone_setup.type == PHONE_COREAPP:
        if options:
            raise Unsupported(f"{', '.join(options)} need a Bumble phone")
        return CoreAppPhone(dut)
    if phone_setup.type != PHONE_BUMBLE:
        raise HarnessError(f"no phone of type {phone_setup.type!r}")
    address = options.get("address", HOST_ADDRESS)
    keystore = os.path.join(results_dir, f"keys-{address.replace(':', '')}.json")
    return BumblePhone(dut, keystore, **options)
