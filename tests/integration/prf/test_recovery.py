# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import time

import numpy as np
import pytest
from harness.helpers.snapshot import Region
from harness.helpers.ui import Button, Ui

pytestmark = [pytest.mark.variants("prf"), pytest.mark.smoke]

# The watch's name under the QR code: its address is random on the emulator.
NAME_BAND_H = 28
# The backlight stays on 3 s after a press, then fades out over 0.5 s.
BACKLIGHT_OFF_S = (3.0, 4.0)
BACKLIGHT_WATCH_S = 6
BACKLIGHT_SAMPLE_S = 0.05


def test_runs_recovery_firmware(prompt):
    running = "\n".join(prompt("version")).split("Recovery FW:")[0]
    assert "recov:1" in running, running


def test_getting_started(build, snapshot, getting_started):
    image = getting_started()
    if build.emulated:
        name = Region(0, image.height - NAME_BAND_H, image.width, NAME_BAND_H)
        snapshot.assert_match(image, "getting_started", mask=[name])


def _brightness(image):
    return float(np.asarray(image.convert("L")).mean())


@pytest.mark.device_types("qemu")
def test_backlight_timeout(dut, getting_started):
    """The emulator's display follows the backlight."""
    getting_started()
    dark = _brightness(dut.screenshot())
    Ui(dut).press(Button.UP, settle=False)
    pressed = time.monotonic()
    samples = []
    while time.monotonic() - pressed < BACKLIGHT_WATCH_S:
        before = time.monotonic()
        level = _brightness(dut.screenshot())
        samples.append(((before + time.monotonic()) / 2 - pressed, level))
        # Back to back, screenshots slow the emulated watch down.
        time.sleep(BACKLIGHT_SAMPLE_S)
    lit = max(level for _, level in samples)
    assert lit > dark, "the backlight did not turn on"
    threshold = (lit + dark) / 2
    on = [t for t, level in samples if level > threshold]
    off_at = next(t for t, level in samples if t > on[0] and level <= threshold)
    low, high = BACKLIGHT_OFF_S
    assert low <= off_at <= high, f"backlight off after {off_at:.2f} s"
