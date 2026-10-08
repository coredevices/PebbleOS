# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""What the watch tells any phone over GATT: its name, and the Device
Information Service, which match what it reports over the Pebble
Protocol."""

import re

import pytest

pytestmark = [
    pytest.mark.integration_boards("qemu_emery"),
    pytest.mark.variants("normal", "prf"),
]

GAP_SERVICE = "1800"
DEVICE_NAME = "2A00"
DIS_SERVICE = "180A"
MODEL_NUMBER = "2A24"
SERIAL_NUMBER = "2A25"
FIRMWARE_REVISION = "2A26"
SOFTWARE_REVISION = "2A28"
MANUFACTURER_NAME = "2A29"


def _text(phone, service, characteristic):
    value = phone.link.read_value(service, characteristic)
    assert value is not None, f"no characteristic {characteristic}"
    return value.rstrip(b"\0").decode()


def _versions(phone):
    from libpebble2.protocol.system import WatchVersion, WatchVersionRequest

    return phone.pebble.send_and_read(
        WatchVersion(data=WatchVersionRequest()), WatchVersion, timeout=15
    ).data


def test_device_information(watch, phones):
    phone = phones().connect()
    versions = _versions(phone)
    assert _text(phone, DIS_SERVICE, FIRMWARE_REVISION) == versions.running.version_tag
    assert _text(phone, DIS_SERVICE, MODEL_NUMBER) == versions.board
    assert _text(phone, DIS_SERVICE, SERIAL_NUMBER) == versions.serial
    sdk = _text(phone, DIS_SERVICE, SOFTWARE_REVISION)
    assert re.fullmatch(r"[ \d]\d\.\d{2,3}", sdk), sdk
    assert _text(phone, DIS_SERVICE, MANUFACTURER_NAME)


def test_device_name(dut, watch, phones):
    """The default name ends with the last two bytes of the address."""
    phone = phones().connect()
    name = _text(phone, GAP_SERVICE, DEVICE_NAME)
    (mac,) = dut.prompt(dut.command("bt_mac"))
    assert name == f"Pebble {mac.replace(':', '')[-4:].upper()}"
