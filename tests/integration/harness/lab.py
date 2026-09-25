# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""A lab: the hardware wired to the host running the tests, and the setup a
build gets from it.

A lab file (YAML) describes the wiring only: watches (board, debug serial
port, the supply powering them), power supplies and Bluetooth dongles. The
setup for a build is what the tests run with: the device, how it is
reached and powered, and what plays the phone, from the lab and the
command line. Tests only see the setup, so they run unchanged on any of:

- the emulator, with Bumble's software controllers for its Bluetooth and
  the phone;
- the emulator, with a dongle for its Bluetooth and another for the phone;
- a watch on its debug serial port, with the phone on a dongle;
- a watch on its debug serial port, with a phone running CoreApp (not
  supported yet).
"""

import dataclasses
import os

from harness.errors import HarnessError

#: The emulator's Bluetooth through Bumble's software controllers, which
#: also give the phone one.
VIRTUAL = "virtual"
#: The emulator's Bluetooth through the lab's first dongle, the phone's
#: through its second.
FROM_LAB = "lab"

PHONE_BUMBLE = "bumble"
PHONE_COREAPP = "coreapp"
PHONE_TYPES = (PHONE_BUMBLE, PHONE_COREAPP)

SUPPLY_PPK2 = "ppk2"
DEFAULT_VOLTAGE_MV = 3800
DEFAULT_SERIAL_BAUD = 115200


@dataclasses.dataclass
class Watch:
    name: str
    board: str
    serial: str
    serial_baud: int = DEFAULT_SERIAL_BAUD
    supply: str = None

    def matches(self, build):
        """Whether it runs ``build``: the board with its revision, or any
        revision when the lab gives none."""
        if "@" in self.board:
            return self.board == build.board_target
        return self.board == build.board


@dataclasses.dataclass
class Supply:
    name: str
    type: str
    port: str = "auto"


@dataclasses.dataclass
class Dongle:
    """A Bluetooth controller: an nRF52840 dongle running Zephyr's hci_uart,
    by serial port, or any Bumble transport."""

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

        with open(path) as f:
            data = yaml.safe_load(f) or {}
        try:
            return cls.parse(data, path)
        except (KeyError, TypeError, ValueError) as e:
            raise HarnessError(f"{path}: invalid lab file: {e}") from None

    @classmethod
    def parse(cls, data, path=None):
        unknown = set(data) - {"watches", "supplies", "dongles"}
        if unknown:
            raise ValueError(f"unknown sections {sorted(unknown)}")
        lab = cls(path=path)
        for name, entry in (data.get("supplies") or {}).items():
            lab.supplies[name] = Supply(name=name, **entry)
            if lab.supplies[name].type != SUPPLY_PPK2:
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

    def watch_for(self, build, name=None):
        if name is not None:
            if name not in self.watches:
                raise HarnessError(f"{self.path}: no watch {name!r}")
            watch = self.watches[name]
            if not watch.matches(build):
                raise HarnessError(
                    f"watch {name} is a {watch.board}, the build is for "
                    f"{build.board_target}"
                )
            return watch
        for watch in self.watches.values():
            if watch.matches(build):
                return watch
        return None

    def dongle_ports(self):
        return [dongle.port for dongle in self.dongles.values()]


@dataclasses.dataclass
class PhoneSetup:
    """What plays the phone: PHONE_BUMBLE, the harness on a Bluetooth
    controller (a Bumble transport), or PHONE_COREAPP."""

    type: str
    controller: str = None

    def __str__(self):
        return f"{self.type} on {self.controller}" if self.controller else self.type


@dataclasses.dataclass
class Setup:
    """What the tests run with."""

    device_type: str
    watch: str = None
    serial: list = dataclasses.field(default_factory=list)
    serial_baud: int = DEFAULT_SERIAL_BAUD
    ppk2: str = None
    voltage_mv: int = DEFAULT_VOLTAGE_MV
    #: The emulator's Bluetooth: VIRTUAL, or a QEMU -serial spec.
    qemu_bt_hci: str = None
    phone: PhoneSetup = None

    def lacks(self, need):
        """Why the setup cannot serve ``need`` (``phone``, ``power``), or
        None."""
        if need == "phone":
            if self.phone is None:
                return "no phone in this setup"
            if self.phone.type == PHONE_COREAPP:
                return "CoreApp phones are not supported yet"
            if self.phone.controller is None and self.qemu_bt_hci != VIRTUAL:
                return "no Bluetooth controller for the phone"
        elif need == "power":
            if self.ppk2 is None:
                return "no power supply in this setup"
        return None

    def describe(self):
        parts = [self.watch or self.device_type]
        if self.serial:
            parts.append(f"serial {', '.join(self.serial)}")
        if self.ppk2:
            parts.append(f"PPK2 {self.ppk2} at {self.voltage_mv} mV")
        if self.qemu_bt_hci:
            parts.append(f"Bluetooth {self.qemu_bt_hci}")
        if self.phone:
            parts.append(f"phone {self.phone}")
        return ", ".join(parts)


def resolve(build, device_type, options, lab=None):
    """The setup for ``build``, from the command line's ``options`` and the
    lab's wiring: ``serial`` (list), ``serial_baud``, ``ppk2``,
    ``voltage_mv``, ``qemu_bt_hci`` (VIRTUAL, FROM_LAB or a QEMU -serial
    spec), ``ble_controller``, ``phone`` (a PHONE_* type) and ``watch`` (the
    lab's name for it)."""
    lab = lab or Lab()
    setup = Setup(device_type=device_type)
    phone_type = options.get("phone") or PHONE_BUMBLE
    if phone_type not in PHONE_TYPES:
        raise HarnessError(f"no phone of type {phone_type!r}")
    dongles = lab.dongle_ports()
    phone_controller = None

    if device_type == "hardware" and build is not None:
        watch = lab.watch_for(build, options.get("watch"))
        if watch is not None:
            setup.watch = watch.name
            setup.serial = [watch.serial]
            setup.serial_baud = watch.serial_baud
            if watch.supply is not None:
                setup.ppk2 = lab.supplies[watch.supply].port
        phone_controller = dongles[0] if dongles else None
    elif device_type == "qemu" and build is not None:
        if build.config.get("CONFIG_BT_HCI_UART"):
            setup.qemu_bt_hci = options.get("qemu_bt_hci") or VIRTUAL
        if setup.qemu_bt_hci == FROM_LAB:
            if len(dongles) < 2:
                raise HarnessError(
                    f"--qemu-bt-hci {FROM_LAB} takes two of the lab's dongles, "
                    f"it has {len(dongles)}"
                )
            setup.qemu_bt_hci, phone_controller = dongles[0], dongles[1]

    if options.get("serial"):
        setup.serial = list(options["serial"])
    if options.get("serial_baud"):
        setup.serial_baud = options["serial_baud"]
    if options.get("ppk2"):
        setup.ppk2 = options["ppk2"]
    if options.get("voltage_mv"):
        setup.voltage_mv = options["voltage_mv"]
    phone_controller = options.get("ble_controller") or phone_controller
    if device_type == "hardware" or setup.qemu_bt_hci:
        setup.phone = PhoneSetup(
            type=phone_type,
            controller=phone_controller if phone_type == PHONE_BUMBLE else None,
        )
    return setup


def default_lab_path():
    """The lab file of $PBL_ITEST_LAB, if set."""
    return os.environ.get("PBL_ITEST_LAB") or None
