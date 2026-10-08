# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""The Pebble Protocol over GATT, as the watch runs it: windows sized to the
link, transfers larger than a window, resets from either side, lost
packets, and the versions it agrees on with a phone hosting the service."""

import re
import time
import uuid

import pytest
from harness.ble import FORWARD, PPOG_FORWARD_META
from harness.ble.ppogatt import SN_MOD
from harness.errors import Unsupported, WatchTimeout
from harness.helpers.ui import Ui

pytestmark = pytest.mark.integration_boards("qemu_emery", "getafix@dvt2")
BOTH = pytest.mark.variants("normal", "prf")

SESSION_OPENED = (
    r"PPoGATT Session is opened \((\w+), Vers: (\d+) TXW: (\d+) RXW: (\d+)\)"
)
SESSION_CLOSED = r"Session event: is_open=0"
MIN_MTU = 23
# What the harness asks for; the watch supports up to 256.
LARGE_MTU = 339
# The windows the harness offers.
PHONE_WINDOW = 25
V0_WINDOW = 4
PINGS = 100
SETTLE_S = 5.0
APP_UUID = uuid.UUID("3b0b7c4f-0e1a-4d3a-9c47-6a0c3d1e2f50")


def _meta(min_version, max_version, app=None, session_type=True):
    """A forward PPoGATT meta characteristic value."""
    value = bytes([min_version, max_version]) + (app.bytes if app else bytes(16))
    return value + bytes([0]) if session_type else value


def _opened(dut, since, timeout=15):
    record = dut.wait_for_log(SESSION_OPENED, timeout, since)
    role, version, tx, rx = re.search(SESSION_OPENED, str(record)).groups()
    return role, int(version), int(tx), int(rx)


def _settled(phone):
    """``phone`` once the watch has answered it and is done with what it
    does on a new session, nothing left in flight."""
    assert phone.watch_version() is not None
    time.sleep(SETTLE_S)
    return phone


def _ping_all(pebble, count, timeout=60.0):
    """Send ``count`` pings back to back; all their pongs, by cookie."""
    from libpebble2.protocol.system import Ping, PingPong, Pong

    pongs = pebble.get_endpoint_queue(PingPong)
    try:
        for cookie in range(count):
            pebble.send_packet(PingPong(cookie=cookie, message=Ping(idle=False)))
        cookies = set()
        deadline = time.monotonic() + timeout
        while len(cookies) < count:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                break
            try:
                message = pongs.get(timeout=remaining)
            except Exception:  # noqa: BLE001
                break
            if isinstance(message.message, Pong):
                cookies.add(message.cookie)
    finally:
        pongs.close()
    return cookies


@BOTH
@pytest.mark.parametrize("mtu", [MIN_MTU, LARGE_MTU], ids=["mtu23", "mtu_large"])
def test_window_follows_mtu(dut, watch, phones, mtu):
    """With the minimum MTU the watch sends a full window, with a large one
    the version 0 window; either way it takes the phone's."""
    since = dut.logs.mark()
    phone = phones(mtu=mtu).connect()
    role, version, tx, rx = _opened(dut, since)
    assert (role, version) == ("reversed", 1)
    assert phone.link.att_mtu == min(mtu, 256)
    assert tx == (PHONE_WINDOW if mtu == MIN_MTU else V0_WINDOW)
    assert 0 < rx <= PHONE_WINDOW


@pytest.mark.parametrize("mtu", [MIN_MTU, LARGE_MTU], ids=["mtu23", "mtu_large"])
def test_messages_back_to_back(watch, phones, mtu):
    """Many messages queued at once go through the window, sequence numbers
    wrapping, and are all answered."""
    phone = phones(mtu=mtu).connect()
    assert _ping_all(phone.pebble, PINGS) == set(range(PINGS))


@pytest.mark.parametrize("mtu", [MIN_MTU, LARGE_MTU], ids=["mtu23", "mtu_large"])
def test_large_transfer(dut, watch, phones, mtu):
    """A screenshot, tens of kilobytes from the watch, arrives whole: it is
    the screen the device shows, when the device can tell."""
    phone = phones(mtu=mtu).connect()
    for _ in range(3):
        rows = phone.screenshot()
        assert len({len(r) for r in rows}) == 1
        try:
            image = Ui(dut).screenshot()
        except Unsupported:
            return
        assert (len(rows[0]) // 3, len(rows)) == image.size
        if b"".join(bytes(r) for r in rows) == image.tobytes():
            return
    pytest.fail("the screenshot differs from the screen")


@BOTH
def test_phone_resets_session(dut, watch, phones):
    phone = _settled(phones().connect())
    since = dut.logs.mark()
    phone.link.reset_session()
    dut.wait_for_log(r"Got reset request!", 10, since)
    dut.wait_for_log(SESSION_CLOSED, 10, since)
    _opened(dut, since)
    assert phone.watch_version() is not None


def test_phone_resets_mid_transfer(dut, watch, phones):
    """A reset while the watch is sending drops what it was sending, and the
    new session works."""
    from libpebble2.protocol.screenshots import ScreenshotRequest, ScreenshotResponse

    phone = phones(mtu=MIN_MTU).connect()
    responses = phone.pebble.get_endpoint_queue(ScreenshotResponse)
    try:
        phone.pebble.send_packet(ScreenshotRequest())
        responses.get(timeout=30)
        since = dut.logs.mark()
        phone.link.reset_session()
    finally:
        responses.close()
    dut.wait_for_log(SESSION_CLOSED, 10, since)
    assert phone.watch_version() is not None
    assert _ping_all(phone.pebble, 10) == set(range(10))


@BOTH
def test_watch_resets_on_bad_ack(dut, watch, phones):
    """An acknowledgement for nothing the watch sent makes it start over."""
    phone = _settled(phones().connect())
    since = dut.logs.mark()
    # Half the sequence space away from anything the watch sent.
    sn = (phone.link.expected_sn + SN_MOD // 2) % SN_MOD
    phone.link.write_packet(bytes([sn << 3 | 1]))
    dut.wait_for_log(r"Ack'd packet out of range", 10, since)
    _opened(dut, since)
    phone.link.wait_session_reopened()
    assert phone.watch_version() is not None


@BOTH
def test_watch_resends_lost_data(dut, watch, phones):
    """Data the phone does not acknowledge is sent again."""
    phone = phones().connect()
    since = dut.logs.mark()
    phone.link.ignore_data(1)
    assert phone.watch_version(timeout=30) is not None
    dut.wait_for_log(r"Rolling back", 1, since)


@BOTH
@pytest.mark.parametrize(
    "meta,version,window",
    [
        (_meta(0, 0, session_type=False), 0, V0_WINDOW),
        (PPOG_FORWARD_META, 1, None),
        (_meta(0, 5), 1, None),
    ],
    ids=["v0", "v1", "newer"],
)
def test_forward_version(dut, watch, phones, meta, version, window):
    """A phone hosting the service gets the highest version both support."""
    since = dut.logs.mark()
    phone = phones(ppogatt=FORWARD, forward_meta=meta).connect()
    role, agreed, tx, rx = _opened(dut, since)
    assert (role, agreed) == ("forward", version)
    if window is not None:
        assert (tx, rx) == (window, window)
    assert phone.watch_version() is not None


@BOTH
def test_forward_version_unsupported(dut, watch, phones):
    """A phone that needs a newer version than the watch's gets no session."""
    since = dut.logs.mark()
    phone = phones(ppogatt=FORWARD, forward_meta=_meta(2, 2))
    with pytest.raises(WatchTimeout):
        phone.connect()
    dut.wait_for_log(r"Failed handling PPoGATT meta", 10, since)
    assert not dut.logs.find(SESSION_OPENED, since)


@BOTH
def test_forward_app_session(dut, build, watch, phones):
    """A third-party app's service opens an app session; PRF only talks to
    the Pebble app."""
    since = dut.logs.mark()
    phone = phones(ppogatt=FORWARD, forward_meta=_meta(0, 1, APP_UUID))
    if build.variant == "prf":
        with pytest.raises(WatchTimeout):
            phone.connect()
        dut.wait_for_log(r"not connecting in PRF", 10, since)
        return
    phone.connect()
    prefix = str(APP_UUID)[:8]
    dut.wait_for_log(rf"is_open=1, destination=A, app_uuid=\{{{prefix}", 10, since)
