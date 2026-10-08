# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""What the phone exchanges with the watch in the background: pings, the
data logging the watch collects, health syncs, files, screenshots and
polls."""

import re
import struct
import time

import pytest
from harness.helpers.ui import protocol_screenshot

pytestmark = pytest.mark.integration_boards("qemu_emery", "getafix@dvt2")

PING_ENDPOINT = 2001
HEALTH_SYNC_ENDPOINT = 911
DATA_LOGGING_ENDPOINT = 0x1A7A
GET_BYTES_ENDPOINT = 9000
POLL_REMOTE_ENDPOINT = 0xCAFE

PING = 0x00
PONG = 0x01
# The watch's own pings carry this cookie.
WATCH_PING_COOKIE = 42

HEALTH_SYNC = 0x01
HEALTH_SYNC_ACK = 0x11
ACK = 0x01

DLS_OPEN = 0x01
DLS_DATA = 0x02
DLS_ACK = 0x85
DLS_GET_SEND_ENABLE = 0x89
DLS_SEND_ENABLE = 0x0A
DLS_SET_SEND_ENABLE = 0x8B
# Data logging's tag for native analytics heartbeats.
DLS_TAG_HEARTBEAT = 87

GET_BYTES_FILE = 0x03
GET_BYTES_INFO = 0x01
GET_BYTES_DATA = 0x02
GET_BYTES_OK = 0x00
OBJECT_FILE = 0x06
PUT_BYTES_ACK = 0x01
# What the watch with the smallest receive buffer takes in one message.
PUT_BYTES_CHUNK = 1000

POLL = 0x00
POLL_SET_INTERVAL = 0x02
POLL_SERVICE_MAIL = 0x00
POLL_WAIT_S = 150


def test_ping_from_phone(dut, phone, ui):
    since = phone.inbox.mark()
    phone.send(PING_ENDPOINT, struct.pack(">BIB", PING, 0xC0FFEE, 1))
    phone.inbox.wait(
        PING_ENDPOINT, lambda p: p == struct.pack(">BI", PONG, 0xC0FFEE), 10, since
    )
    deadline = time.monotonic() + 10
    while not ui.modal_stack():
        assert time.monotonic() < deadline, "no ping dialog"
        time.sleep(0.2)
    ui.go_home()


def test_ping_from_watch(prompt, phone):
    since = phone.inbox.mark()
    prompt("ping")
    ping = phone.inbox.wait(PING_ENDPOINT, lambda p: p[0] == PING, 10, since)
    assert struct.unpack_from(">I", ping, 1)[0] == WATCH_PING_COOKIE
    phone.send(PING_ENDPOINT, struct.pack(">BI", PONG, WATCH_PING_COOKIE))


def test_health_sync(phone):
    since = phone.inbox.mark()
    phone.send(HEALTH_SYNC_ENDPOINT, struct.pack("<BI", HEALTH_SYNC, 3600))
    phone.inbox.wait(
        HEALTH_SYNC_ENDPOINT, lambda p: p == bytes([HEALTH_SYNC_ACK, ACK]), 15, since
    )


def _dls_sessions(prompt):
    """``dls list``: session id -> (tag, bytes)."""
    sessions = {}
    for line in prompt("dls list"):
        if m := re.match(r"session_id : (\d+), tag: (\d+), bytes: (\d+)", line):
            sessions[int(m.group(1))] = (int(m.group(2)), int(m.group(3)))
    return sessions


def test_data_logging(prompt, phone):
    """What the watch logs goes to the phone, session by session, and is
    dropped from the watch once the phone has it."""
    prompt("analytics heartbeat")
    sessions = _dls_sessions(prompt)
    pending = {
        s for s, (tag, size) in sessions.items() if tag == DLS_TAG_HEARTBEAT and size
    }
    assert pending, f"no heartbeat logged: {sessions}"
    (session,) = pending

    prompt("dls send")
    deadline = time.monotonic() + 30
    since = 0
    data = b""
    while _dls_sessions(prompt)[session][1]:
        message, since = phone.inbox.next(
            DATA_LOGGING_ENDPOINT,
            lambda p: p[0] in (DLS_OPEN, DLS_DATA) and p[1] == session,
            max(deadline - time.monotonic(), 0.1),
            since,
        )
        if message[0] == DLS_OPEN:
            (tag,) = struct.unpack_from("<I", message, 22)
            assert tag == DLS_TAG_HEARTBEAT
        else:
            data += message[10:]
        phone.send(DATA_LOGGING_ENDPOINT, bytes([DLS_ACK, session]))
        time.sleep(0.2)
    assert data, "no data sent"


def _send_enabled(phone):
    since = phone.inbox.mark()
    phone.send(DATA_LOGGING_ENDPOINT, bytes([DLS_GET_SEND_ENABLE]))
    reply = phone.inbox.wait(
        DATA_LOGGING_ENDPOINT, lambda p: p[0] == DLS_SEND_ENABLE, 10, since
    )
    return bool(reply[1])


def test_data_logging_send_enable(phone):
    assert _send_enabled(phone)
    phone.send(DATA_LOGGING_ENDPOINT, bytes([DLS_SET_SEND_ENABLE, 0]))
    try:
        assert not _send_enabled(phone)
    finally:
        phone.send(DATA_LOGGING_ENDPOINT, bytes([DLS_SET_SEND_ENABLE, 1]))
    assert _send_enabled(phone)


def test_screenshot(dut, phone):
    rows = protocol_screenshot(phone.pebble)
    width = len(rows[0]) // 3
    assert width > 0 and len(rows) > 0
    assert all(len(row) == width * 3 for row in rows)
    shown = dut.screenshot()
    if shown is not None:
        assert shown.size == (width, len(rows))


def _put_file(phone, name, data):
    from libpebble2.protocol import transfers
    from libpebble2.util import stm32_crc

    def request(command, payload):
        response = phone.pebble.send_and_read(
            transfers.PutBytes(command=command, data=payload),
            transfers.PutBytesResponse,
            timeout=10,
        )
        assert response.result == PUT_BYTES_ACK, f"put bytes {command:#x} refused"
        return response.cookie

    cookie = request(
        0x01,
        transfers.PutBytesInit(
            object_size=len(data), object_type=OBJECT_FILE, bank=0, filename=name
        ),
    )
    for offset in range(0, len(data), PUT_BYTES_CHUNK):
        chunk = data[offset : offset + PUT_BYTES_CHUNK]
        request(0x02, transfers.PutBytesPut(cookie=cookie, payload=chunk))
    request(
        0x03, transfers.PutBytesCommit(cookie=cookie, object_crc=stm32_crc.crc32(data))
    )
    request(0x05, transfers.PutBytesInstall(cookie=cookie))


def _get_file(phone, name, transaction):
    since = phone.inbox.mark()
    phone.send(
        GET_BYTES_ENDPOINT,
        bytes([GET_BYTES_FILE, transaction, len(name)]) + name.encode() + b"\0",
    )
    info, since = phone.inbox.next(
        GET_BYTES_ENDPOINT,
        lambda p: p[0] == GET_BYTES_INFO and p[1] == transaction,
        10,
        since,
    )
    if info[2] != GET_BYTES_OK:
        return info[2], None
    (size,) = struct.unpack_from(">I", info, 3)
    data = bytearray(size)
    received = 0
    while received < size:
        chunk, since = phone.inbox.next(
            GET_BYTES_ENDPOINT,
            lambda p: p[0] == GET_BYTES_DATA and p[1] == transaction,
            10,
            since,
        )
        (offset,) = struct.unpack_from(">I", chunk, 2)
        data[offset : offset + len(chunk) - 6] = chunk[6:]
        received += len(chunk) - 6
    return GET_BYTES_OK, bytes(data)


def test_file_transfer(prompt, phone):
    """A file the phone puts on the watch comes back the same."""
    name = "itest-file"
    data = bytes(range(256)) * 12
    _put_file(phone, name, data)
    try:
        assert any(name in line for line in prompt("pfs ls"))
        assert _get_file(phone, name, 1) == (GET_BYTES_OK, data)
    finally:
        prompt(f"pfs rm {name}")
    # A missing file is refused as a malformed request.
    status, data = _get_file(phone, name, 2)
    assert status != GET_BYTES_OK and data is None


def test_poll_remote(phone):
    """The phone has the watch ask it to check for mail every minute."""
    since = phone.inbox.mark()
    phone.send(POLL_REMOTE_ENDPOINT, bytes([POLL_SET_INTERVAL, POLL_SERVICE_MAIL, 1]))
    phone.inbox.wait(
        POLL_REMOTE_ENDPOINT,
        lambda p: p == bytes([POLL, POLL_SERVICE_MAIL]),
        POLL_WAIT_S,
        since,
    )
