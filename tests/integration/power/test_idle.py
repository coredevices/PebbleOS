# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import time
import warnings

import pytest

pytestmark = [pytest.mark.power, pytest.mark.slow, pytest.mark.device_types("hardware")]

# How long a watch with no bonding advertises fast (20 ms) for discovery,
# before slowing down to 1022.5 ms (comm/ble/gap_le_slave_discovery.c).
FAST_DISCOVERY_S = 5 * 60
SLOW_MARGIN_S = 30

# Mean current on TicToc (uA), per board, at NOMINAL_VOLTAGE_MV, over five
# runs; a measurement fails outside IDLE_TOLERANCE of it.
NOMINAL_VOLTAGE_MV = 3800
IDLE_NOMINAL_UA = {
    "obelix": {
        "advertising_fast": 300,
        "advertising_slow": 152,
        "airplane_mode": 146,
    },
    "getafix": {
        "advertising_fast": 893,
        "advertising_slow": 141,
        "airplane_mode": 125,
    },
}
IDLE_TOLERANCE = 0.10

_measured = {}


def _check(build, power, record_property, measurement):
    _measured[measurement.name] = measurement
    for key, value in measurement.summary().items():
        record_property(key, value)
    print(measurement)

    nominal = IDLE_NOMINAL_UA.get(build.board, {}).get(measurement.name)
    if nominal is None or power.ppk2.voltage_mv != NOMINAL_VOLTAGE_MV:
        warnings.warn(
            f"no {measurement.name} limit for {build.board} at {power.ppk2.voltage_mv} mV",
            stacklevel=2,
        )
        return
    record_property("nominal_ua", nominal)
    low, high = nominal * (1 - IDLE_TOLERANCE), nominal * (1 + IDLE_TOLERANCE)
    assert low <= measurement.mean_ua <= high, (
        f"{measurement.name}: {measurement.mean_ua:.1f} uA, expected {nominal} uA "
        f"+-{IDLE_TOLERANCE:.0%} ({low:.0f}-{high:.0f} uA)"
    )


def _check_below(measurement, other):
    """Scenarios that differ by less than the tolerance still order."""
    if other is not None:
        assert measurement.mean_ua < other.mean_ua, (
            f"{measurement.name} ({measurement.mean_ua:.1f} uA) should draw less than "
            f"{other.name} ({other.mean_ua:.1f} uA)"
        )


@pytest.fixture(scope="module")
def fresh_boot(dut):
    """A wiped watch, unpaired and so advertising for discovery; when it
    booted."""
    dut.wipe()
    return time.monotonic()


def test_advertising_fast(fresh_boot, build, ui, power, record_property):
    m = power.measure_idle("advertising_fast")
    assert time.monotonic() - fresh_boot < FAST_DISCOVERY_S, (
        "measured past fast advertising"
    )
    _check(build, power, record_property, m)


def test_advertising_slow(fresh_boot, build, ui, power, record_property):
    time.sleep(max(fresh_boot + FAST_DISCOVERY_S + SLOW_MARGIN_S - time.monotonic(), 0))
    ui.go_home()
    m = power.measure_idle("advertising_slow")
    _check_below(m, _measured.get("advertising_fast"))
    _check(build, power, record_property, m)


def test_airplane_mode(fresh_boot, build, ui, power, record_property):
    # A wiped watch starts with airplane mode off.
    ui.toggle_airplane_mode()
    try:
        ui.go_home()
        m = power.measure_idle("airplane_mode")
    finally:
        ui.toggle_airplane_mode()
    _check_below(m, _measured.get("advertising_slow"))
    _check(build, power, record_property, m)
