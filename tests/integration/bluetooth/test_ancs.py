# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""An iPhone's notifications on the watch, over the Apple Notification
Center Service."""

import time
import uuid

import pytest
from harness.ble.apple import Action, Category, Command, EventFlag
from harness.helpers.blobdb import BlobDB, Database, Status
from harness.helpers.ui import TRANSITION_S, Button

pytestmark = pytest.mark.integration_boards("qemu_emery", "getafix@dvt2")

NOTIFICATION_WINDOW = "Notification Window"
ACTION_MENU = "Action Menu"
NOTIFICATIONS = "Notifications"
NOTIFICATIONS_UUID = uuid.UUID("b2cae818-10f8-46df-ad2b-98ad2254a3c1")
# Long enough for a popup the watch would show.
POPUP_WAIT_S = 2
RESPECT_PHONE_SILENCE = b"notifRespectPhoneSilence"
# Well before any earlier session ended: the watch has seen it.
PRE_EXISTING_AGE_S = 10 * 60


def _wait_modal(ui, name=NOTIFICATION_WINDOW, present=True, timeout=10.0):
    deadline = time.monotonic() + timeout
    while (name in ui.modal_stack()) != present:
        assert time.monotonic() < deadline, ui.modal_stack()
        time.sleep(0.2)


def _wait_window(ui, name, timeout=10.0):
    deadline = time.monotonic() + timeout
    while name not in ui.window_stack():
        assert time.monotonic() < deadline, ui.window_stack()
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


def test_missed_notification_recovered(ui, iphone, ancs):
    """A notification posted while the watch was away, which the phone
    sends as pre-existing once the watch is back, is shown."""
    iphone.disconnect()
    iphone.connect()
    ancs.wait_subscribed()
    since = ancs.requests.mark()
    notification = ancs.post(
        "Running 10 min late", title="Anna", flags=EventFlag.PRE_EXISTING
    )
    ancs.wait_fetched(notification.uid, since=since)
    _wait_modal(ui)


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


def test_missed_call_without_actions(ui, ancs):
    """A missed call the phone sends without actions is shown, and its
    timeline pin added."""
    ancs.post(
        "Missed Call",
        title="Anna",
        app_id="com.apple.mobilephone",
        category=Category.MISSED_CALL,
    )
    _wait_modal(ui)


@pytest.fixture
def respect_phone_silence(iphone):
    """The watch set to deliver notifications silenced on the phone quietly."""
    blobdb = BlobDB(iphone)
    status = blobdb.insert(Database.SETTINGS, RESPECT_PHONE_SILENCE, b"\x01")
    assert status == Status.SUCCESS
    yield
    blobdb.insert(Database.SETTINGS, RESPECT_PHONE_SILENCE, b"\x00")


def test_silenced_notification_alerts_by_default(ui, ancs):
    """Without the preference, a notification the phone silenced pops up."""
    ancs.post("Running 10 min late", title="Anna", flags=EventFlag.SILENT)
    _wait_modal(ui)


def test_silenced_notification_listed_quietly(ui, iphone, ancs, respect_phone_silence):
    """A notification the phone silenced, e.g. for a Focus, does not pop up
    but is in the notifications list."""
    assert BlobDB(iphone).clear(Database.NOTIFICATIONS) == Status.SUCCESS
    since = ancs.requests.mark()
    notification = ancs.post(
        "Running 10 min late", title="Anna", flags=EventFlag.SILENT
    )
    ancs.wait_fetched(notification.uid, since=since)
    time.sleep(POPUP_WAIT_S)
    assert NOTIFICATION_WINDOW not in ui.modal_stack()

    ui.launch_app(NOTIFICATIONS_UUID)
    _wait_window(ui, NOTIFICATIONS)
    time.sleep(TRANSITION_S)
    # The newest notification is selected.
    ui.press(Button.SELECT)
    _wait_window(ui, NOTIFICATION_WINDOW)


def test_silenced_missed_call_quiet(ui, ancs, respect_phone_silence):
    """A call a Focus held back reaches the watch as a silenced missed call,
    which does not pop up."""
    since = ancs.requests.mark()
    notification = ancs.post(
        "Missed Call",
        title="Anna",
        app_id="com.apple.mobilephone",
        category=Category.MISSED_CALL,
        flags=EventFlag.SILENT,
        positive_action="Call Back",
        negative_action="Clear",
    )
    ancs.wait_fetched(notification.uid, since=since)
    time.sleep(POPUP_WAIT_S)
    assert NOTIFICATION_WINDOW not in ui.modal_stack()
