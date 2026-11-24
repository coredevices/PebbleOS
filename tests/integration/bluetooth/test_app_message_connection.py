# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""AppMessage and the connection interval: only messages over
APP_MESSAGE_FAST_CONNECTION_MIN_SIZE (500 bytes) ask the phone for the
fastest link; small ones, and the ACK for them, leave it relaxed."""

import struct
import time
import uuid

import pytest
from harness.errors import WatchTimeout
from harness.helpers.ui import Button, Ui

pytestmark = pytest.mark.integration_boards("getafix@dvt2", "obelix@pvt")

APP_MESSAGE_ENDPOINT = 0x0030
APP_RUN_STATE_ENDPOINT = 0x0034
RUN_STATE_START = 0x01
PUSH = 0x01
ACK = 0xFF
NACK = 0x7F
TUPLE_CSTRING = 1

SPORTS_UUID = uuid.UUID("4dab81a6-d2fc-458a-992c-7a1f3b96a970")
SPORTS_TIME_KEY = 0
SPORTS_STATE_KEY = 4

FAST_MAX_INTERVAL_MS = 20  # PBL_BT_RESPONSE_TIME_MIN asks for 15 ms
FAST_MODE_TIMEOUT_S = 10  # PBL_BT_MIN_LATENCY_MODE_TIMEOUT_APP_MESSAGE_SECS
SETTLE_S = FAST_MODE_TIMEOUT_S + 3
WATCH_S = 3.0
SMALL_MESSAGES = 5
LARGE_PAYLOAD = "x" * 600  # over the 500 byte threshold


def _fast_requests(phone, since):
    """The watch's requests for the fastest link made after ``since``."""
    return [
        r
        for r in phone.link.parameter_requests[since:]
        if r.latency == 0 and r.interval_max_ms <= FAST_MAX_INTERVAL_MS
    ]


def _wait_relaxed(phone, timeout=60.0):
    """Returns once the watch has asked for the relaxed link and the fast
    mode it may have held (session setup, starting the app) has expired."""
    deadline = time.monotonic() + timeout
    while not [r for r in phone.link.parameter_requests if r.latency]:
        if time.monotonic() > deadline:
            raise WatchTimeout("the watch never asked for a relaxed link")
        time.sleep(0.5)
    time.sleep(SETTLE_S)


def _push(transaction, tuples):
    body = b"".join(
        struct.pack("<IBH", key, TUPLE_CSTRING, len(v) + 1) + v.encode() + b"\0"
        for key, v in tuples
    )
    return (
        struct.pack("<BB", PUSH, transaction)
        + SPORTS_UUID.bytes
        + struct.pack("<B", len(tuples))
        + body
    )


def _send_push(phone, transaction, tuples, timeout=15):
    since = phone.inbox.mark()
    phone.send(APP_MESSAGE_ENDPOINT, _push(transaction, tuples))
    reply = phone.inbox.wait(
        APP_MESSAGE_ENDPOINT,
        lambda p: p[0] in (ACK, NACK) and p[1] == transaction,
        timeout,
        since,
    )
    return reply[0]


@pytest.fixture
def sports(dut, prompt, phone, ui):
    """The Sports app running, which has an inbox, on a link that has
    settled into its relaxed parameters."""
    phone.send(APP_RUN_STATE_ENDPOINT, bytes([RUN_STATE_START]) + SPORTS_UUID.bytes)
    deadline = time.monotonic() + 10
    while not any("Sports" in line for line in prompt("app active")):
        if time.monotonic() > deadline:
            raise AssertionError("Sports did not start")
        time.sleep(0.2)
    _wait_relaxed(phone)
    yield phone
    ui.go_home()


def test_small_messages_keep_link_relaxed(sports):
    """Small messages in, and their ACKs out, never ask for the fast link."""
    since = len(sports.link.parameter_requests)
    for n in range(SMALL_MESSAGES):
        assert _send_push(sports, 0x40 + n, [(SPORTS_TIME_KEY, "12:34")]) == ACK
    time.sleep(WATCH_S)
    assert _fast_requests(sports, since) == []


def test_small_message_out_keeps_link_relaxed(dut, sports):
    """A small message from the app (Sports sends one on SELECT) leaves the
    link relaxed too."""
    since = len(sports.link.parameter_requests)
    mark = sports.inbox.mark()
    Ui(dut).press(Button.SELECT)
    message = sports.inbox.wait(
        APP_MESSAGE_ENDPOINT,
        lambda p: p[0] == PUSH and p[2:18] == SPORTS_UUID.bytes,
        10,
        mark,
    )
    sports.send(APP_MESSAGE_ENDPOINT, bytes([ACK, message[1]]))
    time.sleep(WATCH_S)
    assert _fast_requests(sports, since) == []


def test_large_message_requests_fast_link(sports):
    """A message over the threshold still asks for the fast link, whether the
    app accepts it or not."""
    since = len(sports.link.parameter_requests)
    assert _send_push(sports, 0x60, [(SPORTS_TIME_KEY, LARGE_PAYLOAD)]) in (ACK, NACK)
    assert _fast_requests(sports, since), "no request for the fast link"
