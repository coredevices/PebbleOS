# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Calendar events, as the phone app syncs them: one calendar pin per event
in the pins blob DB, keyed by the event."""

import calendar
import struct
import time
import uuid

from pbl.feeds import BlobDb, Feed

EVENT_NAMESPACE = uuid.UUID("9f1d1c6a-0b2e-4b4a-8d3f-6e5c4a3b2c1d")
CALENDAR_DATA_SOURCE = uuid.UUID("6c6c6fc2-1912-4d25-8396-3547d1dfac5b")

# AttributeId
TITLE, BODY, LOCATION_NAME, DISPLAY_RECURRING = 1, 3, 11, 31
LAYOUT_CALENDAR = 2
TYPE_PIN = 2
FLAG_VISIBLE, FLAG_ALL_DAY = 1 << 0, 1 << 2
RECURRING = 1

MINUTES_PER_DAY = 24 * 60

# A day around now: when each event starts (minutes from now, or "all-day"),
# how long it is, and what it says.
EVENTS = [
    {
        "start": -90,
        "minutes": 60,
        "title": "Standup",
        "location": "Room 2B",
        "recurring": True,
    },
    {
        "start": -15,
        "minutes": 45,
        "title": "Design review: timeline cards",
        "location": "Video call",
        "body": "Bring the round display mockups.",
    },
    {
        "start": 30,
        "minutes": 80,
        "title": "Gen Phys 7A-201(MS11) Lecture",
    },
    {
        "start": 150,
        "minutes": 30,
        "title": "Dentist",
        "location": "Dr. Vidal, Carrer de Mallorca 123, Barcelona",
        "body": "Bring the insurance card.",
    },
    {
        "start": 300,
        "minutes": 90,
        "title": "Quarterly planning with the whole hardware and firmware team",
        "location": "Main office",
        "recurring": True,
    },
    {
        "start": "all-day",
        "title": "Company offsite planning day for the whole team",
    },
    {
        "start": MINUTES_PER_DAY + 60,
        "minutes": 60,
        "title": "Flight to Tokyo",
        "location": "BCN T1",
    },
]


def _attribute(attr_id, content):
    return struct.pack("<BH", attr_id, len(content)) + content


def pin(event_id, timestamp, minutes, flags, attributes):
    """A serialized calendar pin (SerializedTimelineItemHeader + attributes)."""
    payload = b"".join(attributes)
    header = struct.pack(
        "<16s16sIHBHBHBB",
        event_id.bytes,
        CALENDAR_DATA_SOURCE.bytes,
        timestamp,
        minutes,
        TYPE_PIN,
        flags,
        LAYOUT_CALENDAR,
        len(payload),
        len(attributes),
        0,
    )
    return header + payload


def event_pin(event, now):
    """The pin for one event, as the phone would sync it now."""
    attributes = [_attribute(TITLE, event["title"].encode())]
    if event.get("location"):
        attributes.append(_attribute(LOCATION_NAME, event["location"].encode()))
    if event.get("body"):
        attributes.append(_attribute(BODY, event["body"].encode()))
    if event.get("recurring"):
        attributes.append(_attribute(DISPLAY_RECURRING, bytes([RECURRING])))

    flags = FLAG_VISIBLE
    if event["start"] == "all-day":
        # All-day pins carry local midnight as if it were UTC; the firmware
        # shifts it by the watch's timezone.
        local = time.localtime(now)
        timestamp = calendar.timegm(
            (local.tm_year, local.tm_mon, local.tm_mday, 0, 0, 0)
        )
        minutes = MINUTES_PER_DAY
        flags |= FLAG_ALL_DAY
    else:
        timestamp = int(now) + event["start"] * 60
        minutes = event["minutes"]
    return pin(event_key(event["title"]), timestamp, minutes, flags, attributes)


def event_key(title):
    return uuid.uuid5(EVENT_NAMESPACE, title)


class Calendar(Feed):
    name = "calendar"
    help = "A day of calendar events"
    description = (
        "Write a day of events around now into the timeline, or a single "
        "event of your own."
    )

    def add_arguments(self, parser):
        parser.add_argument(
            "--title", help="Push a single event with this title instead"
        )
        parser.add_argument("--location", help="Where it takes place")
        parser.add_argument("--body", help="Its description")
        parser.add_argument(
            "--recurring", action="store_true", help="Mark it as recurring"
        )
        parser.add_argument(
            "--all-day", action="store_true", help="Make it an all-day event today"
        )
        parser.add_argument(
            "--start",
            type=int,
            default=30,
            metavar="MINUTES",
            help="Minutes from now it starts, negative for an ongoing one (default: 30)",
        )
        parser.add_argument(
            "--duration",
            type=int,
            default=60,
            metavar="MINUTES",
            help="How long it lasts (default: 60)",
        )
        parser.add_argument(
            "--clear",
            action="store_true",
            help="Remove every pin instead, reminders and other sources included",
        )

    def run(self, args, watch, inf):
        if args.clear:
            watch.blobdb_clear(BlobDb.PINS)
            inf("cleared every pin")
            return

        if args.title:
            events = [
                {
                    "start": "all-day" if args.all_day else args.start,
                    "minutes": args.duration,
                    "title": args.title,
                    "location": args.location,
                    "body": args.body,
                    "recurring": args.recurring,
                }
            ]
        else:
            events = EVENTS

        now = time.time()
        for event in events:
            watch.blobdb_insert(
                BlobDb.PINS, event_key(event["title"]), event_pin(event, now)
            )
        inf(f"pushed {len(events)} event{'s' if len(events) != 1 else ''}")
