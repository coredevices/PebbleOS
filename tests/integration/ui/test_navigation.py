# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import pytest
from harness.helpers.ui import Button

pytestmark = pytest.mark.ui


def test_launcher_opens(ui):
    ui.press(Button.SELECT)
    ui.wait_idle()
    assert ui.top_window() == "Launcher Menu"


def test_settings(ui, snapshot):
    ui.press(Button.SELECT)
    ui.wait_idle()
    ui.press(Button.SELECT)
    image = ui.wait_idle()
    assert ui.top_window() == "Settings"
    snapshot.assert_match(image, "settings")


def test_back_returns_home(ui):
    home = ui.window_stack()
    ui.press(Button.SELECT)
    ui.press(Button.SELECT)
    ui.go_home()
    assert ui.window_stack() == home
