# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""A lab: the hardware wired to this host.

A lab file (YAML) describes the wiring only: watches (board, debug serial
port, debug probe, the supply powering them), power supplies and Bluetooth
dongles. Commands that reach a watch take its ports from the lab when none
are given on the command line, and the integration tests take their setup
from it.
"""

import dataclasses
import os

from pbl import power

ENV = "PBL_LAB"
LEGACY_ENV = "PBL_ITEST_LAB"
DEFAULT_SERIAL_BAUD = 115200


class LabError(Exception):
    pass


def board_matches(board, board_target):
    """Whether a lab's ``board`` covers ``board_target``: the board with its
    revision, or any revision when the lab gives none."""
    if "@" in board:
        return board == board_target
    return board == board_target.partition("@")[0]


@dataclasses.dataclass
class Watch:
    name: str
    board: str
    serial: str = None
    serial_baud: int = DEFAULT_SERIAL_BAUD
    probe: str = None
    supply: str = None

    def matches(self, board_target):
        return board_matches(self.board, board_target)


@dataclasses.dataclass
class Supply:
    name: str
    type: str
    port: str = "auto"


@dataclasses.dataclass
class Dongle:
    """A Bluetooth controller: an nRF52840 dongle running an H4 HCI UART
    firmware, by serial port, or any Bumble transport."""

    name: str
    port: str


@dataclasses.dataclass
class Lab:
    path: str = None
    watches: dict = dataclasses.field(default_factory=dict)
    supplies: dict = dataclasses.field(default_factory=dict)
    dongles: dict = dataclasses.field(default_factory=dict)

    @classmethod
    def load(cls, path):
        import yaml

        try:
            with open(path) as f:
                data = yaml.safe_load(f) or {}
        except OSError as e:
            raise LabError(f"cannot read lab file: {e}") from None
        except yaml.YAMLError as e:
            raise LabError(f"{path}: invalid lab file: {e}") from None
        try:
            return cls.parse(data, path)
        except (AttributeError, KeyError, TypeError, ValueError) as e:
            raise LabError(f"{path}: invalid lab file: {e}") from None

    @classmethod
    def parse(cls, data, path=None):
        unknown = set(data) - {"watches", "supplies", "dongles"}
        if unknown:
            raise ValueError(f"unknown sections {sorted(unknown)}")
        lab = cls(path=path)
        for name, entry in (data.get("supplies") or {}).items():
            lab.supplies[name] = Supply(name=name, **entry)
            if lab.supplies[name].type not in power.TYPES:
                raise ValueError(f"supply {name}: unknown type {entry['type']!r}")
        for name, entry in (data.get("dongles") or {}).items():
            lab.dongles[name] = Dongle(name=name, **entry)
        for name, entry in (data.get("watches") or {}).items():
            lab.watches[name] = Watch(name=name, **entry)
        lab._check()
        return lab

    def _check(self):
        powered = {}
        for watch in self.watches.values():
            if watch.supply is None:
                continue
            if watch.supply not in self.supplies:
                raise ValueError(f"watch {watch.name}: no supply {watch.supply!r}")
            if watch.supply in powered:
                raise ValueError(
                    f"supply {watch.supply} powers both {powered[watch.supply]} "
                    f"and {watch.name}"
                )
            powered[watch.supply] = watch.name

    def watch_for(self, board_target, name=None):
        """The watch running ``board_target``: the one called ``name``, or
        the only one that matches. None when no watch matches."""
        if name is not None:
            if name not in self.watches:
                raise LabError(f"{self.path}: no watch {name!r}")
            watch = self.watches[name]
            if not watch.matches(board_target):
                raise LabError(
                    f"watch {name} is a {watch.board}, the build is for {board_target}"
                )
            return watch
        matching = [w for w in self.watches.values() if w.matches(board_target)]
        if len(matching) > 1:
            names = ", ".join(w.name for w in matching)
            raise LabError(
                f"several watches run {board_target} ({names}), "
                "pick one with --watch"
            )
        return matching[0] if matching else None

    def watch_on(self, port):
        """The watch whose debug serial port is ``port``, or None."""
        for watch in self.watches.values():
            if watch.serial and _same_port(watch.serial, port):
                return watch
        return None

    def dongle_ports(self):
        return [dongle.port for dongle in self.dongles.values()]


def _same_port(a, b):
    # macOS has a tty.* and a cu.* node for each port
    def norm(port):
        head, _, tail = port.rpartition("/")
        return head, tail.removeprefix("tty.").removeprefix("cu.")

    return norm(a) == norm(b)


def default_path():
    """The lab file of $PBL_LAB (or $PBL_ITEST_LAB), else the user's, if
    there is one."""
    path = os.environ.get(ENV) or os.environ.get(LEGACY_ENV)
    if path:
        return path
    config = os.environ.get("XDG_CONFIG_HOME") or os.path.expanduser("~/.config")
    path = os.path.join(config, "pbl", "lab.yaml")
    return path if os.path.isfile(path) else None


def port_present(port):
    """Whether a serial port is there: True or False for a device path,
    None when it cannot tell (``auto``, sockets, Bumble transports)."""
    if port and port.startswith("/dev/"):
        return os.path.exists(port)
    return None
