# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import pytest
from harness.helpers.ui import Button

pytestmark = pytest.mark.smoke


def test_version(prompt, build):
    response = "\n".join(prompt("version"))
    assert "Running FW:" in response
    assert "tag:v" in response


def test_logs_stream(dut):
    from libpebble2.protocol.blobdb import BlobDatabaseID
    from libpebble2.services.blobdb import BlobDBClient, SyncWrapper

    # Clearing a blob DB is logged, and changes nothing on screen.
    since = dut.logs.mark()
    SyncWrapper(BlobDBClient(dut.protocol).clear, BlobDatabaseID.Notification).wait()
    dut.wait_for_log(r"Flushing BlobDB", timeout=30, since=since)


def test_reset(dut, ui):
    ui.press(Button.SELECT)
    ui.wait_idle()
    ui.press(Button.SELECT)
    ui.wait_idle()
    assert ui.top_window() == "Settings"
    dut.reset()
    ui.wait_idle(timeout=30)
    assert "Settings" not in ui.window_stack()
