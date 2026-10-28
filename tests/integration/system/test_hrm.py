# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import re

import pytest

QUALITY_GOOD = 3
QUALITY_OFF_WRIST = -1
READING = re.compile(r"BPM: (\d+) quality: (\d+)")


def _read(prompt):
    response = "\n".join(prompt("hrm read", timeout=60))
    match = READING.search(response)
    assert match, response
    return int(match.group(1)), int(match.group(2))


@pytest.mark.device_types("qemu")
@pytest.mark.requires_config("CONFIG_HRM_BACKEND_QEMU")
@pytest.mark.parametrize("bpm", [55, 72, 140])
def test_heart_rate_follows_pulse(dut, prompt, bpm):
    dut.set_heart_rate(bpm, QUALITY_GOOD)
    assert _read(prompt) == (bpm, QUALITY_GOOD)


@pytest.mark.device_types("qemu")
@pytest.mark.requires_config("CONFIG_HRM_BACKEND_QEMU")
def test_heart_rate_off_wrist(dut, prompt):
    dut.set_heart_rate(70, QUALITY_OFF_WRIST)
    _, quality = _read(prompt)
    assert quality == (QUALITY_OFF_WRIST & 0xFF)
