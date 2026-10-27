# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""An iPhone's notifications on the watch, over the Apple Notification
Center Service."""

import time

import pytest
from harness.ble.apple import Action, Command, EventFlag
from harness.helpers.ui import TRANSITION_S, Button

pytestmark = pytest.mark.integration_boards("qemu_emery", "getafix@dvt2")

NOTIFICATION_WINDOW = "Notification Window"
ACTION_MENU = "Action Menu"
# Well before any earlier session ended: the watch has seen it.
PRE_EXISTING_AGE_S = 10 * 60


def _wait_modal(ui, name=NOTIFICATION_WINDOW, present=True, timeout=10.0):
    deadline = time.monotonic() + timeout
    while (name in ui.modal_stack()) != present:
        assert time.monotonic() < deadline, ui.modal_stack()
        time.sleep(0.2)


@pytest.fixture
def ancs(ui, iphone):
    """The iPhone's notifications, none of them left on the watch."""
    yield iphone.ancs
    ui.go_home()


def test_notification_shown(ui, ancs):
    """The watch fetches a new notification and the name of its app, and
    shows it."""
    since = ancs.requests.mark()
    notification = ancs.post("Running 10 min late", title="Anna")
    request = ancs.wait_fetched(notification.uid, since=since)
    assert {a for a, _ in request.attributes} >= {0, 1, 3, 5}
    app = ancs.requests.wait(
        lambda r: r.command == Command.GET_APP_ATTRIBUTES, since=since
    )
    assert app.app_id == notification.app_id
    _wait_modal(ui)


def test_pre_existing_ignored(ui, ancs):
    """Notifications the phone had before the watch's last session ended
    are not shown again right after it connects."""
    ancs.post(
        "Old news",
        title="Anna",
        flags=EventFlag.PRE_EXISTING,
        date=ancs.clock() - PRE_EXISTING_AGE_S,
    )
    time.sleep(2)
    assert NOTIFICATION_WINDOW not in ui.modal_stack()


def test_notification_removed(ui, ancs):
    """A notification cleared on the phone goes from the watch's screen."""
    notification = ancs.post("Running 10 min late", title="Anna")
    _wait_modal(ui)
    ancs.remove(notification.uid)
    _wait_modal(ui, present=False)


def test_dismissed_on_watch(ui, ancs):
    """Dismissing a notification on the watch performs its negative action
    on the phone."""
    notification = ancs.post(
        "Running 10 min late",
        title="Anna",
        positive_action="Reply",
        negative_action="Clear",
    )
    _wait_modal(ui)
    # Presses during the window's transition are dropped.
    time.sleep(TRANSITION_S)
    since = ancs.requests.mark()
    ui.press(Button.SELECT)
    _wait_modal(ui, ACTION_MENU)
    time.sleep(TRANSITION_S)
    # Dismiss, first in the menu.
    ui.press(Button.SELECT)
    request = ancs.wait_action(notification.uid, since=since)
    assert request.action == Action.NEGATIVE
