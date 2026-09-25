# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import pytest
from harness.helpers import notifications
from harness.helpers.snapshot import Region
from harness.helpers.ui import Button

pytestmark = pytest.mark.notifications

NOW = 1767261600  # 2026-01-01T10:00:00Z
# The status bar clock follows the watch's timezone and 12/24h settings.
STATUS_BAR_CLOCK_H = 20


@pytest.fixture
def clean_notifications(dut, ui):
    notifications.clear(dut)
    ui.set_time(NOW)
    yield
    notifications.clear(dut)


def test_incoming_notification(dut, ui, snapshot, clean_notifications):
    since = dut.logs.mark()
    notifications.send(dut, "Running 10 min late", sender="Anna", timestamp=NOW)
    dut.wait_for_log(r"Notification added", timeout=10, since=since)
    image = ui.wait_idle(timeout=15)
    assert ui.modal_stack(), "no notification popup"
    clock = Region(0, 0, image.width, STATUS_BAR_CLOCK_H)
    snapshot.assert_match(image, "incoming", mask=[clock])
    ui.press(Button.BACK)
    ui.wait_idle()
    assert not ui.modal_stack()
