# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import contextlib
import os
import sys
import tempfile
import threading
from pathlib import Path

import numpy as np
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "libs" / "pbl-cli"))

from pbl import lock, power
from pbl.commands.power import start_voltage
from pbl.lab import Lab
from pbl.power import daemon
from pbl.power.measurement import Measurement


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


class FakeSupply(power.Supply):
    measures = True

    def __init__(self, port, voltage_mv=None):
        super().__init__(port)
        self.voltage_mv = voltage_mv

    def on(self):
        self.powered = True

    def off(self):
        self.powered = False

    def set_voltage(self, voltage_mv):
        self.voltage_mv = voltage_mv

    @contextlib.contextmanager
    def measure(self, name="measurement"):
        m = Measurement(name, self.voltage_mv, 1000)
        yield m
        m.samples = np.arange(10, dtype=float)


@pytest.fixture
def lock_dir(monkeypatch):
    # Short: macOS limits unix socket paths to 104 bytes.
    with tempfile.TemporaryDirectory(dir="/tmp") as tmp:
        monkeypatch.setenv(lock.ENV, tmp)
        yield tmp


@pytest.fixture
def served(lock_dir):
    supply = FakeSupply("/dev/cu.fake", 3300)
    d = daemon._Daemon(supply, "ppk2")
    thread = threading.Thread(
        target=daemon._serve, args=(d, daemon.socket_path("k")), daemon=True
    )
    thread.start()
    for _ in range(100):
        if os.path.exists(daemon.socket_path("k")):
            break
        threading.Event().wait(0.01)
    yield supply
    d.stopping.set()
    thread.join()


def test_client_drives_the_daemon(served):
    client = daemon.Client("k")
    assert client.voltage_mv == 3300
    assert client.measures
    client.on()
    assert served.powered and client.powered
    client.off()
    assert served.powered is False
    assert daemon.running("k")


def test_client_measures(served):
    client = daemon.Client("k")
    with client.measure("idle") as m:
        pass
    assert m.name == "idle"
    assert m.voltage_mv == 3300
    assert m.sample_rate_hz == 1000
    assert list(m.samples) == list(range(10))


def test_no_daemon(lock_dir):
    assert daemon.status("k") is None
    with pytest.raises(power.PowerError):
        daemon.Client("k")


def test_parent_holds(lock_dir):
    assert not lock.parent_holds("port-x")
    with lock.holding([("port-x", "serial port x")]):
        assert lock.parent_holds("port-x")
    assert not lock.parent_holds("port-x")
    assert not lock.parent_holds(None)
