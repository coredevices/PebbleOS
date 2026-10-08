# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Apps from the phone: starting and stopping them, app messages both
ways, fetching an app the watch does not have, and the launcher order."""

import re
import struct
import time
import uuid

import pytest
from harness.helpers.blobdb import BlobDB, Database, Status
from harness.helpers.ui import Button, Ui

pytestmark = pytest.mark.integration_boards("qemu_emery")

APP_MESSAGE_ENDPOINT = 0x0030
LAUNCHER_ENDPOINT = 0x0031
APP_RUN_STATE_ENDPOINT = 0x0034
APP_FETCH_ENDPOINT = 6001
APP_ORDER_ENDPOINT = 0xABCD

RUN_STATE_START = 0x01
RUN_STATE_STOP = 0x02
RUN_STATE_REQUEST = 0x03
RUNNING = 0x01
NOT_RUNNING = 0x02

PUSH = 0x01
ACK = 0xFF
NACK = 0x7F
TUPLE_CSTRING = 1
TUPLE_UINT = 2
LAUNCHER_RUN_STATE_KEY = 1
LAUNCHER_STATE_FETCH_KEY = 2

FETCH_REQUEST = 0x01
FETCH_NO_DATA = 0x04

APP_ORDER = 0x01
APP_ORDER_SUCCESS = 0x01
APP_ORDER_INVALID = 0x03

SETTINGS_UUID = uuid.UUID("07e0d9cb-8957-4bf7-9d42-35bf47caadfe")
SPORTS_UUID = uuid.UUID("4dab81a6-d2fc-458a-992c-7a1f3b96a970")
SPORTS_TIME_KEY = 0
SPORTS_STATE_KEY = 4
APP_START_S = 10.0


def _active_app(prompt):
    for line in prompt("app active"):
        if m := re.match(r"app name: (.*)", line):
            return m.group(1)
    return None


def _wait_app(prompt, name, present=True, timeout=APP_START_S):
    deadline = time.monotonic() + timeout
    while (_active_app(prompt) == name) != present:
        if time.monotonic() > deadline:
            raise AssertionError(f"running {_active_app(prompt)!r}, waiting for {name}")
        time.sleep(0.2)


def _tuple(key, value):
    if isinstance(value, str):
        data = value.encode() + b"\0"
        kind = TUPLE_CSTRING
    else:
        data = struct.pack("<B", value)
        kind = TUPLE_UINT
    return struct.pack("<IBH", key, kind, len(data)) + data


def _push(transaction, app_uuid, tuples):
    return (
        struct.pack("<BB", PUSH, transaction)
        + app_uuid.bytes
        + struct.pack("<B", len(tuples))
        + b"".join(_tuple(k, v) for k, v in tuples)
    )


def _send_push(phone, endpoint, transaction, app_uuid, tuples, timeout=10):
    """Push a dictionary; returns the watch's ACK or NACK."""
    since = phone.inbox.mark()
    phone.send(endpoint, _push(transaction, app_uuid, tuples))
    reply = phone.inbox.wait(
        endpoint, lambda p: p[0] in (ACK, NACK) and p[1] == transaction, timeout, since
    )
    return reply[0]


@pytest.fixture
def home(dut, ui):
    yield ui
    ui.go_home()


def test_app_run_state(dut, prompt, phone, home):
    """The phone starts and stops apps, asks which one runs, and hears of
    every change."""
    since = phone.inbox.mark()
    phone.send(APP_RUN_STATE_ENDPOINT, bytes([RUN_STATE_START]) + SETTINGS_UUID.bytes)
    _wait_app(prompt, "Settings")
    phone.inbox.wait(
        APP_RUN_STATE_ENDPOINT,
        lambda p: p == bytes([RUNNING]) + SETTINGS_UUID.bytes,
        10,
        since,
    )

    since = phone.inbox.mark()
    phone.send(APP_RUN_STATE_ENDPOINT, bytes([RUN_STATE_REQUEST]))
    phone.inbox.wait(
        APP_RUN_STATE_ENDPOINT,
        lambda p: p == bytes([RUNNING]) + SETTINGS_UUID.bytes,
        10,
        since,
    )

    since = phone.inbox.mark()
    phone.send(APP_RUN_STATE_ENDPOINT, bytes([RUN_STATE_STOP]) + SETTINGS_UUID.bytes)
    _wait_app(prompt, "Settings", present=False)
    phone.inbox.wait(
        APP_RUN_STATE_ENDPOINT,
        lambda p: p == bytes([NOT_RUNNING]) + SETTINGS_UUID.bytes,
        10,
        since,
    )

    running = _active_app(prompt)
    phone.send(APP_RUN_STATE_ENDPOINT, bytes([RUN_STATE_START]) + uuid.uuid4().bytes)
    time.sleep(2.0)
    assert _active_app(prompt) == running


def test_launcher_app_message(prompt, phone, home):
    """The launcher's app message interface, kept for old phone apps."""
    reply = _send_push(
        phone, LAUNCHER_ENDPOINT, 1, SETTINGS_UUID, [(LAUNCHER_RUN_STATE_KEY, 1)]
    )
    assert reply == ACK
    _wait_app(prompt, "Settings")

    since = phone.inbox.mark()
    reply = _send_push(
        phone, LAUNCHER_ENDPOINT, 2, SETTINGS_UUID, [(LAUNCHER_STATE_FETCH_KEY, 1)]
    )
    assert reply == ACK
    state = phone.inbox.wait(LAUNCHER_ENDPOINT, lambda p: p[0] == PUSH, 10, since)
    assert state[2:18] == SETTINGS_UUID.bytes

    reply = _send_push(
        phone, LAUNCHER_ENDPOINT, 3, SETTINGS_UUID, [(LAUNCHER_RUN_STATE_KEY, 0)]
    )
    assert reply == ACK
    _wait_app(prompt, "Settings", present=False)

    assert _send_push(phone, LAUNCHER_ENDPOINT, 4, SETTINGS_UUID, [(9, 1)]) == NACK


@pytest.mark.usefixtures("only_phone")
def test_app_message(dut, prompt, phone, home):
    """App messages reach the app they are for, and the app's reach the
    phone; one for an app that is not running is refused."""
    phone.send(APP_RUN_STATE_ENDPOINT, bytes([RUN_STATE_START]) + SPORTS_UUID.bytes)
    _wait_app(prompt, "Sports")

    reply = _send_push(
        phone, APP_MESSAGE_ENDPOINT, 0x21, SPORTS_UUID, [(SPORTS_TIME_KEY, "12:34")]
    )
    assert reply == ACK
    reply = _send_push(
        phone, APP_MESSAGE_ENDPOINT, 0x22, uuid.uuid4(), [(SPORTS_TIME_KEY, "12:34")]
    )
    assert reply == NACK

    since = phone.inbox.mark()
    Ui(dut).press(Button.SELECT)
    message = phone.inbox.wait(
        APP_MESSAGE_ENDPOINT,
        lambda p: p[0] == PUSH and p[2:18] == SPORTS_UUID.bytes,
        10,
        since,
    )
    assert struct.pack("<I", SPORTS_STATE_KEY) in message[19:]
    phone.send(APP_MESSAGE_ENDPOINT, bytes([ACK, message[1]]))


def _app_entry(app_uuid, name):
    return struct.pack(
        "<16sIIBBBBBB96s", app_uuid.bytes, 0, 0, 1, 0, 5, 86, 0, 0, name.encode()
    )


def test_app_fetch(dut, phone, home):
    """Starting an app the watch knows of but does not have asks the phone
    for it; the phone not having it either ends the fetch."""
    blobdb = BlobDB(phone)
    app_uuid = uuid.uuid4()
    entry = _app_entry(app_uuid, "Itest Fetch")
    assert blobdb.insert(Database.APPS, app_uuid, entry) == Status.SUCCESS
    try:
        since = phone.inbox.mark()
        phone.send(APP_RUN_STATE_ENDPOINT, bytes([RUN_STATE_START]) + app_uuid.bytes)
        request = phone.inbox.wait(APP_FETCH_ENDPOINT, None, 15, since)
        assert request[0] == FETCH_REQUEST
        assert request[1:17] == app_uuid.bytes
        (app_id,) = struct.unpack_from("<i", request, 17)
        assert app_id > 0

        log_since = dut.logs.mark()
        phone.send(APP_FETCH_ENDPOINT, bytes([FETCH_REQUEST, FETCH_NO_DATA]))
        dut.wait_for_log(r"App fetch cleanup with result", 10, log_since)
    finally:
        assert blobdb.delete(Database.APPS, app_uuid) == Status.SUCCESS


def _app_order(phone, count, uuids):
    since = phone.inbox.mark()
    phone.send(
        APP_ORDER_ENDPOINT,
        bytes([APP_ORDER, count]) + b"".join(u.bytes for u in uuids),
    )
    first = phone.inbox.wait(APP_ORDER_ENDPOINT, None, 10, since)
    time.sleep(1.0)
    return first, len(phone.inbox.received(APP_ORDER_ENDPOINT, since))


def test_app_order(phone):
    """The launcher order is taken when it is well formed, refused once
    otherwise."""
    uuids = [SETTINGS_UUID, SPORTS_UUID]
    assert _app_order(phone, len(uuids), uuids) == (bytes([APP_ORDER_SUCCESS]), 1)
    assert _app_order(phone, len(uuids) + 1, uuids) == (bytes([APP_ORDER_INVALID]), 1)
