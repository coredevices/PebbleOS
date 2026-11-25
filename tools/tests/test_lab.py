# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "libs" / "pbl-cli"))

from pbl import power
from pbl.commands.power import start_voltage
from pbl.lab import Lab


def lab(**device):
    return Lab.parse(
        {
            "watches": {"disco": {"board": "stm32h747i_disco", **device}},
            "supplies": {"ppk2": {"type": "ppk2", "port": "/dev/cu.usbmodem1"}},
        }
    )


def test_voltage_is_required_with_a_supply():
    with pytest.raises(ValueError, match="supply ppk2 has no voltage"):
        lab(supply="ppk2")


def test_voltage_is_optional_without_a_supply():
    assert lab().watches["disco"].voltage is None


def test_voltage_of_a_device():
    assert lab(supply="ppk2", voltage=3300).watches["disco"].voltage == 3300


@pytest.mark.parametrize("voltage", ["3300", 3.3, True, 0, -3300])
def test_invalid_voltage_is_rejected(voltage):
    with pytest.raises(ValueError, match="voltage"):
        lab(supply="ppk2", voltage=voltage)


def test_invalid_voltage_fails_the_lab_file(tmp_path):
    path = tmp_path / "lab.yaml"
    path.write_text("watches:\n  disco:\n    board: x\n    voltage: 3.3V\n")
    with pytest.raises(Exception, match="invalid lab file.*voltage"):
        Lab.load(str(path))


def test_powered_by():
    lab_ = lab(supply="ppk2", voltage=3300)
    assert lab_.powered_by("ppk2").name == "disco"
    assert lab(voltage=3300).powered_by("ppk2") is None


def test_lab_file_without_a_voltage_fails(tmp_path):
    path = tmp_path / "lab.yaml"
    path.write_text(
        "watches:\n  disco:\n    board: x\n    supply: ppk2\n"
        "supplies:\n  ppk2:\n    type: ppk2\n"
    )
    with pytest.raises(Exception, match="invalid lab file.*has no voltage"):
        Lab.load(str(path))


def test_start_voltage():
    device = lab(supply="ppk2", voltage=3300).watches["disco"]
    assert start_voltage(None, "ppk2", device) == 3300
    assert start_voltage(3300, "ppk2", device) == 3300
    assert start_voltage(3000, "ppk2", None) == 3000


def test_no_voltage_to_start_at():
    with pytest.raises(
        power.PowerError, match="supply ppk2 powers no device.* pass --voltage"
    ):
        start_voltage(None, "ppk2", None)
