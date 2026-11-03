# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Settings files, through what the phone app sees of them: blob DB status
codes and settings sync."""

import itertools
import struct
import time

import pytest
from harness.helpers.blobdb import BlobDB, Database, Status
from harness.helpers.phone import Inbox, send_raw

BLOBDB2_ENDPOINT = 0xB2DB
TIME_ENDPOINT = 0x000B
SET_UTC = 0x03

DIRTY_DBS = 0x06
START_SYNC = 0x07
WRITE = 0x08
WRITEBACK = 0x09
SYNC_DONE = 0x0A
DIRTY_ALL = 0x0C
RESPONSE = 0x80


class SyncPhone:
    """The harness's session as a phone app that supports settings sync: it
    accepts every setting the watch writes back."""

    _tokens = itertools.count(0x8000)

    def __init__(self, dut):
        dut.set_phone_settings_sync(True)
        self.pebble = dut.protocol
        self.inbox = Inbox()
        self.blobdb = BlobDB(self)
        self._handle = self.pebble.register_raw_inbound_handler(self._on_frame)

    def close(self):
        self.pebble.unregister_endpoint(self._handle)

    def _on_frame(self, frame):
        length, endpoint = struct.unpack_from(">HH", frame)
        payload = bytes(frame[4 : 4 + length])
        if endpoint == BLOBDB2_ENDPOINT and payload[0] in (WRITE, WRITEBACK, SYNC_DONE):
            reply = bytes([payload[0] | RESPONSE]) + payload[1:3]
            self.send(BLOBDB2_ENDPOINT, reply + bytes([Status.SUCCESS]))
        self.inbox(frame)

    def send(self, endpoint, payload):
        send_raw(self.pebble, endpoint, payload)

    def request(self, cmd, body, timeout=10.0):
        """A blob DB 2 request's status."""
        token = struct.pack("<H", next(self._tokens) & 0xFFFF)
        since = self.inbox.mark()
        self.send(BLOBDB2_ENDPOINT, bytes([cmd]) + token + body)
        response = self.inbox.wait(
            BLOBDB2_ENDPOINT,
            lambda p: p[0] == cmd | RESPONSE and p[1:3] == token,
            timeout,
            since,
        )
        return Status(response[3])

    def writes(self, since=0):
        """The ``(db, key, value)`` the watch wrote after mark ``since``."""
        found = []
        for p in self.inbox.received(BLOBDB2_ENDPOINT, since):
            if p[0] in (WRITE, WRITEBACK):
                key_len = p[8]
                key = p[9 : 9 + key_len]
                (val_len,) = struct.unpack_from("<H", p, 9 + key_len)
                found.append((p[3], key, p[11 + key_len : 11 + key_len + val_len]))
        return found

    def synced(self, since=0):
        """The databases whose sync ended after mark ``since``."""
        return [
            p[3]
            for p in self.inbox.received(BLOBDB2_ENDPOINT, since)
            if p[0] == SYNC_DONE
        ]

    def wait(self, pred, timeout):
        deadline = time.monotonic() + timeout
        while not pred():
            if time.monotonic() > deadline:
                return False
            time.sleep(0.2)
        return True


@pytest.fixture
def sync_phone(dut):
    sync_phone = SyncPhone(dut)
    # Let a sync started by the new session end.
    time.sleep(3)
    yield sync_phone
    sync_phone.close()
    dut.set_phone_settings_sync(False)


def test_status_codes(sync_phone):
    """Settings file errors reach the phone as blob DB statuses."""
    blobdb = sync_phone.blobdb
    assert blobdb.delete(Database.APPS, bytes(16)) == Status.KEY_DOES_NOT_EXIST
    assert blobdb.delete(Database.IOS_NOTIF_PREFS, b"noSuchApp") == Status.SUCCESS
    notification = blobdb.insert(Database.NOTIFICATIONS, bytes(16), bytes(20))
    assert notification == Status.INVALID_DATA
    assert blobdb.insert(Database.SETTINGS, b"clock24h\0", b"\x00") == Status.SUCCESS
    prefs = struct.pack("<IBB", 0, 0, 0)
    assert blobdb.insert(Database.IOS_NOTIF_PREFS, b"com.test", prefs) == Status.SUCCESS
    assert blobdb.delete(Database.IOS_NOTIF_PREFS, b"com.test") == Status.SUCCESS


def test_sync_round_trip(sync_phone):
    """A full sync writes back what the phone set, and leaves nothing
    dirty."""
    blobdb = sync_phone.blobdb
    assert blobdb.insert(Database.SETTINGS, b"clock24h\0", b"\x01") == Status.SUCCESS
    assert blobdb.insert(Database.SETTINGS, b"vibeIntensity", b"\x02") == Status.SUCCESS
    since = sync_phone.inbox.mark()
    assert sync_phone.request(DIRTY_ALL, bytes([Database.SETTINGS])) == Status.SUCCESS
    status = sync_phone.request(START_SYNC, bytes([Database.SETTINGS]))
    assert status in (Status.SUCCESS, Status.TRY_LATER)
    assert sync_phone.wait(lambda: Database.SETTINGS in sync_phone.synced(since), 60)
    settings = {
        k: v for db, k, v in sync_phone.writes(since) if db == Database.SETTINGS
    }
    assert settings[b"clock24h\0"] == b"\x01"
    assert settings[b"vibeIntensity"] == b"\x02"
    assert sync_phone.request(DIRTY_DBS, b"") == Status.SUCCESS


def _set_utc(phone, offset_s, region):
    utc = struct.pack(">BIhB", SET_UTC, int(time.time()), offset_s // 60, len(region))
    phone.send(TIME_ENDPOINT, utc + region)


@pytest.fixture
def restore_time(prompt):
    yield
    prompt("time tz_clear")
    prompt(f"time set {int(time.time())}")


def test_watch_change_syncs(sync_phone, restore_time):
    """A setting the watch changes itself is written to the phone."""
    since = sync_phone.inbox.mark()
    for name in (b"Europe/Madrid", b"America/New_York"):
        _set_utc(sync_phone, 3600, name)
        time.sleep(1)
    assert sync_phone.wait(
        lambda: any(
            db == Database.SETTINGS and k.startswith(b"automaticTimezoneID")
            for db, k, _ in sync_phone.writes(since)
        ),
        30,
    ), f"no timezone write-back: {sync_phone.writes(since)}"


def test_other_file_does_not_sync(sync_phone):
    """A record in another settings file, keyed as a synced setting is, does
    not start a settings sync."""
    since = sync_phone.inbox.mark()
    prefs = struct.pack("<IBB", 0, 0, 0)
    status = sync_phone.blobdb.insert(Database.IOS_NOTIF_PREFS, b"clock24h\0", prefs)
    assert status == Status.SUCCESS
    synced = sync_phone.wait(
        lambda: (
            Database.SETTINGS in sync_phone.synced(since)
            or any(db == Database.SETTINGS for db, _, _ in sync_phone.writes(since))
        ),
        10,
    )
    sync_phone.blobdb.delete(Database.IOS_NOTIF_PREFS, b"clock24h\0")
    assert not synced, (
        f"an iOS notification pref started a settings sync: {sync_phone.writes(since)}"
    )
