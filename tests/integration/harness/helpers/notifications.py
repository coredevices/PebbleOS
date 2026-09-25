# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Notifications, inserted into the watch's blob DB as the phone app would."""

import struct
import time
import uuid

from harness.errors import HarnessError

BLOBDB_NOTIFS = 0x04
ANDROID_DATA_SOURCE = uuid.UUID("ed429c16-f674-4220-95da-454f303f15e2")

TITLE, BODY, SENDER, APP_NAME = 1, 3, 12, 30
TYPE_NOTIFICATION = 1
LAYOUT_NOTIFICATION = 4
FLAG_VISIBLE = 1 << 0
ACTION_DISMISS = 0x04


def _attribute(attr_id, text):
    data = text.encode()
    return struct.pack("<BH", attr_id, len(data)) + data


def serialize(
    notification_id, timestamp, body, title=None, sender=None, app="WhatsApp"
):
    """A notification timeline item with a Dismiss action."""
    attributes = [
        _attribute(TITLE, title or sender or app),
        _attribute(BODY, body),
        _attribute(APP_NAME, app),
    ]
    if sender:
        attributes.insert(0, _attribute(SENDER, sender))
    dismiss = struct.pack("<BBB", 0, ACTION_DISMISS, 1) + _attribute(TITLE, "Dismiss")
    payload = b"".join(attributes) + dismiss
    header = struct.pack(
        "<16s16sIHBBBBHBB",
        notification_id.bytes,
        ANDROID_DATA_SOURCE.bytes,
        timestamp,
        0,
        TYPE_NOTIFICATION,
        FLAG_VISIBLE,
        0,
        LAYOUT_NOTIFICATION,
        len(payload),
        len(attributes),
        1,
    )
    return header + payload


class _Key:
    def __init__(self, raw):
        self.bytes = raw


def _blobdb(pebble, method, *args):
    from libpebble2.protocol.blobdb import BlobStatus
    from libpebble2.services.blobdb import BlobDBClient, SyncWrapper

    status = SyncWrapper(getattr(BlobDBClient(pebble), method), *args).wait()
    if status != BlobStatus.Success:
        name = "no response" if status is None else BlobStatus(status).name
        raise HarnessError(f"blob DB {method}: {name}")


def send(dut, body, title=None, sender=None, app="WhatsApp", timestamp=None):
    """Insert a notification, received at ``timestamp`` (default: the host's
    now); returns its id."""
    notification_id = uuid.uuid4()
    when = int(time.time()) if timestamp is None else int(timestamp)
    item = serialize(notification_id, when, body, title, sender, app)
    _blobdb(dut.protocol, "insert", BLOBDB_NOTIFS, notification_id, item)
    return notification_id


def clear(dut):
    _blobdb(dut.protocol, "clear", BLOBDB_NOTIFS)
