# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import pytest
from harness.connections import Capability
from harness.helpers.pairing import WatchPairing
from harness.helpers.phone import IOS


@pytest.fixture
def watch(dut, prompt):
    """The watch with no phone paired and no pairing prompt up, so that it
    takes a new pairing; left that way."""
    pairing = WatchPairing(dut)
    pairing.reset()
    yield pairing
    pairing.reset()


@pytest.fixture
def phone(watch, phones):
    """A phone paired and connected to the watch, with the pairing result
    gone from the screen."""
    phone = phones().connect()
    watch.close_success()
    return phone


@pytest.fixture
def only_phone(dut, phone):
    """Skips unless ``phone`` is the only Pebble protocol session: the
    watch sends some messages to whichever session it takes for the phone,
    and the harness's own (e.g. PULSE on the emulator) can be that one."""
    if dut.has(Capability.PROTOCOL):
        pytest.skip("the harness's own protocol session competes with the phone")
    return phone


@pytest.fixture
def iphone(watch, phones):
    """An iPhone paired and connected to the watch, which subscribed to its
    notifications (ANCS) and registered for its media player's updates
    (AMS)."""
    phone = phones(os=IOS).connect()
    watch.close_success()
    phone.ancs.wait_subscribed()
    phone.ams.wait_registered()
    return phone
