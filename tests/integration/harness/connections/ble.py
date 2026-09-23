# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

from harness.connections import Capability, Connection, start_protocol
from harness.errors import HarnessError


class BleConnection(Connection):
    """Bluetooth LE through a controller Bumble drives, as the phone app
    would connect: pairing (or encrypting with the kept bond) and the Pebble
    protocol over reversed PPoGATT. The address is ``WATCH[@CONTROLLER]``:
    the watch's address, advertised name or ``auto``, and the controller's
    serial port (an hci_uart dongle) or Bumble transport, by default the
    session's (--ble-controller)."""

    scheme = "ble"
    capabilities = Capability.PROTOCOL
    opens_last = True
    help = "Bluetooth LE through an H4 controller, WATCH[@CONTROLLER]"

    def __init__(self, address, dehasher=None):
        super().__init__(address, dehasher)
        self._link = None
        self._pebble = None

    def open(self, timeout):
        from harness.ble import BleLink
        from harness.ble.transport import BleTransport

        watch, _, controller = self.address.partition("@")
        controller = controller or (self.device.ble_controller if self.device else None)
        if not controller:
            raise HarnessError(
                f"{self!r}: no controller; pass WATCH@PORT or --ble-controller"
            )
        confirm = self.device.confirm_pairing if self.device is not None else None
        self._link = BleLink(watch, controller, confirm_pairing=confirm)
        try:
            self._link.open(timeout)
            self._pebble = start_protocol(BleTransport(self._link))
        except BaseException:
            self.close()
            raise

    def close(self):
        self._pebble = None
        if self._link is not None:
            self._link.close()
            self._link = None

    @property
    def link(self):
        return self._link

    @property
    def protocol(self):
        return self._pebble
