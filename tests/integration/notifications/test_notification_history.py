# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Which notifications the Notifications history keeps, on Android and iOS.

The rule is the same for both phones. A notification read or cleared on the
phone stays in the watch's history. One dismissed or acted on from the watch
is hidden. The history hides a notification when its stored status has the
Actioned bit. These tests check that each path stores the right status, read
back with ``notif list``. test_notifications_history covers the rule itself.

The harness plays the Android app over the Pebble protocol. The emulator has
no ANCS link, so iOS notifications come from ``notif ancs add``, and
``notif ancs remove`` clears one the way Notification Center does.
"""

import os
import re
import threading
import time
import uuid

import pytest
from harness.helpers import notifications
from harness.helpers.ui import Button

pytestmark = pytest.mark.notifications

NOW = 1767261600  # 2026-01-01T10:00:00Z

STATUS_READ = 0x01
STATUS_ACTIONED = 0x04
STATUS_DISMISSED = 0x10

# libpebble3 sets TimelineItem.Flag.STATE_READ (bit 12 of its u16 flags) when
# a notification leaves the shade. The high byte is the firmware's status byte.
ANDROID_READ_ON_PHONE = STATUS_DISMISSED

ACTION_RESPONSE = 0x11
ACK, NACK = 0, 1
SUBTITLE = 2

STATUS_TIMEOUT_S = 15
_LIST_LINE = re.compile(r"\{?([0-9a-fA-F-]{36})\}? status=0x([0-9a-f]{2})")
_ANCS_ADDED = re.compile(r"added as \{?([0-9a-fA-F-]{36})\}?")


def stored_status(dut, notification_id):
    for line in dut.prompt("notif list"):
        m = _LIST_LINE.search(line)
        if m and uuid.UUID(m.group(1)) == notification_id:
            return int(m.group(2), 16)
    raise AssertionError(f"{notification_id} is not in notification storage")


def wait_for_status(dut, notification_id, bits, timeout=STATUS_TIMEOUT_S):
    """The status once it has every bit in ``bits``, or the last one seen."""
    deadline = time.monotonic() + timeout
    while True:
        status = stored_status(dut, notification_id)
        if status & bits == bits or time.monotonic() > deadline:
            return status
        time.sleep(0.25)


def hidden_in_history(status):
    return bool(status & STATUS_ACTIONED)


def ancs_add(dut, ancs_uid):
    for line in dut.prompt(f"notif ancs add {ancs_uid}"):
        if m := _ANCS_ADDED.search(line):
            return uuid.UUID(m.group(1))
    raise AssertionError(f"notif ancs add {ancs_uid} gave no id")


class AndroidPhone:
    """Answers the watch's actions the way the Android app does: an
    ActionResponse per InvokeAction, after ``delay_s``. With ``echo_read``
    it then also syncs the notification as read, since the app clears it
    from the shade after dismissing it there."""

    def __init__(
        self, dut, response=ACK, delay_s=0.0, echo_read=True, conversation=False
    ):
        self.dut = dut
        self.response = response
        self.delay_s = delay_s
        self.echo_read = echo_read
        # every notification sent is one chat's messages, which the shade shows as one
        # notification. The first dismiss clears the chat and marks every message read, and
        # the app refuses a dismiss for one it no longer knows. A real phone can still
        # accept a few that arrive before the chat has gone, so refusing all the later
        # ones is the worst case.
        self.conversation = conversation
        self.bodies = {}
        self.invoked = threading.Event()
        self._answered = False
        self._lock = threading.Lock()
        # the answers run on their own threads, so one lock keeps their packets from interleaving
        self._send_lock = threading.Lock()
        self._closed = threading.Event()
        self._threads = []
        self._handle = None

    def send(self, body, sender="Anna"):
        notification_id = notifications.send(
            self.dut, body, sender=sender, timestamp=NOW
        )
        self.bodies[notification_id] = (body, sender)
        return notification_id

    def read_on_phone(self, notification_id):
        body, sender = self.bodies[notification_id]
        with self._send_lock:
            notifications.send(
                self.dut,
                body,
                sender=sender,
                timestamp=NOW,
                notification_id=notification_id,
                status=ANDROID_READ_ON_PHONE,
            )

    def __enter__(self):
        from libpebble2.protocol.timeline import TimelineActionEndpoint

        self._handle = self.dut.protocol.register_endpoint(
            TimelineActionEndpoint, self._on_action
        )
        return self

    def __exit__(self, *exc):
        self.dut.protocol.unregister_endpoint(self._handle)
        # stop pending answers, so one cannot land in the next test
        self._closed.set()
        for thread in self._threads:
            thread.join(timeout=self.delay_s + 5)

    def _on_action(self, packet):
        from libpebble2.protocol.timeline import InvokeAction

        if isinstance(packet.data, InvokeAction) and not self._closed.is_set():
            thread = threading.Thread(
                target=self._answer, args=(packet.data,), daemon=True
            )
            self._threads.append(thread)
            thread.start()

    def _answer(self, invoke):
        from libpebble2.protocol.timeline import (
            ActionResponse,
            TimelineActionEndpoint,
            TimelineAttribute,
        )

        self.invoked.set()
        if self._closed.wait(self.delay_s):
            return
        answer = self.response
        if self.conversation:
            with self._lock:
                answer = NACK if self._answered else ACK
                self._answered = True
        response = ActionResponse(
            item_id=invoke.item_id,
            response=answer,
            attributes=[TimelineAttribute(attribute_id=SUBTITLE, content=b"Dismissed")],
        )
        with self._send_lock:
            self.dut.protocol.send_packet(
                TimelineActionEndpoint(command=ACTION_RESPONSE, data=response)
            )
        if self.echo_read and answer == ACK:
            if self.conversation:
                for notification_id in self.bodies:
                    self.read_on_phone(notification_id)
            else:
                self.read_on_phone(uuid.UUID(bytes=invoke.item_id.bytes))


def wait_for_popup(ui, timeout=10):
    deadline = time.monotonic() + timeout
    while not ui.modal_stack():
        if time.monotonic() > deadline:
            raise AssertionError("no notification popup")
        time.sleep(0.2)
    ui.wait_idle()


def dismiss_from_popup(ui):
    """Open the popup's action menu and pick Dismiss, its first action."""
    ui.press(Button.SELECT)
    ui.wait_idle()
    ui.press(Button.SELECT)


def clear_modals(ui, tries=6):
    """Close popups and first-use tips left on screen."""
    for _ in range(tries):
        if not ui.modal_stack():
            return
        ui.press(Button.BACK)
        time.sleep(0.5)


@pytest.fixture
def clean(dut, ui):
    notifications.clear(dut)
    ui.set_time(NOW)
    clear_modals(ui)
    yield
    clear_modals(ui)
    notifications.clear(dut)


@pytest.fixture
def save_screen(ui, test_results_dir):
    def save(name):
        ui.wait_idle().save(os.path.join(test_results_dir, f"{name}.png"))

    return save


# Android


def test_android_read_on_phone_stays(dut, ui, clean):
    """Opening a notification on the phone syncs it as read, which must not
    take it out of the watch's history."""
    with AndroidPhone(dut) as phone:
        notification_id = phone.send("Opened on the phone")
        wait_for_popup(ui)
        clear_modals(ui)

        phone.read_on_phone(notification_id)
        status = wait_for_status(dut, notification_id, STATUS_DISMISSED)

    assert status & STATUS_DISMISSED
    assert not hidden_in_history(status)


def test_android_dismissed_on_watch_is_hidden(dut, ui, clean, save_screen):
    """Dismiss on the watch, the phone acks it and clears it from the shade."""
    with AndroidPhone(dut) as phone:
        notification_id = phone.send("Dismissed on the watch")
        wait_for_popup(ui)
        dismiss_from_popup(ui)
        status = wait_for_status(dut, notification_id, STATUS_ACTIONED)
        save_screen("after-dismiss")

    assert hidden_in_history(status)


def test_android_ack_after_window_closed_is_hidden(dut, ui, clean):
    """The phone's ack arrives after the wearer has backed out of the
    notification. Its window is gone, so the service has to mark it."""
    with AndroidPhone(dut, delay_s=5) as phone:
        notification_id = phone.send("Acked late")
        wait_for_popup(ui)
        dismiss_from_popup(ui)
        assert phone.invoked.wait(10), "the watch never sent the action"
        clear_modals(ui)
        assert not ui.modal_stack(), "the notification window is still open"
        status = wait_for_status(dut, notification_id, STATUS_ACTIONED)

    assert hidden_in_history(status)


def test_android_dismiss_all_of_a_chat_is_hidden(dut, ui, clean, save_screen):
    """Dismiss All on three messages from one chat. The phone takes the first dismiss,
    clears the chat from the shade, and refuses the other two. Dismiss All still shows
    success, so all three have to leave the history."""
    with AndroidPhone(dut, conversation=True) as phone:
        notification_ids = [phone.send(f"Message {n}", sender="Bob") for n in (1, 2, 3)]
        wait_for_popup(ui)
        ui.press(Button.SELECT)
        ui.wait_idle()
        save_screen("action-menu")
        # the Android menu is Dismiss, Dismiss All
        ui.press(Button.DOWN)
        ui.press(Button.SELECT)
        statuses = [wait_for_status(dut, n, STATUS_ACTIONED) for n in notification_ids]

    assert all(hidden_in_history(status) for status in statuses)


def test_android_failed_dismiss_stays(dut, ui, clean):
    """A dismiss the phone refuses leaves the notification in the history."""
    with AndroidPhone(dut, response=NACK) as phone:
        notification_id = phone.send("Refused by the phone")
        wait_for_popup(ui)
        dismiss_from_popup(ui)
        assert phone.invoked.wait(10), "the watch never sent the action"
        time.sleep(3)
        status = stored_status(dut, notification_id)

    assert not hidden_in_history(status)


# iOS


def test_ios_cleared_from_notification_center_stays(dut, ui, clean):
    """Clearing a notification from Notification Center marks it Dismissed,
    which must not take it out of the watch's history."""
    notification_id = ancs_add(dut, 7)
    wait_for_popup(ui)
    clear_modals(ui)

    dut.prompt("notif ancs remove 7")
    status = wait_for_status(dut, notification_id, STATUS_DISMISSED)

    assert status & STATUS_DISMISSED
    assert not hidden_in_history(status)


def test_ios_dismissed_on_watch_is_hidden(dut, ui, clean):
    notification_id = ancs_add(dut, 8)
    wait_for_popup(ui)
    dismiss_from_popup(ui)
    status = wait_for_status(dut, notification_id, STATUS_ACTIONED)

    assert hidden_in_history(status)


def test_ios_dismiss_all_is_hidden(dut, ui, clean, save_screen):
    """Dismiss All sends the second and later iOS dismissals in bulk mode,
    which posts no action result."""
    first = ancs_add(dut, 9)
    second = ancs_add(dut, 10)
    wait_for_popup(ui)
    ui.press(Button.SELECT)
    ui.wait_idle()
    save_screen("action-menu")
    # the iOS menu is Dismiss, Mute, Dismiss All
    ui.press(Button.DOWN, presses=2)
    ui.press(Button.SELECT)

    first_status = wait_for_status(dut, first, STATUS_ACTIONED)
    second_status = wait_for_status(dut, second, STATUS_ACTIONED)

    assert hidden_in_history(first_status)
    assert hidden_in_history(second_status)
