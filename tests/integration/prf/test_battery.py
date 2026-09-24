# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import pytest
from harness.helpers.ui import Ui

pytestmark = [pytest.mark.variants("prf"), pytest.mark.device_types("qemu")]

LOW_POWER_APP = "Low Power App"
LOW_PERCENT = 3


@pytest.fixture
def full_battery(dut, getting_started):
    getting_started()
    yield
    dut.set_battery(100)


def test_low_battery(dut, snapshot, settled_on, full_battery):
    since = dut.logs.mark()
    dut.set_battery(LOW_PERCENT)
    dut.wait_for_log(r"Battery low: enter low power mode", 30, since)
    snapshot.assert_match(settled_on(LOW_POWER_APP), "low_battery")

    since = dut.logs.mark()
    dut.set_battery(100)
    dut.wait_for_log(r"Battery good: resume normal operation", 30, since)
    assert LOW_POWER_APP not in Ui(dut).window_stack()
