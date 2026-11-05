# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""The setup a build gets from the lab (:mod:`pbl.lab`), the hardware wired
to the host running the tests, and the command line.

The setup for a build is what the tests run with: the device, how it is
reached and powered, and what plays the phone, from the lab and the
command line. Tests only see the setup, so they run unchanged on any of:

- the emulator or a native build, with Bumble's software controllers for
  its Bluetooth and the phone;
- the emulator or a native build, with a dongle for its Bluetooth and
  another for the phone;
- a watch on its debug serial port, with the phone on a dongle;
- a watch on its debug serial port, with a phone running CoreApp (not
  supported yet).
"""

import dataclasses

from pbl.lab import Lab, LabError

from harness.errors import HarnessError

#: An emulated watch's Bluetooth through Bumble's software controllers,
#: which also give the phone one.
VIRTUAL = "virtual"
#: An emulated watch's Bluetooth through the lab's first dongle, the
#: phone's through its second.
FROM_LAB = "lab"

PHONE_BUMBLE = "bumble"
PHONE_COREAPP = "coreapp"
PHONE_TYPES = (PHONE_BUMBLE, PHONE_COREAPP)

DEFAULT_VOLTAGE_MV = 3800


def load(path):
    try:
        return Lab.load(path)
    except LabError as e:
        raise HarnessError(str(e)) from None


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
    serial_baud: int = None
    #: The supply powering the watch: a :mod:`pbl.power` type, and its port.
    supply_type: str = None
    supply_port: str = None
    voltage_mv: int = DEFAULT_VOLTAGE_MV
    #: An emulated watch's Bluetooth: VIRTUAL, or a dongle's serial port
    #: (for QEMU, any -serial spec).
    bt_hci: str = None
    phone: PhoneSetup = None

    def lacks(self, need):
        """Why the setup cannot serve ``need`` (``phone``, ``power``), or
        None."""
        if need == "phone":
            if self.phone is None:
                return "no phone in this setup"
            if self.phone.type == PHONE_COREAPP:
                return "CoreApp phones are not supported yet"
            if self.phone.controller is None and self.bt_hci != VIRTUAL:
                return "no Bluetooth controller for the phone"
        elif need == "power" and self.supply_port is None:
            return "no power supply in this setup"
        return None

    def locks(self):
        """The locks (:mod:`pbl.lock`) on the ports the setup uses."""
        from pbl import lock

        locks = [(lock.serial_key(port), f"serial port {port}") for port in self.serial]
        if self.watch:
            locks.append(
                (lock.probe_key(self.watch), f"watch {self.watch}'s debug probe")
            )
        if self.supply_port:
            from pbl import power

            key = power.lock_key(self.supply_type, self.supply_port)
            locks.append((key, f"{self.supply_type} {self.supply_port}"))
        for port in (self.bt_hci, self.phone.controller if self.phone else None):
            locks.append((lock.serial_key(port), f"Bluetooth controller {port}"))
        return locks

    def describe(self):
        parts = [self.watch or self.device_type]
        if self.serial:
            parts.append(f"serial {', '.join(self.serial)}")
        if self.supply_port:
            parts.append(
                f"{self.supply_type} {self.supply_port} at {self.voltage_mv} mV"
            )
        if self.bt_hci:
            parts.append(f"Bluetooth {self.bt_hci}")
        if self.phone:
            parts.append(f"phone {self.phone}")
        return ", ".join(parts)


def resolve(build, device_type, options, lab=None):
    """The setup for ``build``, from the command line's ``options`` and the
    lab's wiring: ``serial`` (list), ``serial_baud``, ``ppk2``,
    ``voltage_mv``, ``bt_hci`` (VIRTUAL, FROM_LAB or a serial port), ``ble_controller``, ``phone`` (a PHONE_* type) and ``watch`` (the
    lab's name for it)."""
    lab = lab or Lab()
    setup = Setup(device_type=device_type)
    phone_type = options.get("phone") or PHONE_BUMBLE
    if phone_type not in PHONE_TYPES:
        raise HarnessError(f"no phone of type {phone_type!r}")
    dongles = lab.dongle_ports()
    phone_controller = None

    if device_type == "hardware" and build is not None:
        try:
            watch = lab.watch_for(build.board_target, options.get("watch"))
        except LabError as e:
            raise HarnessError(str(e)) from None
        if watch is not None:
            setup.watch = watch.name
            setup.serial = [watch.serial] if watch.serial else []
            if watch.supply is not None:
                supply = lab.supplies[watch.supply]
                setup.supply_type, setup.supply_port = supply.type, supply.port
        phone_controller = dongles[0] if dongles else None
    elif device_type in ("qemu", "native") and build is not None:
        if build.config.get("CONFIG_BT_HCI_UART"):
            setup.bt_hci = options.get("bt_hci") or VIRTUAL
        if setup.bt_hci == FROM_LAB:
            if len(dongles) < 2:
                raise HarnessError(
                    f"--bt-hci {FROM_LAB} takes two of the lab's dongles, "
                    f"it has {len(dongles)}"
                )
            setup.bt_hci, phone_controller = dongles[0], dongles[1]

    if options.get("serial"):
        setup.serial = list(options["serial"])
    if options.get("serial_baud"):
        setup.serial_baud = options["serial_baud"]
    if options.get("ppk2"):
        setup.supply_type, setup.supply_port = "ppk2", options["ppk2"]
    if options.get("voltage_mv"):
        setup.voltage_mv = options["voltage_mv"]
    phone_controller = options.get("ble_controller") or phone_controller
    if device_type == "hardware" or setup.bt_hci:
        setup.phone = PhoneSetup(
            type=phone_type,
            controller=phone_controller if phone_type == PHONE_BUMBLE else None,
        )
    return setup
