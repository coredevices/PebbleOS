# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import struct
import time
import uuid

import pytest
from harness.helpers.prefs import set_pref
from harness.helpers.remote_input import RemoteInputAck, RemoteInputSwipe, Status
from harness.helpers.ui import Swipe

pytestmark = [
    pytest.mark.requires_config("CONFIG_SERVICE_TOUCH"),
    pytest.mark.device_types("qemu", "native"),
]

SETTINGS_UUID = uuid.UUID("07e0d9cb-8957-4bf7-9d42-35bf47caadfe")
INVALID_UUID = uuid.UUID(int=0)
SWIPE_KEYS = {
    Swipe.UP: "qlSwipeUp",
    Swipe.DOWN: "qlSwipeDown",
    Swipe.LEFT: "qlSwipeLeft",
    Swipe.RIGHT: "qlSwipeRight",
}


def _quick_launch_pref(enabled, app_uuid):
    return struct.pack("<B16s", enabled, app_uuid.bytes)


def _set_swipe_pref(dut, direction, enabled, app_uuid=INVALID_UUID):
    set_pref(dut, SWIPE_KEYS[direction], _quick_launch_pref(enabled, app_uuid))


@pytest.fixture
def home_swipe_prefs(dut):
    """Start and finish with the fresh-device defaults for these prefs."""
    # QEMU and native devices start from a fresh copy of the build's flash.
    # These are the preference defaults on that image.
    for direction in Swipe:
        _set_swipe_pref(dut, direction, False)
    set_pref(dut, "touchEnabled", b"\x01")
    set_pref(dut, "touchNavMenuEnabled", b"\x01")
    try:
        yield
    finally:
        for direction in Swipe:
            _set_swipe_pref(dut, direction, False)
        set_pref(dut, "touchEnabled", b"\x01")
        set_pref(dut, "touchNavMenuEnabled", b"\x01")


def _window_stack_after_swipe(ui, direction):
    before = ui.window_stack()
    ui.swipe(direction)
    ui.wait_idle()
    return before, ui.window_stack()


def test_unbound_swipe_does_not_change_window_stack(ui, home_swipe_prefs):
    before, after = _window_stack_after_swipe(ui, Swipe.LEFT)
    assert after == before


def test_bound_left_swipe_opens_settings(ui, dut, home_swipe_prefs):
    _set_swipe_pref(dut, Swipe.LEFT, True, SETTINGS_UUID)
    ui.swipe(Swipe.LEFT)

    deadline = time.monotonic() + 5
    while ui.top_window() != "Settings" and time.monotonic() < deadline:
        time.sleep(0.1)
    assert ui.top_window() == "Settings"


def test_swipe_does_nothing_when_touch_navigation_is_disabled(
    ui, dut, home_swipe_prefs
):
    _set_swipe_pref(dut, Swipe.LEFT, True, SETTINGS_UUID)
    set_pref(dut, "touchNavMenuEnabled", b"\x00")

    before, after = _window_stack_after_swipe(ui, Swipe.LEFT)
    assert after == before


def test_swipe_does_nothing_when_touch_is_disabled(ui, dut, home_swipe_prefs):
    _set_swipe_pref(dut, Swipe.LEFT, True, SETTINGS_UUID)
    set_pref(dut, "touchEnabled", b"\x00")

    before = ui.window_stack()
    # The master switch rejects injection before it can enqueue touch events.
    ack = dut.protocol.send_and_read(
        RemoteInputSwipe(direction=Swipe.LEFT, duration_ms=0), RemoteInputAck, timeout=5
    )
    assert ack.status == Status.INVALID
    ui.wait_idle()
    assert ui.window_stack() == before
