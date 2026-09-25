# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import time

import numpy as np
import pytest
from harness.helpers.power import SAMPLE_RATE_HZ
from harness.helpers.ui import Ui

pytestmark = [
    pytest.mark.variants("prf"),
    pytest.mark.power,
    pytest.mark.device_types("hardware"),
]

# Limits from the PRF release checklist, at NOMINAL_VOLTAGE_MV.
NOMINAL_VOLTAGE_MV = 3800
ADVERTISING_MAX_UA = 1000
CONNECTED_MAX_UA = 550
OFF_MAX_UA = 15
MEASURE_S = 30
OFF_SETTLE_S = 5

# The backlight stays on 3 s after a press, then fades out over 0.5 s.
BACKLIGHT_ON_S = (3.0, 4.0)
BACKLIGHT_IDLE_S = 6
BACKLIGHT_PRESS_AT_S = 1
BACKLIGHT_MEASURE_S = 7
SMOOTHING_S = 0.01

# Booting at this VBAT, the fuel gauge estimates under 5%: PRF's low battery.
LOW_BATTERY_MV = 3500


@pytest.fixture(autouse=True)
def nominal_voltage(power):
    if power.ppk2.voltage_mv != NOMINAL_VOLTAGE_MV:
        pytest.skip(f"limits are for {NOMINAL_VOLTAGE_MV} mV")


def _record(record_property, measurement, limit_ua):
    for key, value in measurement.summary().items():
        record_property(key, value)
    record_property("limit_ua", limit_ua)
    print(measurement)


def test_advertising(dut, power, record_property):
    dut.reset()
    m = power.measure_idle("advertising", seconds=MEASURE_S)
    _record(record_property, m, ADVERTISING_MAX_UA)
    assert m.mean_ua <= ADVERTISING_MAX_UA, m


def test_connected(dut, phones, power, record_property):
    dut.reset()
    phone = phones().connect()
    m = power.measure_idle("connected", seconds=MEASURE_S)
    assert phone.watch_version().is_recovery
    _record(record_property, m, CONNECTED_MAX_UA)
    assert m.mean_ua <= CONNECTED_MAX_UA, m


def test_off(dut, power, record_property):
    dut.standby()
    try:
        time.sleep(OFF_SETTLE_S)
        m = power.measure_for(MEASURE_S, "off")
    finally:
        dut.reset()
    _record(record_property, m, OFF_MAX_UA)
    assert m.mean_ua <= OFF_MAX_UA, m


def _on_time_s(samples):
    """How long the current stays in its upper half: the backlight's."""
    window = int(SMOOTHING_S * SAMPLE_RATE_HZ)
    smooth = np.convolve(samples, np.ones(window) / window, mode="valid")
    low, high = np.percentile(smooth, [10, 90])
    above = np.flatnonzero(smooth > (low + high) / 2)
    if len(above) == 0:
        return 0.0
    return (above[-1] - above[0]) / SAMPLE_RATE_HZ


def test_backlight_timeout(dut, power, record_property):
    time.sleep(BACKLIGHT_IDLE_S)
    with power.measure("backlight") as m:
        time.sleep(BACKLIGHT_PRESS_AT_S)
        dut.prompt("click short 1")
        time.sleep(BACKLIGHT_MEASURE_S - BACKLIGHT_PRESS_AT_S)
    on_s = _on_time_s(m.samples)
    record_property("backlight_on_s", on_s)
    low, high = BACKLIGHT_ON_S
    assert low <= on_s <= high, f"backlight on for {on_s:.2f} s"


def test_low_battery(dut, power):
    power.ppk2.set_voltage(LOW_BATTERY_MV)
    try:
        since = dut.logs.mark()
        dut.reset()
        dut.wait_for_log(r"Battery low: enter low power mode", 60, since)
        assert "Low Power App" in Ui(dut).window_stack()
    finally:
        power.ppk2.set_voltage(NOMINAL_VOLTAGE_MV)
        dut.reset()
