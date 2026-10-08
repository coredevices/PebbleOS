# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""What the watch advertises, as a phone scanning for it sees it: an
unpaired watch is discoverable, a bonded one only lets its phone reconnect,
a connected one is silent, and airplane mode silences it."""

import dataclasses
import re
import statistics
import struct
import time

import pytest
from harness.ble import PAIRING_SERVICE
from harness.ble.scanner import Scanner
from harness.errors import WatchTimeout
from harness.helpers.ui import TRANSITION_S, Button

pytestmark = pytest.mark.integration_boards("qemu_emery", "getafix@dvt2")

VENDOR_ID = 0x0EEA
FLAGS = 0x06
FLAG_RECOVERY = 0x01
FLAG_FIRST_USE = 0x02
MANUFACTURER_DATA = struct.Struct("<B12sBB3BB")
# The most a Bluetooth LE transmitter may put out, in dBm.
TX_POWER_RANGE = range(-127, 21)
FAST_INTERVAL_S = 0.25
SCAN_S = 3.0
SETTLE_S = 2.0
ADVERT_TIMEOUT_S = 20.0
# Bluetooth is the first category; each opens in a window of this name.
SETTINGS_CATEGORY = "Settings Window"


@dataclasses.dataclass
class PebbleData:
    """The manufacturer specific data of a discoverable watch."""

    payload_type: int
    serial: str
    hw_platform: int
    color: int
    version: tuple
    flags: int

    @classmethod
    def parse(cls, data):
        payload_type, serial, hw_platform, color, *version, flags = (
            MANUFACTURER_DATA.unpack(data)
        )
        return cls(
            payload_type,
            serial.rstrip(b"\0").decode(),
            hw_platform,
            color,
            tuple(version),
            flags,
        )


def local_address(dut):
    """The watch's address, as ``bt mac`` prints it."""
    (line,) = dut.prompt(dut.command("bt_mac"))
    octets = line.strip().removeprefix("0x")
    return ":".join(octets[i : i + 2] for i in range(0, 12, 2)).upper()


def scan(dut, duration=SCAN_S, link=None):
    """The watch's advertising heard within ``duration``, or None."""
    scanner = Scanner(link=link) if link else Scanner(dut.ble_controller)
    return scanner.scan(duration).get(local_address(dut))


def wait_advert(dut, predicate, timeout=ADVERT_TIMEOUT_S, link=None):
    """The watch's advertising once ``predicate`` holds for all of its data
    heard in a scan."""
    deadline = time.monotonic() + timeout
    while True:
        advert = scan(dut, link=link)
        if advert is not None and all(predicate(d) for d in _all_data(advert)):
            return advert
        if time.monotonic() > deadline:
            data = advert.data if advert else "nothing"
            raise WatchTimeout(f"the watch advertises {data} after {timeout}s")


def discoverable(data):
    from bumble.core import UUID

    return UUID(PAIRING_SERVICE) in uuids(data)


def reconnectable(data):
    return not data.ad_structures


def uuids(data):
    from bumble.core import AdvertisingData

    found = data.get(AdvertisingData.COMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS)
    return list(found or [])


def field(data, ad_type):
    """An AD structure's raw value, or None."""
    for found, value in data.ad_structures:
        if found == ad_type:
            return bytes(value)
    return None


def pebble_data(advert):
    from bumble.core import AdvertisingData

    assert advert.scan_response is not None, "the watch sent no scan response"
    value = field(advert.scan_response, AdvertisingData.MANUFACTURER_SPECIFIC_DATA)
    assert value is not None, "no manufacturer data in the scan response"
    (company,) = struct.unpack_from("<H", value)
    assert company == VENDOR_ID
    assert len(value) - 2 == MANUFACTURER_DATA.size
    return PebbleData.parse(value[2:])


def assert_address(dut, advert):
    """Advertised from the identity address: no privacy, so a random one
    is static (its two top bits set)."""
    assert advert.address == local_address(dut)
    if advert.address_type == "random":
        assert int(advert.address[:2], 16) >> 6 == 0b11, "not a static address"


def wait_window(ui, name, timeout=10.0):
    deadline = time.monotonic() + timeout
    while ui.top_window() != name:
        if time.monotonic() > deadline:
            raise WatchTimeout(f"{name!r} is not on top: {ui.window_stack()}")
        time.sleep(0.2)
    time.sleep(TRANSITION_S)


def bonded(phones, watch):
    """A phone paired, then gone."""
    phone = phones().connect()
    watch.close_success()
    phone.disconnect()
    return phone


def watch_info(phone):
    """The watch's version response and color, as the phone reads them."""
    from libpebble2.protocol.system import (
        ModelRequest,
        WatchModel,
        WatchVersion,
        WatchVersionRequest,
    )

    version = phone.pebble.send_and_read(
        WatchVersion(data=WatchVersionRequest()), WatchVersion, timeout=15
    ).data
    model = phone.pebble.send_and_read(
        WatchModel(data=ModelRequest()), WatchModel, timeout=15
    ).data
    (color,) = struct.unpack(">I", bytes(model.data))
    return version, color


@pytest.mark.variants("normal", "prf")
def test_discoverable_when_unpaired(dut, build, watch):
    from bumble.core import AdvertisingData

    advert = wait_advert(dut, discoverable)
    data = advert.data
    assert advert.connectable
    assert_address(dut, advert)
    assert data.get(AdvertisingData.FLAGS) == FLAGS
    suffix = local_address(dut).replace(":", "")[-4:]
    assert data.get(AdvertisingData.COMPLETE_LOCAL_NAME) == f"Pebble {suffix}"
    (tx_power,) = struct.unpack("b", field(data, AdvertisingData.TX_POWER_LEVEL))
    assert tx_power in TX_POWER_RANGE

    pebble = pebble_data(advert)
    assert pebble.payload_type == 0
    assert bool(pebble.flags & FLAG_RECOVERY) == (build.variant == "prf")
    assert not pebble.flags & FLAG_FIRST_USE


@pytest.mark.variants("normal", "prf")
def test_manufacturer_data_describes_watch(dut, watch, phones):
    pebble = pebble_data(wait_advert(dut, discoverable))

    phone = phones().connect()
    version, color = watch_info(phone)
    assert pebble.serial == version.serial
    assert pebble.hw_platform == version.running.hardware_platform
    assert pebble.color == color
    tag = re.match(r"v(\d+)\.(\d+)(?:\.(\d+))?", version.running.version_tag)
    assert tag, f"no version in {version.running.version_tag!r}"
    assert pebble.version == tuple(int(part or 0) for part in tag.groups())


def test_reconnection_advert_when_bonded(dut, watch, phones):
    phone = bonded(phones, watch)

    advert = wait_advert(dut, reconnectable)
    assert advert.connectable
    assert_address(dut, advert)
    assert not advert.scan_response or not advert.scan_response.ad_structures

    phone.connect()
    assert watch.prompt() is None
    assert phone.watch_version() is not None


@pytest.mark.variants("prf")
def test_recovery_stays_discoverable_when_bonded(dut, watch, phones):
    bonded(phones, watch)
    time.sleep(SETTLE_S)

    advert = wait_advert(dut, discoverable)
    assert pebble_data(advert).flags & FLAG_RECOVERY


@pytest.mark.variants("normal", "prf")
def test_silent_while_connected(dut, build, watch, phones):
    phone = phones().connect()
    watch.close_success()

    assert scan(dut, link=phone.link) is None, "advertising while connected"

    # The same scanner hears the watch once the link drops.
    phone.link.drop()
    expected = discoverable if build.variant == "prf" else reconnectable
    wait_advert(dut, expected, link=phone.link)


def test_discoverable_again_once_unpaired(dut, watch, phones):
    bonded(phones, watch)
    wait_advert(dut, reconnectable)

    watch.unpair()
    advert = wait_advert(dut, discoverable)
    assert pebble_data(advert).flags == 0


def test_settings_keep_bonded_watch_hidden(dut, ui, watch, phones):
    bonded(phones, watch)
    wait_advert(dut, reconnectable)
    for window in ("Launcher Menu", "Settings", SETTINGS_CATEGORY):
        ui.press(Button.SELECT)
        wait_window(ui, window)
    try:
        time.sleep(SETTLE_S)
        advert = scan(dut)
        assert advert is not None, "not advertising"
        assert not any(discoverable(r) for r in _all_data(advert)), (
            "discoverable with a phone bonded"
        )
    finally:
        ui.go_home()


@pytest.mark.variants("normal", "prf")
def test_airplane_mode_silences(dut, watch):
    wait_advert(dut, discoverable)

    dut.prompt(dut.command("bt_airplane", mode="on"))
    try:
        time.sleep(SETTLE_S)
        assert scan(dut) is None, "advertising in airplane mode"
    finally:
        dut.prompt(dut.command("bt_airplane", mode="off"))

    advert = wait_advert(dut, discoverable)
    # Discovery starts over at the fast interval.
    assert statistics.median(advert.intervals()) < FAST_INTERVAL_S


def test_quick_launch_airplane_mode_silences(dut, ui, watch):
    wait_advert(dut, discoverable)

    ui.toggle_airplane_mode()
    try:
        time.sleep(SETTLE_S)
        assert scan(dut) is None, "advertising in airplane mode"
    finally:
        ui.toggle_airplane_mode()

    wait_advert(dut, discoverable)


def _all_data(advert):
    from bumble.core import AdvertisingData

    return [AdvertisingData.from_bytes(r.data) for r in advert.adverts]
