# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import pytest
from harness.helpers.ui import Button, Swipe

pytestmark = [pytest.mark.ui, pytest.mark.requires_config("CONFIG_SERVICE_TOUCH")]


@pytest.fixture
def settings(ui):
    ui.press(Button.SELECT)
    ui.wait_idle()
    ui.press(Button.SELECT)
    ui.wait_idle()
    assert ui.top_window() == "Settings"
    return ui


def test_swipe_scrolls_menu(settings):
    before = settings.screenshot()
    settings.swipe(Swipe.UP)
    after = settings.wait_idle()
    assert after.tobytes() != before.tobytes()


@pytest.mark.device_types("qemu")
def test_tap_opens_item(settings):
    width, height = settings.screenshot().size
    settings.tap(width // 2, height // 2)
    settings.wait_idle()
    assert settings.top_window() != "Settings"
