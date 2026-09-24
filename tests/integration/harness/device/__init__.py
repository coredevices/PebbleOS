# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""The device under test.

A :class:`DeviceAdapter` owns the firmware's lifecycle on one kind of
device (the emulator, real hardware) and the connections to it. Each of
the connections' capabilities (prompt, logs, Pebble protocol) is served by
the first connection that has it.
"""

import contextlib
import logging
import os
import threading
import time
from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from datetime import datetime

from harness import connections
from harness.connections import Capability
from harness.errors import HarnessError, Unsupported, WatchTimeout
from harness.logs import Dehasher, LogBuffer, LogFile

logger = logging.getLogger(__name__)

QUIESCE_SEND_S = 0.5
# The console listens again once the firmware's timer fires.
QUIESCE_MARGIN_S = 2.0


@dataclass
class DeviceConfig:
    build: object
    results_dir: str
    base_timeout: float = 60.0
    connections: list = field(default_factory=list)
    serial: list = field(default_factory=list)
    serial_baud: int = 115200
    flash_before: bool = False
    erase_fs: bool = False
    flash_command: str = None
    qemu_rtc: str = None
    qemu_bt_hci: str = None
    ble_controller: str = None
    power_supply: object = None


class DeviceAdapter(ABC):
    type = None

    def __init__(self, config):
        self.config = config
        self.build = config.build
        self.logs = LogBuffer()
        self.connections = []
        self._dehasher = Dehasher(self.build.loghash_dict)
        #: The Bumble transport of the controller the harness talks to the
        #: watch's Bluetooth with, if any.
        self.ble_controller = config.ble_controller
        self._log_file = None
        self._launched = False
        self._log_listeners = [self.logs]
        self._log_lock = threading.Lock()

    # --- lifecycle ----------------------------------------------------------

    def launch(self):
        """Boot the firmware and connect to it."""
        self.close()
        os.makedirs(self.config.results_dir, exist_ok=True)
        # One session log, across the launches of a session.
        self._log_file = LogFile(
            os.path.join(self.config.results_dir, "device.log"), append=self._launched
        )
        self._launched = True
        self.add_log_listener(self._log_file)
        self._device_launch()
        self.connect()
        self.wait_ready()

    def close(self):
        self.disconnect()
        self._close_device()
        if self._log_file is not None:
            self.remove_log_listener(self._log_file)
            self._log_file.close()
            self._log_file = None

    def add_log_listener(self, listener):
        """Call ``listener(record)`` for every log record, across reconnects."""
        with self._log_lock:
            self._log_listeners.append(listener)

    def remove_log_listener(self, listener):
        with self._log_lock:
            if listener in self._log_listeners:
                self._log_listeners.remove(listener)

    def _on_log(self, record):
        with self._log_lock:
            listeners = list(self._log_listeners)
        for listener in listeners:
            listener(record)

    def connect(self):
        specs = self.config.connections or self.default_connections()
        if not specs:
            raise HarnessError(
                f"no connection to the {self.type} device: pass --device-serial or --connection"
            )
        # Capabilities are served in the given order; some connections need
        # the others to be up first to open.
        created = [connections.create(spec, self._dehasher) for spec in specs]
        self.connections = list(created)
        opened = []
        try:
            for connection in sorted(created, key=lambda c: c.opens_last):
                connection.device = self
                connection.add_log_listener(self._on_log)
                connection.open(self.config.base_timeout)
                opened.append(connection)
        except BaseException:
            for connection in opened:
                connection.close()
            self.connections = []
            raise

    def disconnect(self):
        while self.connections:
            self.connections.pop().close()

    @contextlib.contextmanager
    def quiesce(self, seconds):
        """Have the firmware stop listening on its console for ``seconds``
        (so it can sleep as it would unplugged) and drop every connection
        meanwhile; reconnect once it listens again."""
        self._connection(Capability.PROMPT).prompt_no_reply(
            f"console disable rx {int(seconds)}"
        )
        until = time.monotonic() + seconds
        # Let the command go out before the port closes.
        time.sleep(QUIESCE_SEND_S)
        self.disconnect()
        try:
            yield until
        finally:
            time.sleep(max(until - time.monotonic(), 0) + QUIESCE_MARGIN_S)
            self.connect()
            self.wait_ready()

    def standby(self):
        """Have the firmware turn the watch off (PRF only); :meth:`reset`
        turns it back on."""
        self._connection(Capability.PROMPT).prompt_no_reply("enter standby")
        time.sleep(QUIESCE_SEND_S)
        self.disconnect()

    def wipe(self):
        """Erase the watch's filesystem and boot it afresh: no bondings,
        default settings, no apps or data."""
        raise Unsupported(f"the {self.type} device cannot be wiped")

    def confirm_pairing(self):
        """Accept a pairing request shown on the watch (Up)."""
        if not self.has(Capability.PROMPT):
            logger.warning("confirm the pairing on the watch: press Up")
            return
        self.prompt("click short 1")

    def reset(self):
        """Restart the firmware, and wait until it answers again."""
        if not self._hard_reset():
            try:
                self.prompt("reset", timeout=2)
            except WatchTimeout:
                pass
        self.disconnect()
        self.connect()
        self.wait_ready()

    def wait_ready(self, timeout=None):
        """Wait until the firmware answers on the prompt or the protocol."""
        timeout = timeout or self.config.base_timeout
        deadline = time.monotonic() + timeout
        last_error = None
        while time.monotonic() < deadline:
            try:
                if self.has(Capability.PROMPT):
                    self.prompt("version", timeout=5)
                else:
                    from libpebble2.protocol.system import (
                        WatchVersion,
                        WatchVersionRequest,
                    )

                    self.protocol.send_and_read(
                        WatchVersion(data=WatchVersionRequest()),
                        WatchVersion,
                        timeout=5,
                    )
                return
            except Exception as e:  # noqa: BLE001
                last_error = e
                time.sleep(0.5)
        raise WatchTimeout(
            f"the firmware did not answer within {timeout}s ({last_error})"
        )

    def initialize_log_files(self, test_name):
        header = f"==== Test {test_name} started at {datetime.now().astimezone()} ===="
        if self._log_file is not None:
            self._log_file(header)

    # --- capabilities -------------------------------------------------------

    def has(self, capability):
        return any(capability in c.capabilities for c in self.connections)

    def _connection(self, capability):
        for connection in self.connections:
            if capability in connection.capabilities:
                return connection
        raise Unsupported(
            f"no connection offers {capability.name.lower()}: "
            f"{', '.join(map(repr, self.connections)) or 'none open'}"
        )

    def prompt(self, command, timeout=20):
        """Run a prompt command, returning its response lines."""
        return self._connection(Capability.PROMPT).prompt(command, timeout)

    @property
    def protocol(self):
        """The libpebble2 connection to the firmware."""
        return self._connection(Capability.PROTOCOL).protocol

    def wait_for_log(self, pattern, timeout=None, since=0):
        """The first log record matching ``pattern`` after mark ``since``."""
        self._connection(Capability.LOGS)
        return self.logs.wait_for(pattern, timeout or self.config.base_timeout, since)

    # --- what the device itself offers --------------------------------------

    @abstractmethod
    def default_connections(self):
        """``SCHEME:ADDRESS`` connections to use when none are given."""

    @abstractmethod
    def _device_launch(self):
        pass

    @abstractmethod
    def _close_device(self):
        pass

    def _hard_reset(self):
        """Reset without the firmware's help; False when not possible."""
        return False

    def set_battery(self, percent, charging=False):
        """Have the battery report ``percent``, with a charger or not."""
        raise Unsupported(f"the {self.type} device has no battery to set")

    def screenshot(self):
        """The display as a PIL image, when the device can capture it."""

    def tap(self, x, y):
        return False
