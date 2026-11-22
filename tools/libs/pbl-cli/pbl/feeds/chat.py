# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Recent chats for the Chat app, as the phone app writes them to the chat db
(see pbl/services/blob_db/chat_db.h)."""

import hashlib
import struct
import time

from pbl.feeds import BlobDb, Feed

CHATS = [
    ("Taz Family", True, 2, [
        ("Anna", False, "Dinner at 7?"),
        ("You", True, "Sounds good, I'll bring dessert"),
        ("Ben", False, "Can someone pick up Leo from practice?"),
        ("Anna", False, "I can do it 🚗"),
        ("Ben", False, "Thanks! 🙏"),
    ]),
    ("Maya Chen", False, 1, [
        ("Maya Chen", False, "Are we still on for the hike tomorrow?"),
        ("You", True, "Yes! Trailhead at 8"),
        ("Maya Chen", False, "Perfect, I'll bring coffee ☕"),
    ]),
    ("Climbing Crew", True, 0, [
        ("Sam", False, "Gym tonight?"),
        ("Jo", False, "In, 6:30"),
        ("You", True, "See you there"),
    ]),
]


def _key(text):
    return hashlib.sha256(text.encode()).digest()[:8]


def _str8(text):
    raw = text.encode()[:255]
    return struct.pack("<B", len(raw)) + raw


def _str16(text):
    raw = text.encode()[:256]
    return struct.pack("<H", len(raw)) + raw


def chat_record(title, group, unread, last_activity, synced, preview):
    return (
        struct.pack("<BBHII", 1, 1 if group else 0, unread, last_activity, synced)
        + _str8(title)
        + _str8(preview)
    )


def message_record(msg_id, sender, outgoing, when, text, reactions="", reply=("", "")):
    return (
        struct.pack("<BBI", 1, 1 if outgoing else 0, when)
        + _str8(msg_id)
        + _str8(sender)
        + _str16(text)
        + _str8(reactions)
        + _str8(reply[0])
        + _str8(reply[1])
    )


class Chat(Feed):
    name = "chat"
    help = "A few chats for the Chat app"
    description = "Write three chats with a few messages each, as the phone app would."

    def add_arguments(self, parser):
        parser.add_argument("--clear", action="store_true", help="Remove every chat instead")
        parser.add_argument(
            "--age", type=int, default=0, help="Minutes since the phone synced (default: 0)"
        )

    def run(self, args, watch, inf):
        watch.blobdb_clear(BlobDb.CHAT)
        if args.clear:
            inf("cleared")
            return
        now = int(time.time())
        synced = now - args.age * 60
        watch.blobdb_insert(BlobDb.CHAT, b"F", _str8("react,edit,delete,reply"))
        for n, (title, group, unread, messages) in enumerate(CHATS):
            chat_key = _key(title)
            last = now - n * 3600 - 60
            for i, (sender, outgoing, text) in enumerate(messages):
                when = last - (len(messages) - 1 - i) * 120
                msg_id = str(when * 10)
                reply = ("Anna", "Dinner at 7?") if title == "Taz Family" and i == 1 else ("", "")
                reactions = "👍2" if i == len(messages) - 1 else ""
                watch.blobdb_insert(
                    BlobDb.CHAT,
                    b"M" + chat_key + _key(msg_id),
                    message_record(msg_id, sender, outgoing, when, text, reactions, reply),
                )
            watch.blobdb_insert(
                BlobDb.CHAT,
                b"C" + chat_key,
                chat_record(title, group, unread, last, synced, messages[-1][2]),
            )
        inf(f"pushed {len(CHATS)} chats")
