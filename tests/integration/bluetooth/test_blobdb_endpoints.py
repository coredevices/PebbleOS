# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Blob DB over the phone connection: what the phone app stores on the
watch (notifications, pins and reminders, apps, glances) and the sync of
what the watch changes back to the phone."""

import struct
import threading
import time
import uuid

import pytest
from harness.errors import WatchTimeout
from harness.helpers.blobdb import (
    ActionType,
    Attribute,
    BlobDB,
    Command,
    Database,
    ItemType,
    Layout,
    Status,
    attribute,
    timeline_item,
)
from harness.helpers.ui import Button, Ui

pytestmark = [pytest.mark.bluetooth, pytest.mark.integration_boards("qemu_emery")]

TIMELINE_ACTION_ENDPOINT = 0x2CB0
BLOBDB2_ENDPOINT = 0xB2DB

NOTIFICATION_WINDOW = "Notification Window"
ACTION_MENU = "Action Menu"
ACTION_PROGRESS = "Progress Window"
ACTION_RESULT = "Action Result"
# How long a new notification's peek animation takes to finish.
PEEK_S = 3.0
INVOKE_ACTION = 0x02
PHONE_RESPONSE = 0x11
RESPONSE_ACK = 0x00
DISPLAYED_ITEM = 0x04
SETTINGS_UUID = uuid.UUID("07e0d9cb-8957-4bf7-9d42-35bf47caadfe")

DIRTY_DBS = 0x06
START_SYNC = 0x07
WRITE = 0x08
WRITEBACK = 0x09
SYNC_DONE = 0x0A
VERSION = 0x0B
DIRTY_ALL = 0x0C
RESPONSE = 0x80
# The watch resends an unanswered write after this long.
SYNC_RETRY_S = 30

GLANCE_VERSION = 1
GLANCE_EXPIRATION = 37
GLANCE_SUBTITLE = 47


def _notification(item_id, timestamp, body, actions=()):
    return timeline_item(
        item_id,
        ItemType.NOTIFICATION,
        Layout.NOTIFICATION,
        timestamp,
        [(Attribute.TITLE, "Itest"), (Attribute.BODY, body)],
        actions=actions,
    )


def _wait_modal(ui, window, present=True, timeout=15.0):
    deadline = time.monotonic() + timeout
    while (window in ui.modal_stack()) != present:
        if time.monotonic() > deadline:
            state = "up" if present else "gone"
            raise AssertionError(f"{window} is not {state}: {ui.modal_stack()}")
        time.sleep(0.2)


@pytest.fixture
def blobdb(phone):
    db = BlobDB(phone)
    yield db
    db.clear(Database.NOTIFICATIONS)


def test_notification(dut, phone, blobdb):
    """A notification pops up and the watch tells the phone it is shown."""
    ui = Ui(dut)
    item_id = uuid.uuid4()
    since = phone.inbox.mark()
    value = _notification(item_id, phone.watch_time(), "Hello from the phone")
    assert blobdb.insert(Database.NOTIFICATIONS, item_id, value) == Status.SUCCESS
    _wait_modal(ui, NOTIFICATION_WINDOW)
    phone.inbox.wait(
        TIMELINE_ACTION_ENDPOINT,
        lambda p: p == bytes([DISPLAYED_ITEM]) + item_id.bytes,
        10,
        since,
    )
    time.sleep(PEEK_S)
    assert blobdb.delete(Database.NOTIFICATIONS, item_id) == Status.SUCCESS
    ui.press(Button.BACK)
    _wait_modal(ui, NOTIFICATION_WINDOW, present=False)


@pytest.mark.xfail(reason="a notification the phone deletes stays on screen")
def test_deleted_notification_leaves_screen(dut, phone, blobdb):
    ui = Ui(dut)
    item_id = uuid.uuid4()
    value = _notification(item_id, phone.watch_time(), "Deleted from the phone")
    assert blobdb.insert(Database.NOTIFICATIONS, item_id, value) == Status.SUCCESS
    _wait_modal(ui, NOTIFICATION_WINDOW)
    time.sleep(PEEK_S)
    assert blobdb.delete(Database.NOTIFICATIONS, item_id) == Status.SUCCESS
    try:
        _wait_modal(ui, NOTIFICATION_WINDOW, present=False)
    finally:
        if NOTIFICATION_WINDOW in ui.modal_stack():
            ui.press(Button.BACK)


def test_notification_action(dut, phone, blobdb):
    """An action the phone handles goes to it, the watch waits for its
    answer, shows it, and the notification is done."""
    ui = Ui(dut)
    item_id = uuid.uuid4()
    actions = [(7, ActionType.GENERIC, [(Attribute.TITLE, "Reply OK")])]
    value = _notification(item_id, phone.watch_time(), "Can you reply?", actions)
    assert blobdb.insert(Database.NOTIFICATIONS, item_id, value) == Status.SUCCESS
    _wait_modal(ui, NOTIFICATION_WINDOW)
    time.sleep(PEEK_S)

    since = phone.inbox.mark()
    ui.press(Button.SELECT)
    _wait_modal(ui, ACTION_MENU)
    ui.press(Button.SELECT)
    invoke = phone.inbox.wait(
        TIMELINE_ACTION_ENDPOINT, lambda p: p[0] == 0x02, 10, since
    )
    assert invoke == bytes([INVOKE_ACTION]) + item_id.bytes + bytes([7, 0])
    _wait_modal(ui, ACTION_PROGRESS)

    phone.send(
        TIMELINE_ACTION_ENDPOINT,
        bytes([PHONE_RESPONSE])
        + item_id.bytes
        + bytes([RESPONSE_ACK, 1])
        + attribute(Attribute.SUBTITLE, "Sent"),
    )
    _wait_modal(ui, ACTION_RESULT)
    _wait_modal(ui, NOTIFICATION_WINDOW, present=False)


def test_clear_notifications(dut, phone, blobdb):
    """Clearing the notifications while one pops up leaves the watch
    running."""
    ui = Ui(dut)
    now = phone.watch_time()
    for body in ("One of two", "Two of two"):
        item_id = uuid.uuid4()
        value = _notification(item_id, now, body)
        assert blobdb.insert(Database.NOTIFICATIONS, item_id, value) == Status.SUCCESS
    _wait_modal(ui, NOTIFICATION_WINDOW)
    assert blobdb.clear(Database.NOTIFICATIONS) == Status.SUCCESS
    time.sleep(PEEK_S)
    assert phone.watch_version() is not None
    ui.press(Button.BACK)
    _wait_modal(ui, NOTIFICATION_WINDOW, present=False)


def test_reminder_of_a_pin(dut, phone, blobdb):
    """A pin's reminder pops up when due."""
    ui = Ui(dut)
    now = phone.watch_time()
    pin_id = uuid.uuid4()
    pin = timeline_item(
        pin_id,
        ItemType.PIN,
        Layout.GENERIC,
        now + 3600,
        [(Attribute.TITLE, "Itest meeting")],
    )
    assert blobdb.insert(Database.PINS, pin_id, pin) == Status.SUCCESS

    reminder_id = uuid.uuid4()
    reminder = timeline_item(
        reminder_id,
        ItemType.REMINDER,
        Layout.REMINDER,
        now + 3,
        [(Attribute.TITLE, "Itest meeting soon")],
        parent_id=pin_id,
    )
    assert blobdb.insert(Database.REMINDERS, reminder_id, reminder) == Status.SUCCESS
    try:
        _wait_modal(ui, NOTIFICATION_WINDOW, timeout=20)
    finally:
        assert blobdb.delete(Database.PINS, pin_id) == Status.SUCCESS
    time.sleep(PEEK_S)
    ui.press(Button.BACK)
    _wait_modal(ui, NOTIFICATION_WINDOW, present=False)


def _app_entry(app_uuid, name):
    return struct.pack(
        "<16sIIBBBBBB96s", app_uuid.bytes, 0, 0, 1, 0, 5, 86, 0, 0, name.encode()
    )


def test_installed_app(prompt, phone, blobdb):
    """An app the phone adds to the app DB is listed, until it is deleted."""
    app_uuid = uuid.uuid4()
    name = "Itest App"
    assert blobdb.insert(Database.APPS, app_uuid, _app_entry(app_uuid, name)) == (
        Status.SUCCESS
    )
    try:
        assert any(name in line for line in prompt("app list"))
    finally:
        assert blobdb.delete(Database.APPS, app_uuid) == Status.SUCCESS
    assert not any(name in line for line in prompt("app list"))


def _glance(creation_time, subtitle):
    attributes = (
        struct.pack("<BHI", GLANCE_EXPIRATION, 4, 0)
        + struct.pack("<BH", GLANCE_SUBTITLE, len(subtitle))
        + subtitle.encode()
    )
    slice_ = struct.pack("<HBB", 4 + len(attributes), 0, 2) + attributes
    return struct.pack("<BI", GLANCE_VERSION, creation_time) + slice_


def test_app_glance(phone, blobdb):
    """Glances are taken for installed apps only, and only newer than the
    one stored."""
    now = phone.watch_time()
    db = Database.APP_GLANCE
    assert blobdb.insert(db, SETTINGS_UUID, _glance(now, "First")) == Status.SUCCESS
    try:
        assert blobdb.insert(db, SETTINGS_UUID, _glance(now, "Same")) == (
            Status.INVALID_DATA
        )
        assert blobdb.insert(db, SETTINGS_UUID, _glance(now + 1, "Newer")) == (
            Status.SUCCESS
        )
        assert blobdb.insert(db, uuid.uuid4(), _glance(now, "Nobody")) == (
            Status.KEY_DOES_NOT_EXIST
        )
    finally:
        assert blobdb.delete(db, SETTINGS_UUID) == Status.SUCCESS


def test_rejected_requests(phone, blobdb):
    item_id = uuid.uuid4()
    value = _notification(item_id, phone.watch_time(), "Rejected")
    assert blobdb.insert(0x7F, item_id, value) == Status.INVALID_DATABASE_ID
    assert blobdb.insert(Database.NOTIFICATIONS, item_id, value[:20]) == (
        Status.INVALID_DATA
    )
    assert blobdb.request(Command.INSERT, bytes([Database.NOTIFICATIONS, 16])) == (
        Status.INVALID_DATA
    )
    body = bytes([Database.NOTIFICATIONS, 16]) + item_id.bytes
    assert blobdb.request(Command.READ, body) == Status.INVALID_OPERATION
    assert blobdb.delete(Database.APPS, item_id) == Status.KEY_DOES_NOT_EXIST


def _blobdb2(phone, command, body=b"", token=0x1234, timeout=10):
    since = phone.inbox.mark()
    phone.send(BLOBDB2_ENDPOINT, struct.pack("<BH", command, token) + body)
    return phone.inbox.wait(
        BLOBDB2_ENDPOINT,
        lambda p: p[0] == command | RESPONSE and p[1:3] == struct.pack("<H", token),
        timeout,
        since,
    )


class _SyncAnswerer:
    """Acknowledges every write the watch sends, as a phone does, and keeps
    the keys written per database and the databases it finished."""

    def __init__(self, phone):
        self.phone = phone
        self.keys = {}
        self.done = set()
        self._stop = threading.Event()
        self._thread = threading.Thread(target=self._run, daemon=True)

    def __enter__(self):
        self._thread.start()
        return self

    def __exit__(self, *exc):
        self._stop.set()
        self._thread.join()

    def _run(self):
        since = 0
        while not self._stop.is_set():
            try:
                message, since = self.phone.inbox.next(
                    BLOBDB2_ENDPOINT,
                    lambda p: p[0] in (WRITE, WRITEBACK, SYNC_DONE),
                    0.2,
                    since,
                )
            except WatchTimeout:
                continue
            command, token, db = message[0], message[1:3], message[3]
            self.phone.send(
                BLOBDB2_ENDPOINT, bytes([command | RESPONSE]) + token + b"\x01"
            )
            if command == SYNC_DONE:
                self.done.add(db)
            else:
                (key_len,) = struct.unpack_from("<B", message, 8)
                self.keys.setdefault(db, []).append(bytes(message[9 : 9 + key_len]))

    def wait_done(self, db, timeout):
        deadline = time.monotonic() + timeout
        while db not in self.done:
            if time.monotonic() > deadline:
                raise WatchTimeout(f"database {db} was not synced within {timeout}s")
            time.sleep(0.2)


def _dirty_dbs(phone):
    response = _blobdb2(phone, DIRTY_DBS)
    assert response[3] == Status.SUCCESS
    return set(response[5 : 5 + response[4]])


def test_blobdb2_version(phone):
    response = _blobdb2(phone, VERSION)
    assert response[3:] == bytes([Status.SUCCESS, 1])


@pytest.mark.usefixtures("only_phone")
def test_settings_sync(phone):
    """The watch writes its settings back to the phone when the phone marks
    them all dirty and asks for a sync."""
    with _SyncAnswerer(phone) as answerer:
        response = _blobdb2(phone, DIRTY_ALL, bytes([Database.SETTINGS]))
        assert response[3] == Status.SUCCESS
        assert Database.SETTINGS in _dirty_dbs(phone)

        response = _blobdb2(phone, START_SYNC, bytes([Database.SETTINGS]))
        # A sync left over from an earlier connection goes on when it retries.
        assert response[3] in (Status.SUCCESS, Status.TRY_LATER)
        answerer.wait_done(Database.SETTINGS, SYNC_RETRY_S + 15)
        assert answerer.keys.get(Database.SETTINGS)
        assert Database.SETTINGS not in _dirty_dbs(phone)

    response = _blobdb2(phone, DIRTY_ALL, bytes([Database.NOTIFICATIONS]))
    assert response[3] == Status.NOT_SUPPORTED
