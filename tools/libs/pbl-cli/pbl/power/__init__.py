# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Power supplies wired in place of a watch's battery.

A supply powers the watch on, off and through a cycle, and sets the voltage
it supplies. Some also measure the current the watch draws
(:attr:`Supply.measures`). Each type of supply is a module of this package,
registered in :data:`TYPES`; lab files name supplies by type.
"""

import contextlib
import importlib
import time
from abc import ABC, abstractmethod

#: Supply types, and the class implementing each.
TYPES = {
    "ppk2": "pbl.power.ppk2:Ppk2",
}

DEFAULT_OFF_S = 1.0


class PowerError(Exception):
    pass


class Supply(ABC):
    """A power supply in place of a watch's battery."""

    #: Whether :meth:`measure` is available.
    measures = False

    def __init__(self, port):
        self.port = port
        self.voltage_mv = None
        #: Whether the watch is powered: None until switched.
        self.powered = None

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    @abstractmethod
    def on(self):
        pass

    @abstractmethod
    def off(self):
        pass

    def cycle(self, off_s=DEFAULT_OFF_S):
        self.off()
        time.sleep(off_s)
        self.on()

    @abstractmethod
    def set_voltage(self, voltage_mv):
        pass

    def close(self):
        pass

    @contextlib.contextmanager
    def measure(self, name="measurement"):
        """Sample the current for the duration of the ``with`` block,
        yielding a :class:`~pbl.power.measurement.Measurement` that holds
        the samples once the block exits."""
        raise PowerError(f"a {type(self).__name__} cannot measure current")
        yield


def supply_class(type):
    try:
        module, _, name = TYPES[type].partition(":")
    except KeyError:
        raise PowerError(f"unknown supply type {type!r}") from None
    return getattr(importlib.import_module(module), name)


def open_supply(type, port="auto", voltage_mv=None):
    """Open a supply of ``type`` on ``port``, setting its voltage when
    ``voltage_mv`` is given. A supply may cut the power once closed: to keep
    a watch powered across commands, :mod:`pbl.power.daemon` holds it."""
    return supply_class(type)(port, voltage_mv)


def lock_key(type, port):
    """The lock (:mod:`pbl.lock`) on a supply's port."""
    from pbl import lock

    return lock.serial_key(port) or f"{type}-{port}"
