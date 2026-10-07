# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import pytest
from harness.connections import Capability
from harness.helpers.pairing import SUCCESS_SHOWN_S, WatchPairing


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
    watch.wait(timeout=SUCCESS_SHOWN_S + 5)
    return phone


@pytest.fixture
def only_phone(dut, phone):
    """Skips unless ``phone`` is the only Pebble protocol session: the
    watch sends some messages to whichever session it takes for the phone,
    and the harness's own (e.g. PULSE on the emulator) can be that one."""
    if dut.has(Capability.PROTOCOL):
        pytest.skip("the harness's own protocol session competes with the phone")
    return phone
