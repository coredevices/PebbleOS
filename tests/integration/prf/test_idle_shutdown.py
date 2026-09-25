# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import pytest
from harness.errors import WatchTimeout

pytestmark = [pytest.mark.variants("prf"), pytest.mark.slow]

# PRF turns off after 10 minutes unplugged with no phone connected
# (services/idle_watchdog), checked on a multi-minute timer.
IDLE_S = 10 * 60
CHECK_MARGIN_S = 2 * 60
CHARGING_PERCENT = 50
STANDBY = r"Preparing to enter standby mode \(reason 12\)"


@pytest.fixture
def restart_after(dut):
    yield
    dut.reset()


def test_turns_off_when_idle(dut, restart_after):
    since = dut.logs.mark()
    dut.reset()
    dut.wait_for_log(STANDBY, IDLE_S + CHECK_MARGIN_S, since)


def test_stays_on_with_phone(dut, phones, restart_after):
    since = dut.logs.mark()
    dut.reset()
    phone = phones().connect()
    with pytest.raises(WatchTimeout):
        dut.wait_for_log(STANDBY, IDLE_S + CHECK_MARGIN_S, since)
    assert phone.watch_version().is_recovery


def test_stays_on_while_charging(dut, restart_after):
    since = dut.logs.mark()
    dut.reset()
    dut.set_battery(CHARGING_PERCENT, charging=True)
    try:
        with pytest.raises(WatchTimeout):
            dut.wait_for_log(STANDBY, IDLE_S + CHECK_MARGIN_S, since)
    finally:
        dut.set_battery(100)
