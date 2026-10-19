# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""The GATT services an iPhone hosts for the watch: the Apple Notification
Center Service (:class:`ANCS`), the Apple Media Service (:class:`AMS`) and
Device Information (:class:`DeviceInformation`, which tells the watch it is
iOS 9 or newer).

Each is handed to :class:`~harness.ble.BleLink` as one of its ``services``.
Their methods are called from the test's thread; what the watch writes is
answered on the link's loop and kept for the test to wait on.
"""

import asyncio
import dataclasses
import enum
import logging
import struct
import threading
import time

from harness.errors import HarnessError, WatchTimeout

logger = logging.getLogger(__name__)

ANCS_SERVICE = "7905F431-B5CE-4E99-A40F-4B1E122D00D0"
ANCS_NOTIFICATION_SOURCE = "9FBF120D-6301-42D9-8C58-25E699A21DBD"
ANCS_CONTROL_POINT = "69D1D8F3-45E1-49A8-9821-9BBDFDAAD9D9"
ANCS_DATA_SOURCE = "22EAC6E9-24D6-4BB5-BE44-B36ACE7C7BFB"

AMS_SERVICE = "89D3502B-0F36-433A-8EF4-C502AD55F8DC"
AMS_REMOTE_COMMAND = "9B3C81D8-57B1-4A8A-B8DF-0E56F7CA51C2"
AMS_ENTITY_UPDATE = "2F7CABCE-808D-411F-9A0C-BB92BA96C102"
AMS_ENTITY_ATTRIBUTE = "C6B2F38C-23AB-46D8-A6AB-A3A870BBD5D7"

DIS_SERVICE = "180A"
DIS_MANUFACTURER_NAME = "2A29"
DIS_MODEL_NUMBER = "2A24"

# ATT notification header.
NOTIFICATION_HEADER = 3


class Category(enum.IntEnum):
    OTHER = 0
    INCOMING_CALL = 1
    MISSED_CALL = 2
    VOICEMAIL = 3
    SOCIAL = 4
    SCHEDULE = 5
    EMAIL = 6
    NEWS = 7
    HEALTH_AND_FITNESS = 8
    BUSINESS_AND_FINANCE = 9
    LOCATION = 10
    ENTERTAINMENT = 11


class EventFlag(enum.IntFlag):
    SILENT = 1 << 0
    IMPORTANT = 1 << 1
    PRE_EXISTING = 1 << 2
    POSITIVE_ACTION = 1 << 3
    NEGATIVE_ACTION = 1 << 4


class Action(enum.IntEnum):
    POSITIVE = 0
    NEGATIVE = 1


class Command(enum.IntEnum):
    GET_NOTIFICATION_ATTRIBUTES = 0
    GET_APP_ATTRIBUTES = 1
    PERFORM_NOTIFICATION_ACTION = 2


class _Event(enum.IntEnum):
    ADDED = 0
    MODIFIED = 1
    REMOVED = 2


class _Attribute(enum.IntEnum):
    APP_IDENTIFIER = 0
    TITLE = 1
    SUBTITLE = 2
    MESSAGE = 3
    MESSAGE_SIZE = 4
    DATE = 5
    POSITIVE_ACTION_LABEL = 6
    NEGATIVE_ACTION_LABEL = 7


# Attributes the request follows with a maximum length.
_SIZED_ATTRIBUTES = {_Attribute.TITLE, _Attribute.SUBTITLE, _Attribute.MESSAGE}
APP_DISPLAY_NAME = 0

#: ANCS's ATT errors.
ANCS_UNKNOWN_COMMAND = 0xA0
ANCS_INVALID_COMMAND = 0xA1
ANCS_INVALID_PARAMETER = 0xA2
ANCS_ACTION_FAILED = 0xA3
#: The error iOS gives once the user no longer lets the watch see
#: notifications.
WRITE_NOT_PERMITTED = 0x03

#: Display names of the apps posting notifications, as iOS has them.
APP_NAMES = {
    "com.apple.MobileSMS": "Messages",
    "com.apple.mobilephone": "Phone",
    "com.apple.mobilemail": "Mail",
    "com.apple.mobilecal": "Calendar",
    "net.whatsapp.WhatsApp": "WhatsApp",
}


class Recorder:
    """What the watch did, in order; :meth:`mark` and :meth:`wait` work as
    the device log's do."""

    def __init__(self):
        self._items = []
        self._cond = threading.Condition()

    def append(self, item):
        with self._cond:
            self._items.append(item)
            self._cond.notify_all()

    def mark(self):
        with self._cond:
            return len(self._items)

    def since(self, mark=0):
        with self._cond:
            return list(self._items[mark:])

    def wait(self, match=None, timeout=10.0, since=0, what="the watch"):
        """The first item after mark ``since`` that ``match`` accepts."""
        deadline = time.monotonic() + timeout
        with self._cond:
            while True:
                for item in self._items[since:]:
                    if match is None or match(item):
                        return item
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    raise WatchTimeout(
                        f"nothing expected from {what} within {timeout}s"
                    )
                self._cond.wait(remaining)


class _Server:
    """A service of the phone's GATT server, on a :class:`BleLink`."""

    def __init__(self):
        self.link = None
        self._subscribed = {}

    def bind(self, link):
        self.link = link

    def disconnected(self):
        for event in self._subscribed.values():
            event.clear()

    def service(self):
        raise NotImplementedError

    def _track_subscription(self, characteristic, name):
        event = self._subscribed[name] = threading.Event()

        def on_subscription(bearer, notify, indicate):
            logger.info(
                "%s: %s %ssubscribed", type(self).__name__, name, "" if notify else "un"
            )
            if notify:
                event.set()
            else:
                event.clear()

        characteristic.on("subscription", on_subscription)

    def wait_subscribed(self, timeout=15.0):
        """Wait until the watch subscribed to every characteristic it
        listens on."""
        deadline = time.monotonic() + timeout
        for name, event in self._subscribed.items():
            if not event.wait(max(0.0, deadline - time.monotonic())):
                raise WatchTimeout(
                    f"the watch did not subscribe to {type(self).__name__} {name}"
                )

    def _notify(self, characteristic, value):
        """Notify ``value`` from the link's loop."""
        link = self.link
        if link is None or not link.is_linked:
            return
        link.call_soon(link.notify, characteristic, value)


@dataclasses.dataclass
class Notification:
    uid: int
    app_id: str
    title: str = ""
    subtitle: str = ""
    message: str = ""
    #: When it was posted (UNIX time), in the phone's timezone, which the
    #: watch takes for its own.
    date: int | None = None
    category: Category = Category.OTHER
    flags: int = 0
    positive_action: str | None = None
    negative_action: str | None = None

    def attribute(self, attribute_id):
        if attribute_id == _Attribute.APP_IDENTIFIER:
            return self.app_id
        if attribute_id == _Attribute.TITLE:
            return self.title
        if attribute_id == _Attribute.SUBTITLE:
            return self.subtitle
        if attribute_id == _Attribute.MESSAGE:
            return self.message
        if attribute_id == _Attribute.MESSAGE_SIZE:
            return str(len(self.message.encode()))
        if attribute_id == _Attribute.DATE:
            if self.date is None:
                return ""
            return time.strftime("%Y%m%dT%H%M%S", time.gmtime(self.date))
        if attribute_id == _Attribute.POSITIVE_ACTION_LABEL:
            return self.positive_action or ""
        if attribute_id == _Attribute.NEGATIVE_ACTION_LABEL:
            return self.negative_action or ""
        return ""

    @property
    def event_flags(self):
        flags = self.flags
        if self.positive_action:
            flags |= EventFlag.POSITIVE_ACTION
        if self.negative_action:
            flags |= EventFlag.NEGATIVE_ACTION
        return flags


@dataclasses.dataclass
class Request:
    """A Control Point command the watch wrote, as parsed."""

    command: int
    uid: int | None = None
    app_id: str | None = None
    attributes: list = dataclasses.field(default_factory=list)
    action: int | None = None
    #: The ATT error it was answered with, None when accepted.
    error: int | None = None


def _truncate(text, max_length):
    data = text.encode()
    if max_length is None or len(data) <= max_length:
        return data
    return data[:max_length].decode(errors="ignore").encode()


class ANCS(_Server):
    """The phone's notifications. :meth:`post` one, then :meth:`modify` or
    :meth:`remove` it; the watch fetches what it shows over the Control
    Point. ``requests`` keeps every command the watch wrote and ``actions``
    the notification actions among them. ``clock()`` gives the phone's time
    for notifications posted without a date. ``control_point_error``, when
    set, is the ATT error every Control Point write is refused with."""

    def __init__(self, clock=None):
        super().__init__()
        self.clock = clock
        self.app_names = dict(APP_NAMES)
        self.control_point_error = None
        self.requests = Recorder()
        self._notifications = {}
        self._next_uid = 1
        self._lock = threading.Lock()
        self._notification_source = None
        self._data_source = None

    def service(self):
        from bumble.gatt import Characteristic, CharacteristicValue, Service

        self._notification_source = Characteristic(
            ANCS_NOTIFICATION_SOURCE,
            Characteristic.NOTIFY,
            Characteristic.READ_REQUIRES_ENCRYPTION,
            b"",
        )
        control_point = Characteristic(
            ANCS_CONTROL_POINT,
            Characteristic.WRITE,
            Characteristic.WRITE_REQUIRES_ENCRYPTION,
            CharacteristicValue(write=self._on_control_point),
        )
        self._data_source = Characteristic(
            ANCS_DATA_SOURCE,
            Characteristic.NOTIFY,
            Characteristic.READ_REQUIRES_ENCRYPTION,
            b"",
        )
        self._track_subscription(self._notification_source, "Notification Source")
        self._track_subscription(self._data_source, "Data Source")
        return Service(
            ANCS_SERVICE, [self._notification_source, control_point, self._data_source]
        )

    # --- the phone's side ---------------------------------------------------

    def post(
        self,
        message,
        title="",
        app_id="com.apple.MobileSMS",
        subtitle="",
        date=None,
        category=Category.SOCIAL,
        flags=0,
        positive_action=None,
        negative_action=None,
    ):
        """Post a notification; returns it, ``uid`` naming it."""
        if date is None and self.clock is not None:
            date = self.clock()
        with self._lock:
            uid = self._next_uid
            self._next_uid += 1
            notification = Notification(
                uid,
                app_id,
                title,
                subtitle,
                message,
                date,
                Category(category),
                EventFlag(flags),
                positive_action,
                negative_action,
            )
            self._notifications[uid] = notification
        self._send_event(_Event.ADDED, notification)
        return notification

    def modify(self, uid, **changes):
        """Change a posted notification's fields and tell the watch."""
        with self._lock:
            notification = self._notifications[uid]
            for field, value in changes.items():
                if not hasattr(notification, field) or field == "uid":
                    raise HarnessError(f"a notification has no field {field!r}")
                setattr(notification, field, value)
        self._send_event(_Event.MODIFIED, notification)
        return notification

    def remove(self, uid):
        """Withdraw a notification, as when it is cleared on the phone."""
        with self._lock:
            notification = self._notifications.pop(uid)
        self._send_event(_Event.REMOVED, notification)

    def wait_action(self, uid=None, timeout=10.0, since=0):
        """The first notification action the watch performed after mark
        ``since`` (on ``uid``, when given)."""
        return self.requests.wait(
            lambda r: (
                r.command == Command.PERFORM_NOTIFICATION_ACTION
                and (uid is None or r.uid == uid)
            ),
            timeout,
            since,
            what="the watch's notification action",
        )

    def wait_fetched(self, uid, timeout=10.0, since=0):
        """Wait until the watch fetched the attributes of notification
        ``uid``."""
        return self.requests.wait(
            lambda r: (
                r.command == Command.GET_NOTIFICATION_ATTRIBUTES
                and r.uid == uid
                and r.error is None
            ),
            timeout,
            since,
            what=f"the watch fetching notification {uid}",
        )

    def _send_event(self, event, notification):
        with self._lock:
            count = sum(
                1
                for n in self._notifications.values()
                if n.category == notification.category
            )
        value = struct.pack(
            "<BBBBI",
            event,
            notification.event_flags,
            notification.category,
            min(count, 0xFF),
            notification.uid,
        )
        self._notify(self._notification_source, value)

    # --- the watch's side ---------------------------------------------------

    def _on_control_point(self, connection, value):
        from bumble.att import ATT_Error

        value = bytes(value)
        request = self._parse(value)
        if request.error is None and self.control_point_error is not None:
            request.error = self.control_point_error
        if request.error is None:
            response = self._respond(request)
            if response is None:
                request.error = ANCS_INVALID_PARAMETER
        logger.info("ANCS: %s", request)
        self.requests.append(request)
        if request.error is not None:
            raise ATT_Error(request.error)
        if response:
            asyncio.ensure_future(self._send_data_source(response))

    @staticmethod
    def _parse(value):
        if not value:
            return Request(command=-1, error=ANCS_UNKNOWN_COMMAND)
        command = value[0]
        try:
            if command == Command.GET_NOTIFICATION_ATTRIBUTES:
                (uid,) = struct.unpack_from("<I", value, 1)
                attributes = []
                offset = 5
                while offset < len(value):
                    attribute = value[offset]
                    offset += 1
                    max_length = None
                    if attribute in _SIZED_ATTRIBUTES:
                        (max_length,) = struct.unpack_from("<H", value, offset)
                        offset += 2
                    attributes.append((attribute, max_length))
                return Request(command, uid=uid, attributes=attributes)
            if command == Command.GET_APP_ATTRIBUTES:
                end = value.index(0, 1)
                return Request(
                    command,
                    app_id=value[1:end].decode(),
                    attributes=[(a, None) for a in value[end + 1 :]],
                )
            if command == Command.PERFORM_NOTIFICATION_ACTION:
                uid, action = struct.unpack_from("<IB", value, 1)
                return Request(command, uid=uid, action=action)
        except (struct.error, ValueError, UnicodeDecodeError):
            return Request(command, error=ANCS_INVALID_COMMAND)
        return Request(command, error=ANCS_UNKNOWN_COMMAND)

    def _respond(self, request):
        """The Data Source response to ``request``: b"" for none, None when
        it names no notification."""
        if request.command == Command.GET_APP_ATTRIBUTES:
            response = bytes([request.command]) + request.app_id.encode() + b"\0"
            for attribute, _ in request.attributes:
                text = (
                    self.app_names.get(request.app_id, request.app_id)
                    if attribute == APP_DISPLAY_NAME
                    else ""
                )
                data = text.encode()
                response += struct.pack("<BH", attribute, len(data)) + data
            return response
        with self._lock:
            notification = self._notifications.get(request.uid)
        if notification is None:
            return None
        if request.command == Command.PERFORM_NOTIFICATION_ACTION:
            return b""
        response = struct.pack("<BI", request.command, request.uid)
        for attribute, max_length in request.attributes:
            data = _truncate(notification.attribute(attribute), max_length)
            response += struct.pack("<BH", attribute, len(data)) + data
        return response

    async def _send_data_source(self, response):
        chunk = self.link.att_payload
        for offset in range(0, len(response), chunk):
            await self.link.notify(self._data_source, response[offset : offset + chunk])


class PlaybackState(enum.IntEnum):
    PAUSED = 0
    PLAYING = 1
    REWINDING = 2
    FAST_FORWARDING = 3


class RemoteCommand(enum.IntEnum):
    PLAY = 0
    PAUSE = 1
    TOGGLE_PLAY_PAUSE = 2
    NEXT_TRACK = 3
    PREVIOUS_TRACK = 4
    VOLUME_UP = 5
    VOLUME_DOWN = 6
    ADVANCE_REPEAT_MODE = 7
    ADVANCE_SHUFFLE_MODE = 8
    SKIP_FORWARD = 9
    SKIP_BACKWARD = 10
    LIKE = 11
    DISLIKE = 12
    BOOKMARK = 13


class Entity(enum.IntEnum):
    PLAYER = 0
    QUEUE = 1
    TRACK = 2


class PlayerAttribute(enum.IntEnum):
    NAME = 0
    PLAYBACK_INFO = 1
    VOLUME = 2
    BUNDLE_IDENTIFIER = 3


class QueueAttribute(enum.IntEnum):
    INDEX = 0
    COUNT = 1
    SHUFFLE_MODE = 2
    REPEAT_MODE = 3


class TrackAttribute(enum.IntEnum):
    ARTIST = 0
    ALBUM = 1
    TITLE = 2
    DURATION = 3


_ATTRIBUTES = {
    Entity.PLAYER: PlayerAttribute,
    Entity.QUEUE: QueueAttribute,
    Entity.TRACK: TrackAttribute,
}

#: AMS's ATT errors.
AMS_INVALID_STATE = 0xA0
AMS_INVALID_COMMAND = 0xA1
AMS_ABSENT_ATTRIBUTE = 0xA2

ENTITY_UPDATE_TRUNCATED = 0x01


class AMS(_Server):
    """The phone's media player. Set what plays with :meth:`play`,
    :meth:`set_playback`, :meth:`set_volume` and :meth:`set_queue`; the watch
    hears of it for the attributes it registered for (``registrations``).
    ``commands`` keeps the remote commands it sent; those outside
    ``supported_commands`` are refused."""

    def __init__(self):
        super().__init__()
        self.supported_commands = set(RemoteCommand)
        self.commands = Recorder()
        self.registrations = Recorder()
        self._values = {}
        self._registered = {}
        self._selected = None
        self._lock = threading.Lock()
        self._remote_command = None
        self._entity_update = None
        self.set_volume(0.5)
        self.set_playback(PlaybackState.PAUSED)
        self.set_queue()

    def service(self):
        from bumble.gatt import Characteristic, CharacteristicValue, Service

        self._remote_command = Characteristic(
            AMS_REMOTE_COMMAND,
            Characteristic.WRITE | Characteristic.NOTIFY,
            Characteristic.WRITE_REQUIRES_ENCRYPTION,
            CharacteristicValue(write=self._on_remote_command),
        )
        self._entity_update = Characteristic(
            AMS_ENTITY_UPDATE,
            Characteristic.WRITE | Characteristic.NOTIFY,
            Characteristic.WRITE_REQUIRES_ENCRYPTION,
            CharacteristicValue(write=self._on_entity_update),
        )
        entity_attribute = Characteristic(
            AMS_ENTITY_ATTRIBUTE,
            Characteristic.READ | Characteristic.WRITE,
            Characteristic.READ_REQUIRES_ENCRYPTION
            | Characteristic.WRITE_REQUIRES_ENCRYPTION,
            CharacteristicValue(
                read=self._on_entity_attribute_read,
                write=self._on_entity_attribute_write,
            ),
        )
        self._track_subscription(self._entity_update, "Entity Update")
        return Service(
            AMS_SERVICE, [self._remote_command, self._entity_update, entity_attribute]
        )

    def disconnected(self):
        super().disconnected()
        with self._lock:
            self._registered.clear()

    # --- the phone's side ---------------------------------------------------

    def set(self, entity, attribute, value):
        """Set one attribute, as its string value, and tell the watch if it
        registered for it."""
        entity = Entity(entity)
        attribute = _ATTRIBUTES[entity](attribute)
        with self._lock:
            self._values[(entity, attribute)] = value
            registered = attribute in self._registered.get(entity, ())
        if registered:
            self._notify(self._entity_update, self._update(entity, attribute, value))

    def play(self, title, artist="", album="", duration=0.0, elapsed=0.0, rate=1.0):
        """Play a track: ``duration`` and ``elapsed`` in seconds."""
        self.set_track(title, artist, album, duration)
        self.set_playback(PlaybackState.PLAYING, rate, elapsed)

    def set_track(self, title, artist="", album="", duration=0.0):
        self.set(Entity.TRACK, TrackAttribute.ARTIST, artist)
        self.set(Entity.TRACK, TrackAttribute.ALBUM, album)
        self.set(Entity.TRACK, TrackAttribute.TITLE, title)
        self.set(Entity.TRACK, TrackAttribute.DURATION, f"{duration:.3f}")

    def set_playback(self, state, rate=None, elapsed=0.0):
        """``rate`` defaults to 1 while playing, 0 otherwise."""
        if rate is None:
            rate = 1.0 if state == PlaybackState.PLAYING else 0.0
        self.set(
            Entity.PLAYER,
            PlayerAttribute.PLAYBACK_INFO,
            f"{int(state)},{rate:.1f},{elapsed:.3f}",
        )

    def set_volume(self, volume):
        """``volume`` from 0 to 1."""
        self.set(Entity.PLAYER, PlayerAttribute.VOLUME, f"{volume:.2f}")

    def set_queue(self, index=0, count=1, shuffle=0, repeat=0):
        self.set(Entity.QUEUE, QueueAttribute.INDEX, str(index))
        self.set(Entity.QUEUE, QueueAttribute.COUNT, str(count))
        self.set(Entity.QUEUE, QueueAttribute.SHUFFLE_MODE, str(shuffle))
        self.set(Entity.QUEUE, QueueAttribute.REPEAT_MODE, str(repeat))

    def wait_registered(self, entities=tuple(Entity), timeout=15.0):
        """Wait until the watch registered for updates of ``entities``."""
        deadline = time.monotonic() + timeout
        for entity in entities:
            self.registrations.wait(
                lambda r, e=entity: r[0] == e,
                max(0.0, deadline - time.monotonic()),
                what=f"the watch registering for {Entity(entity).name} updates",
            )

    def wait_command(self, command=None, timeout=10.0, since=0):
        """The first remote command the watch sent after mark ``since``
        (``command``, when given)."""
        return self.commands.wait(
            lambda c: command is None or c == command,
            timeout,
            since,
            what="the watch's remote command",
        )

    # --- the watch's side ---------------------------------------------------

    def _update(self, entity, attribute, value):
        data = value.encode()
        room = self.link.att_payload - 3 if self.link and self.link.is_linked else None
        flags = 0
        if room is not None and len(data) > room:
            data = _truncate(value, room)
            flags |= ENTITY_UPDATE_TRUNCATED
        return bytes([entity, attribute, flags]) + data

    def _on_entity_update(self, connection, value):
        from bumble.att import ATT_Error

        value = bytes(value)
        if not value or value[0] not in _ATTRIBUTES:
            raise ATT_Error(AMS_INVALID_COMMAND)
        entity = Entity(value[0])
        try:
            attributes = [_ATTRIBUTES[entity](a) for a in value[1:]]
        except ValueError:
            raise ATT_Error(AMS_INVALID_COMMAND) from None
        logger.info(
            "AMS: registered for %s %s", entity.name, [a.name for a in attributes]
        )
        with self._lock:
            self._registered[entity] = attributes
            updates = [
                self._update(entity, a, self._values[(entity, a)])
                for a in attributes
                if (entity, a) in self._values
            ]
        self.registrations.append((entity, attributes))
        for update in updates:
            asyncio.ensure_future(self.link.notify(self._entity_update, update))

    def _on_remote_command(self, connection, value):
        from bumble.att import ATT_Error

        value = bytes(value)
        if len(value) != 1:
            raise ATT_Error(AMS_INVALID_COMMAND)
        try:
            command = RemoteCommand(value[0])
        except ValueError:
            raise ATT_Error(AMS_INVALID_COMMAND) from None
        logger.info("AMS: remote command %s", command.name)
        self.commands.append(command)
        if command not in self.supported_commands:
            raise ATT_Error(AMS_INVALID_COMMAND)

    def _on_entity_attribute_write(self, connection, value):
        from bumble.att import ATT_Error

        value = bytes(value)
        try:
            entity = Entity(value[0])
            attribute = _ATTRIBUTES[entity](value[1])
        except (IndexError, ValueError):
            raise ATT_Error(AMS_INVALID_COMMAND) from None
        self._selected = (entity, attribute)

    def _on_entity_attribute_read(self, connection):
        from bumble.att import ATT_Error

        with self._lock:
            if self._selected not in self._values:
                raise ATT_Error(AMS_ABSENT_ATTRIBUTE)
            return self._values[self._selected].encode()


class DeviceInformation(_Server):
    """The phone's Device Information service."""

    def __init__(self, manufacturer="Apple Inc.", model="iPhone17,1"):
        super().__init__()
        self.manufacturer = manufacturer
        self.model = model

    def service(self):
        from bumble.gatt import Characteristic, Service

        return Service(
            DIS_SERVICE,
            [
                Characteristic(
                    DIS_MANUFACTURER_NAME,
                    Characteristic.READ,
                    Characteristic.READABLE,
                    self.manufacturer.encode(),
                ),
                Characteristic(
                    DIS_MODEL_NUMBER,
                    Characteristic.READ,
                    Characteristic.READABLE,
                    self.model.encode(),
                ),
            ],
        )
