# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import warnings

import pytest

# A sporadic burst of background flash work can push a single measurement out
# of range; a real regression fails the rerun too.
pytestmark = [
    pytest.mark.device_types("hardware"),
    pytest.mark.flaky(reruns=1, only_rerun=["AssertionError"]),
]

# Mean current on TicToc (uA), per board, at NOMINAL_VOLTAGE_MV, over five
# runs; a measurement fails outside IDLE_TOLERANCE of it.
NOMINAL_VOLTAGE_MV = 3800
IDLE_NOMINAL_UA = {
    "obelix": {
        "airplane_mode": 89,
    },
    "getafix": {
        "airplane_mode": 86,
    },
    "asterix": {
        "airplane_mode": 64,
    },
}
IDLE_TOLERANCE = 0.10


def _check(build, power, record_property, measurement):
    for key, value in measurement.summary().items():
        record_property(key, value)
    print(measurement)

    nominal = IDLE_NOMINAL_UA.get(build.board, {}).get(measurement.name)
    if nominal is None or power.supply.voltage_mv != NOMINAL_VOLTAGE_MV:
        warnings.warn(
            f"no {measurement.name} limit for {build.board} at {power.supply.voltage_mv} mV",
            stacklevel=2,
        )
        return
    record_property("nominal_ua", nominal)
    low, high = nominal * (1 - IDLE_TOLERANCE), nominal * (1 + IDLE_TOLERANCE)
    assert low <= measurement.mean_ua <= high, (
        f"{measurement.name}: {measurement.mean_ua:.1f} uA, expected {nominal} uA "
        f"+-{IDLE_TOLERANCE:.0%} ({low:.0f}-{high:.0f} uA)"
    )


def test_airplane_mode(dut, build, ui, power, record_property):
    # A wiped watch starts with airplane mode off.
    dut.wipe()
    ui.toggle_airplane_mode()
    try:
        ui.go_home()
        m = power.measure_idle("airplane_mode")
    finally:
        ui.toggle_airplane_mode()
    _check(build, power, record_property, m)
