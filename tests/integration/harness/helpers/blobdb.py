# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""The phone's side of the blob DB endpoint, and the timeline items it
stores, for tests that need the watch's exact answers."""

import enum
import itertools
import struct
import uuid

from harness.errors import WatchTimeout

ENDPOINT = 0xB1DB


class Database(enum.IntEnum):
    TEST = 0x00
    PINS = 0x01
    APPS = 0x02
    REMINDERS = 0x03
    NOTIFICATIONS = 0x04
    WEATHER = 0x05
    IOS_NOTIF_PREFS = 0x06
    PREFS = 0x07
    CONTACTS = 0x08
    WATCH_APP_PREFS = 0x09
    HEALTH = 0x0A
    APP_GLANCE = 0x0B
    SETTINGS = 0x0C


class Command(enum.IntEnum):
    INSERT = 0x01
    READ = 0x02
    UPDATE = 0x03
    DELETE = 0x04
    CLEAR = 0x05


class Status(enum.IntEnum):
    SUCCESS = 0x01
    GENERAL_FAILURE = 0x02
    INVALID_OPERATION = 0x03
    INVALID_DATABASE_ID = 0x04
    INVALID_DATA = 0x05
    KEY_DOES_NOT_EXIST = 0x06
    DATABASE_FULL = 0x07
    DATA_STALE = 0x08
    NOT_SUPPORTED = 0x09
    LOCKED = 0x0A
    TRY_LATER = 0x0B


class ItemType(enum.IntEnum):
    NOTIFICATION = 1
    PIN = 2
    REMINDER = 3


class Layout(enum.IntEnum):
    GENERIC = 1
    CALENDAR = 2
    REMINDER = 3
    NOTIFICATION = 4


class Attribute(enum.IntEnum):
    TITLE = 1
    SUBTITLE = 2
    BODY = 3
    APP_NAME = 30
    IMAGE_ASPECT_RATIO = 52


class ActionType(enum.IntEnum):
    GENERIC = 0x02
    DISMISS = 0x04


FLAG_VISIBLE = 1 << 0


def attribute(attr_id, value):
    """A serialized attribute; text is encoded as UTF-8."""
    if isinstance(value, str):
        value = value.encode()
    return struct.pack("<BH", attr_id, len(value)) + value


def timeline_item(
    item_id,
    item_type,
    layout,
    timestamp,
    attributes,
    actions=(),
    parent_id=None,
    duration=0,
):
    """A serialized timeline item. ``attributes`` are ``(id, value)``;
    ``actions`` are ``(action_id, type, [(id, value), ...])``."""
    payload = b"".join(attribute(i, v) for i, v in attributes)
    for action_id, action_type, action_attributes in actions:
        payload += struct.pack("<BBB", action_id, action_type, len(action_attributes))
        payload += b"".join(attribute(i, v) for i, v in action_attributes)
    header = struct.pack(
        "<16s16sIHBHBHBB",
        item_id.bytes,
        (parent_id or uuid.UUID(int=0)).bytes,
        int(timestamp),
        duration,
        item_type,
        FLAG_VISIBLE,
        layout,
        len(payload),
        len(attributes),
        len(actions),
    )
    return header + payload


class BlobDB:
    """Blob DB requests from ``phone``, returning the watch's status."""

    _tokens = itertools.count(0x4000)

    def __init__(self, phone):
        self.phone = phone

    def request(self, command, body, timeout=10.0):
        token = next(self._tokens) & 0xFFFF
        since = self.phone.inbox.mark()
        self.phone.send(ENDPOINT, struct.pack("<BH", command, token) + body)
        try:
            response = self.phone.inbox.wait(
                ENDPOINT,
                lambda p: struct.unpack_from("<H", p)[0] == token,
                timeout,
                since,
            )
        except WatchTimeout:
            raise WatchTimeout(f"no blob DB response to command {command:#x}") from None
        return Status(response[2])

    def insert(self, db, key, value):
        key = getattr(key, "bytes", key)
        body = struct.pack("<BB", db, len(key)) + key + struct.pack("<H", len(value))
        return self.request(Command.INSERT, body + value)

    def delete(self, db, key):
        key = getattr(key, "bytes", key)
        return self.request(Command.DELETE, struct.pack("<BB", db, len(key)) + key)

    def clear(self, db):
        return self.request(Command.CLEAR, struct.pack("<B", db))
