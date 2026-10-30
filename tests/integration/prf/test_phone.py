# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import pytest
from harness.helpers.pairing import WatchPairing

pytestmark = pytest.mark.variants("prf")

OTHER_PHONE_ADDRESS = "F0:BB:1E:00:00:02"
OTHER_PHONE_NAME = "pbl-itest-2"


def _stored(dut, address, since):
    dut.wait_for_log(rf"Bonded with {address}", timeout=30, since=since)


def test_getting_started_shows_phone_name(
    dut, build, phones, snapshot, getting_started
):
    phone = phones().connect()
    WatchPairing(dut).wait_phone_name(phone.name)
    image = getting_started()
    if build.emulated:
        snapshot.assert_match(image, "phone_name")


def test_new_phone_replaces_bond(dut, phones):
    """PRF keeps a single bond: another phone pairs over it, and the first
    one still connects."""
    first = phones().connect()
    first.disconnect()

    since = dut.logs.mark()
    other = phones(address=OTHER_PHONE_ADDRESS, name=OTHER_PHONE_NAME).connect()
    assert other.watch_version().is_recovery
    _stored(dut, OTHER_PHONE_ADDRESS, since)
    other.disconnect()

    first = phones().connect()
    assert first.watch_version().is_recovery
