# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import time

import pytest
from harness.errors import WatchTimeout

pytestmark = pytest.mark.variants("prf")

OTHER_PHONE_ADDRESS = "F0:BB:1E:00:00:02"
OTHER_PHONE_NAME = "pbl-itest-2"
NAME_READ_TIMEOUT_S = 30


def _stored(dut, address, since):
    dut.wait_for_log(rf"Storing BLE pairing: addr={address}", timeout=30, since=since)


@pytest.mark.parametrize("ppogatt", ["reversed", "forward"])
def test_pairs_and_opens_session(dut, phones, ppogatt):
    since = dut.logs.mark()
    phone = phones(ppogatt=ppogatt).connect()
    assert phone.watch_version().is_recovery
    _stored(dut, phone.address, since)
    dut.wait_for_log(rf"PPoGATT Session is opened \({ppogatt},", 10, since)


def test_shows_phone_name(dut, build, phones, prompt, snapshot, getting_started):
    phone = phones().connect()
    deadline = time.monotonic() + NAME_READ_TIMEOUT_S
    while f"Device: {phone.name}" not in prompt("bt status"):
        if time.monotonic() > deadline:
            raise WatchTimeout(f"the watch did not read the name {phone.name!r}")
        time.sleep(0.5)
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
