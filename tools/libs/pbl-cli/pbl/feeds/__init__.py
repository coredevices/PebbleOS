# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Simulated phone data for the firmware.

A feed writes what the phone app would into the running emulator, so a
feature that depends on the phone can be exercised without one. Every
module in this package that defines :class:`Feed` subclasses contributes a
``pbl feed <name>`` subcommand; adding a feed is adding a file here.
"""

import importlib
import inspect
import pkgutil
from abc import ABC, abstractmethod
from enum import IntEnum

from pbl import util
from pbl.errors import CommandError


class BlobDb(IntEnum):
    """The firmware's blob DB ids (see pbl/services/blob_db/api.h)."""

    TEST = 0x00
    PINS = 0x01
    APPS = 0x02
    REMINDERS = 0x03
    NOTIFS = 0x04
    WEATHER = 0x05
    IOS_NOTIF_PREF = 0x06
    PREFS = 0x07
    CONTACTS = 0x08
    WATCH_APP_PREFS = 0x09
    HEALTH = 0x0A
    APP_GLANCE = 0x0B
    SETTINGS = 0x0C


class _RawKey:
    """A blob DB key that is not a UUID; the client only wants its bytes."""

    def __init__(self, raw):
        self.bytes = raw


class Watch:
    """What a feed writes through: the firmware over a libpebble2 connection,
    or nothing but a log line in a dry run."""

    def __init__(self, pebble=None):
        self._pebble = pebble
        self._blobdb = None

    @property
    def dry_run(self):
        return self._pebble is None

    def _client(self):
        if self._blobdb is None:
            from libpebble2.services.blobdb import BlobDBClient

            self._blobdb = BlobDBClient(self._pebble)
        return self._blobdb

    def _blobdb_call(self, what, method, *args):
        if self.dry_run:
            util.inf("[dry-run]", what, color="yellow")
            return
        from libpebble2.protocol.blobdb import BlobStatus
        from libpebble2.services.blobdb import SyncWrapper

        status = SyncWrapper(getattr(self._client(), method), *args).wait()
        if status is None:
            raise CommandError(f"{what}: no response from the firmware")
        if status != BlobStatus.Success:
            raise CommandError(f"{what}: firmware answered {BlobStatus(status).name}")

    def blobdb_insert(self, database, key, value):
        """Insert ``value`` under ``key`` (a UUID, or raw bytes) in ``database``."""
        if isinstance(key, (bytes, bytearray)):
            key = _RawKey(bytes(key))
        self._blobdb_call(
            f"{database.name}: insert {len(value)} bytes",
            "insert",
            database,
            key,
            value,
        )

    def blobdb_clear(self, database):
        self._blobdb_call(f"{database.name}: clear", "clear", database)

    def send(self, packet, what=None):
        """Send one Pebble protocol packet."""
        if self.dry_run:
            util.inf("[dry-run]", what or type(packet).__name__, color="yellow")
            return
        self._pebble.send_packet(packet)

    def on(self, packet_class, handler):
        """Call ``handler(packet)`` for every ``packet_class`` the firmware sends."""
        if not self.dry_run:
            self._pebble.register_endpoint(packet_class, handler)


class Feed(ABC):
    #: The ``pbl feed <name>`` subcommand.
    name = None
    help = None
    description = None

    def add_arguments(self, parser):
        """Add the feed's own options to its subparser."""

    @abstractmethod
    def run(self, args, watch, inf):
        """Write the feed's data through ``watch``; ``inf`` prints a line."""


def builtin_feeds():
    """Discover and instantiate every feed in this package, by name."""
    feeds = {}
    for info in pkgutil.iter_modules(__path__, __name__ + "."):
        module = importlib.import_module(info.name)
        for obj in vars(module).values():
            if (
                inspect.isclass(obj)
                and issubclass(obj, Feed)
                and obj.__module__ == module.__name__
                and not inspect.isabstract(obj)
            ):
                feed = obj()
                feeds[feed.name] = feed
    return dict(sorted(feeds.items()))
