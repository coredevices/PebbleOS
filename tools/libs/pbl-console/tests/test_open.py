# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Opening a port keeps what it received as it opened only when asked."""

import pytest
import serial
from pblconsole.link import open_port, usb_ids


class FakePort:
    """Opens as pyserial's POSIX port does: flushing the input it received."""

    def __init__(self):
        self.rts = True
        self.received = b"*** Booting PebbleOS ***\r\n"

    def _reset_input_buffer(self):
        self.received = b""

    def reset_input_buffer(self):
        self._reset_input_buffer()

    def open(self):
        assert not self.rts
        self._reset_input_buffer()


@pytest.fixture
def port(monkeypatch):
    fake = FakePort()
    monkeypatch.setattr(serial, "serial_for_url", lambda *args, **kwargs: fake)
    return fake


def test_flushes_by_default(port):
    assert open_port("/dev/ttyX", 115200, 0) is port
    assert port.received == b""


def test_keeps_input(port):
    assert open_port("/dev/ttyX", 115200, 0, keep_input=True) is port
    assert port.received == b"*** Booting PebbleOS ***\r\n"
    # The port flushes again when asked to
    port.reset_input_buffer()
    assert port.received == b""


class FakeInfo:
    def __init__(self, device, vid, pid):
        self.device = device
        self.vid = vid
        self.pid = pid


@pytest.fixture
def ports(monkeypatch, tmp_path):
    from serial.tools import list_ports

    acm = tmp_path / "ttyACM0"
    acm.touch()
    bridge = tmp_path / "ttyUSB0"
    bridge.touch()
    infos = [FakeInfo(str(acm), 0x1209, 0x0001), FakeInfo(str(bridge), 0x1A86, 0x55D3)]
    monkeypatch.setattr(list_ports, "comports", lambda *args, **kwargs: infos)
    return tmp_path


def test_usb_ids(ports):
    assert usb_ids(str(ports / "ttyACM0")) == (0x1209, 0x0001)
    assert usb_ids(str(ports / "ttyUSB0")) == (0x1A86, 0x55D3)
    assert usb_ids(str(ports / "ttyS0")) is None
    assert usb_ids("socket://localhost:12345") is None


def test_usb_ids_through_link(ports):
    by_id = ports / "usb-PebbleOS_PebbleOS_device-if00"
    by_id.symlink_to(ports / "ttyACM0")
    assert usb_ids(str(by_id)) == (0x1209, 0x0001)


def test_usb_ids_tty_node(monkeypatch):
    from serial.tools import list_ports

    infos = [FakeInfo("/dev/cu.usbmodem1134101", 0x1209, 0x0001)]
    monkeypatch.setattr(list_ports, "comports", lambda *args, **kwargs: infos)
    assert usb_ids("/dev/tty.usbmodem1134101") == (0x1209, 0x0001)
