# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Ways of talking to a watch.

A connection is one channel to the firmware, and offers some of three
capabilities: the debug prompt, the log stream, and the Pebble protocol
(what the phone app speaks). A session may open several connections, and
each capability is served by the first one that has it; e.g. the Pebble
protocol through the phone's developer connection, and logs and the prompt
over the debug serial port.

Connections are given as ``SCHEME:ADDRESS``; adding a backend is adding a
:class:`Connection` subclass to a module in this package.
"""

import enum
import importlib
import inspect
import pkgutil
import threading
from abc import ABC, abstractmethod

from harness.errors import HarnessError, Unsupported
from harness.logs import LogRecord


class Capability(enum.Flag):
    NONE = 0
    PROMPT = enum.auto()
    LOGS = enum.auto()
    PROTOCOL = enum.auto()


class Connection(ABC):
    #: The ``SCHEME:`` the connection is selected with.
    scheme = None
    capabilities = Capability.NONE
    help = None
    #: Open after the other connections, which it may need (e.g. a prompt
    #: to confirm pairing on the watch).
    opens_last = False

    def __init__(self, address, dehasher=None):
        self.address = address
        self.dehasher = dehasher
        #: The device this connection belongs to, set before it is opened.
        self.device = None
        self._listeners = []
        self._listeners_lock = threading.Lock()

    def __repr__(self):
        return f"{self.scheme}:{self.address}"

    @abstractmethod
    def open(self, timeout):
        """Connect, waiting up to ``timeout`` seconds for the firmware."""

    @abstractmethod
    def close(self):
        pass

    def reopen(self, timeout):
        """Reconnect after the firmware restarted."""
        self.close()
        self.open(timeout)

    def prompt(self, command, timeout):
        """Run a prompt command, returning its response lines."""
        raise Unsupported(f"{self!r} has no prompt")

    def prompt_no_reply(self, command):
        """Send a prompt command without waiting for its response, for
        commands after which the firmware stops listening."""
        raise Unsupported(f"{self!r} has no prompt")

    @property
    def protocol(self):
        """A connected libpebble2 ``PebbleConnection``."""
        raise Unsupported(f"{self!r} does not carry the Pebble protocol")

    def add_log_listener(self, listener):
        with self._listeners_lock:
            self._listeners.append(listener)

    def remove_log_listener(self, listener):
        with self._listeners_lock:
            if listener in self._listeners:
                self._listeners.remove(listener)

    def emit_log(self, raw, **fields):
        if self.dehasher is not None:
            record = self.dehasher.record(raw, **fields)
        else:
            record = LogRecord(str(raw), **fields)
        with self._listeners_lock:
            listeners = list(self._listeners)
        for listener in listeners:
            listener(record)


# The version response the harness answers with: an Android phone app, with
# every protocol capability.
PHONE_OS_ANDROID = 2
PHONE_CAPABILITIES = 0xFFFFFFFFFFFFFFFF


def start_protocol(transport):
    """A running libpebble2 connection over ``transport``, identified to the
    firmware as the phone app so private endpoints answer."""
    from libpebble2.communication import PebbleConnection
    from libpebble2.protocol.system import AppVersionResponse, PhoneAppVersion

    pebble = PebbleConnection(transport)
    pebble.connect()
    pebble.run_async()
    # The firmware asks only once per session, which outlives host connections.
    pebble.send_packet(
        PhoneAppVersion(
            message=AppVersionResponse(
                protocol_version=0xFFFFFFFF,
                session_caps=0x80000000,
                platform_flags=PHONE_OS_ANDROID,
                response_version=2,
                major_version=3,
                minor_version=0,
                bugfix_version=0,
                protocol_caps=PHONE_CAPABILITIES,
            )
        )
    )
    return pebble


def backends():
    """Every connection class in this package, by scheme."""
    found = {}
    for info in pkgutil.iter_modules(__path__, __name__ + "."):
        module = importlib.import_module(info.name)
        for obj in vars(module).values():
            if (
                inspect.isclass(obj)
                and issubclass(obj, Connection)
                and obj.__module__ == module.__name__
                and not inspect.isabstract(obj)
            ):
                found[obj.scheme] = obj
    return dict(sorted(found.items()))


def create(spec, dehasher=None):
    """The connection described by ``SCHEME:ADDRESS``."""
    scheme, sep, address = spec.partition(":")
    available = backends()
    if not sep or scheme not in available:
        raise HarnessError(
            f"bad connection {spec!r}: expected SCHEME:ADDRESS with SCHEME one of "
            f"{', '.join(available)}"
        )
    return available[scheme](address, dehasher)
