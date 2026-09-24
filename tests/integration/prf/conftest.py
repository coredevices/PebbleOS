# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import time

import pytest
from harness.errors import WatchTimeout
from harness.helpers.ui import Ui

GETTING_STARTED = "First Use / Recovery"
# The emulator's display follows the backlight: on for 3 s after the last
# input, then fading out.
BACKLIGHT_OFF_S = 4.0


@pytest.fixture
def settled_on(dut, build):
    """``settled_on(window)`` waits until ``window`` is on top and returns
    the screen once it no longer changes, with the backlight off."""
    ui = Ui(dut)

    def wait(window, timeout=30.0):
        deadline = time.monotonic() + timeout
        while (top := ui.top_window()) != window:
            if time.monotonic() > deadline:
                raise WatchTimeout(f"still on {top!r}, not {window!r}")
            time.sleep(0.5)
        if build.emulated:
            time.sleep(BACKLIGHT_OFF_S)
        return ui.wait_idle(timeout=max(deadline - time.monotonic(), 5.0))

    return wait


@pytest.fixture
def getting_started(settled_on):
    """``getting_started()`` waits for the Getting Started screen, e.g. once
    a pairing confirmation is gone."""
    return lambda timeout=30.0: settled_on(GETTING_STARTED, timeout)
