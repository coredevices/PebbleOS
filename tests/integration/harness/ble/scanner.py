# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""What a phone scanning for watches sees: the advertising reports of each
advertiser, collected for a while from a Bumble controller."""

import asyncio
import dataclasses
import itertools
import threading
import time

from harness.ble import controller_transport
from harness.errors import HarnessError

SCANNER_ADDRESS = "F0:BB:1E:00:00:5C"


@dataclasses.dataclass
class Report:
    """One advertising report: the advertising PDU's data, or the scan
    response's."""

    time: float
    connectable: bool
    scan_response: bool
    data: bytes


@dataclasses.dataclass
class Advertiser:
    """An advertiser's reports, in the order they came."""

    address: str
    #: ``public`` or ``random``.
    address_type: str
    reports: list = dataclasses.field(default_factory=list)

    def _last(self, scan_response):
        for report in reversed(self.reports):
            if report.scan_response == scan_response:
                return report
        return None

    @property
    def adverts(self):
        return [r for r in self.reports if not r.scan_response]

    @property
    def connectable(self):
        return any(r.connectable for r in self.adverts)

    @property
    def data(self):
        """The last advertising data, as Bumble ``AdvertisingData``."""
        from bumble.core import AdvertisingData

        report = self._last(False)
        return AdvertisingData.from_bytes(report.data if report else b"")

    @property
    def scan_response(self):
        """The last scan response's data, or None without one."""
        from bumble.core import AdvertisingData

        report = self._last(True)
        return AdvertisingData.from_bytes(report.data) if report else None

    def intervals(self):
        """The time between consecutive advertising reports, in seconds."""
        times = [r.time for r in self.adverts]
        return [b - a for a, b in itertools.pairwise(times)]


def address_type_name(address):
    from bumble.hci import Address

    if address.address_type in (
        Address.PUBLIC_DEVICE_ADDRESS,
        Address.PUBLIC_IDENTITY_ADDRESS,
    ):
        return "public"
    return "random"


class Scanner:
    """Active scanning from a controller of its own (``controller``), or from
    the device of an open :class:`~harness.ble.BleLink` (``link``), e.g.
    while it is connected to the watch."""

    def __init__(self, controller=None, link=None, address=SCANNER_ADDRESS):
        if (controller is None) == (link is None):
            raise HarnessError("scan from a controller or from a link")
        self.controller = controller and controller_transport(controller)
        self.link = link
        self.address = address

    def scan(self, duration, active=True):
        """Advertisers heard within ``duration`` seconds, by address."""
        if self.link is not None:
            future = asyncio.run_coroutine_threadsafe(
                self._scan(self.link._device, duration, active), self.link._loop
            )
            return future.result(duration + 15)
        result = {}
        error = []

        def run():
            try:
                result.update(asyncio.run(self._scan_own(duration, active)))
            except BaseException as e:  # noqa: BLE001
                error.append(e)

        thread = threading.Thread(target=run, daemon=True)
        thread.start()
        thread.join(duration + 30)
        if thread.is_alive():
            raise HarnessError("the scan did not end")
        if error:
            raise error[0]
        return result

    async def _scan_own(self, duration, active):
        from bumble.device import Device, DeviceConfiguration
        from bumble.hci import Address
        from bumble.transport import open_transport

        async with await open_transport(self.controller) as transport:
            device = Device.from_config_with_hci(
                DeviceConfiguration(
                    name="pbl-itest-scanner", address=Address(self.address)
                ),
                transport.source,
                transport.sink,
            )
            await device.power_on()
            return await self._scan(device, duration, active)

    @staticmethod
    async def _scan(device, duration, active):
        from bumble.device import Advertisement

        advertisers = {}

        def on_report(report):
            adv = Advertisement.from_advertising_report(report)
            if adv is None:
                return
            key = str(adv.address).split("/")[0].upper()
            advertiser = advertisers.setdefault(
                key, Advertiser(key, address_type_name(adv.address))
            )
            advertiser.reports.append(
                Report(
                    time.monotonic(),
                    adv.is_connectable,
                    adv.is_scan_response,
                    bytes(adv.data_bytes),
                )
            )

        device.host.on("advertising_report", on_report)
        await device.start_scanning(active=active, filter_duplicates=False)
        try:
            await asyncio.sleep(duration)
        finally:
            await device.stop_scanning()
            device.host.remove_listener("advertising_report", on_report)
        return advertisers
