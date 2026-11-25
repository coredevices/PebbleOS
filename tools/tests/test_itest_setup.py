# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import sys
from pathlib import Path
from types import SimpleNamespace

import pytest

TOPDIR = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(TOPDIR / "tools" / "libs" / "pbl-cli"))
sys.path.insert(0, str(TOPDIR / "tests" / "integration"))

from harness import lab as labs
from harness.errors import HarnessError
from pbl.lab import Lab

BUILD = SimpleNamespace(board_target="stm32h747i_disco", config={})
PORT = "/dev/cu.usbmodem1"


def lab(**device):
    device = {"board": "stm32h747i_disco", "supply": "ppk2", **device}
    return Lab.parse(
        {
            "watches": {"disco": {k: v for k, v in device.items() if v is not None}},
            "supplies": {"ppk2": {"type": "ppk2", "port": PORT}},
        }
    )


def resolve(lab_, **options):
    return labs.resolve(BUILD, "hardware", options, lab_)


def test_voltage_from_the_lab():
    setup = resolve(lab(voltage=3300))
    assert setup.voltage_mv == 3300
    assert "at 3300 mV" in setup.describe()


def test_option_overrides_the_lab():
    setup = resolve(lab(voltage=3300), voltage_mv=3800)
    assert setup.voltage_mv == 3800
    assert setup.voltage_from == "--voltage"


def test_emulator_ignores_the_lab_voltage():
    setup = labs.resolve(BUILD, "qemu", {}, lab(voltage=3300))
    assert setup.voltage_mv is None


def test_ppk2_without_a_voltage_fails():
    with pytest.raises(HarnessError, match="device disco's supply has no voltage"):
        resolve(lab(supply=None), ppk2="auto")


def test_ppk2_with_a_voltage():
    setup = resolve(lab(supply=None), ppk2="auto", voltage_mv=3800)
    assert (setup.supply_port, setup.voltage_mv) == ("auto", 3800)
