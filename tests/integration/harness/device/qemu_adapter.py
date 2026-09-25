# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import json
import os
import shutil
import socket
import subprocess
import tempfile
import time

from harness.device import DeviceAdapter
from harness.errors import HarnessError
from harness.lab import VIRTUAL

MICRO_FLASH_IMAGE = "qemu_micro_flash.bin"
SPI_FLASH_IMAGE = "qemu_spi_flash.bin"
ABS_MAX = 32767
# --qemu-bt-hci value for Bumble's software controllers instead of a radio.


def _free_port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


class _Monitor:
    def __init__(self, path):
        self._sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self._sock.settimeout(10)
        self._sock.connect(path)
        self._read_until_prompt()

    def _read_until_prompt(self):
        buf = b""
        while b"(qemu) " not in buf:
            chunk = self._sock.recv(4096)
            if not chunk:
                break
            buf += chunk
        return buf.decode(errors="replace")

    def command(self, cmd):
        self._sock.sendall((cmd + "\n").encode())
        return self._read_until_prompt()

    def close(self):
        self._sock.close()


class _Qmp:
    def __init__(self, path):
        self._sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self._sock.settimeout(10)
        self._sock.connect(path)
        self._stream = self._sock.makefile("rw")
        self._stream.readline()
        self.execute("qmp_capabilities")

    def execute(self, command, **arguments):
        request = {"execute": command}
        if arguments:
            request["arguments"] = arguments
        self._stream.write(json.dumps(request) + "\n")
        self._stream.flush()
        while True:
            reply = json.loads(self._stream.readline())
            if "event" not in reply:
                return reply

    def close(self):
        self._stream.close()
        self._sock.close()


class QemuAdapter(DeviceAdapter):
    """The firmware under QEMU, headless, on a private copy of the flash."""

    type = "qemu"

    def __init__(self, config):
        super().__init__(config)
        self.workdir = config.results_dir
        self.console_port = _free_port()
        self.pebble_port = _free_port()
        # Unix socket paths are limited to ~100 characters.
        self._sockdir = None
        self._process = None
        self._qemu_log = None
        self._virtual_link = None
        self._bt_hci = None

    def _qemu(self):
        qemu = os.getenv("PEBBLE_QEMU_BIN") or self.build.tool("qemu") or "qemu-pebble"
        if not shutil.which(qemu):
            raise HarnessError(f"QEMU not found ({qemu}); set PEBBLE_QEMU_BIN")
        return qemu

    def _command_line(self, spi_flash):
        config = self.build.config
        machine = config.get("CONFIG_QEMU_MACHINE")
        if not machine or machine == "unknown":
            raise HarnessError(f"board {self.build.board} declares no QEMU machine")

        if config.get("CONFIG_PLATFORM_EMERY") or config.get("CONFIG_PLATFORM_FLINT"):
            machine_args = [
                "-machine",
                f"{machine},audiodev=snd0",
                "-audiodev",
                "none,id=snd0",
            ]
        else:
            machine_args = ["-machine", machine]

        return [
            self._qemu(),
            "-rtc", f"base={self.config.qemu_rtc or 'localtime'}",
            "-display", "none",
            "-monitor", f"unix:{self.monitor_socket},server=on,wait=off",
            "-qmp", f"unix:{self.qmp_socket},server=on,wait=off",
            "-serial", f"file:{os.path.join(self.workdir, 'uart1.log')}",
            "-serial", f"tcp:127.0.0.1:{self.pebble_port},server=on,wait=off",
            "-serial", f"tcp:127.0.0.1:{self.console_port},server=on,wait=off",
            *(["-serial", self._bt_hci] if self._bt_hci else []),
            *machine_args,
            "-kernel", self.build.elf,
            "-drive", f"if=mtd,format=raw,file={spi_flash}",
        ]  # fmt: skip

    @property
    def monitor_socket(self):
        return os.path.join(self._sockdir, "mon.sock")

    @property
    def qmp_socket(self):
        return os.path.join(self._sockdir, "qmp.sock")

    def _start_bluetooth(self):
        """Attach the emulator's fourth UART to an H4 controller, as the
        build's CONFIG_BT_HCI_UART needs: the given one, or Bumble's
        software controllers ('virtual'), whose other end is the harness's."""
        hci_uart = bool(self.build.config.get("CONFIG_BT_HCI_UART"))
        chardev = self.config.qemu_bt_hci
        if hci_uart and not chardev:
            raise HarnessError(
                "the build uses CONFIG_BT_HCI_UART: its Bluetooth needs a controller"
            )
        if chardev and not hci_uart:
            raise HarnessError("--qemu-bt-hci needs a build with CONFIG_BT_HCI_UART=y")
        if chardev == VIRTUAL:
            from harness.ble.virtual import VirtualLink

            self._virtual_link = VirtualLink(os.path.join(self.workdir, "bt-link.log"))
            self._virtual_link.start()
            chardev = self._virtual_link.watch_chardev
            self.ble_controller = self._virtual_link.host_controller
        self._bt_hci = chardev

    def _device_launch(self):
        self._sockdir = tempfile.mkdtemp(prefix="pbl-qemu-")
        os.makedirs(self.workdir, exist_ok=True)
        self._start_bluetooth()
        for image in (MICRO_FLASH_IMAGE, SPI_FLASH_IMAGE):
            if not os.path.isfile(self.build.join(image)):
                raise HarnessError(
                    f"no {image} in {self.build.path} -- run "
                    "'pbl build qemu_image_micro qemu_image_spi' first"
                )
        os.makedirs(self.workdir, exist_ok=True)
        spi_flash = os.path.join(self.workdir, SPI_FLASH_IMAGE)
        shutil.copyfile(self.build.join(SPI_FLASH_IMAGE), spi_flash)

        command = self._command_line(spi_flash)
        self._qemu_log = open(os.path.join(self.workdir, "qemu.log"), "w")  # noqa: SIM115
        self._qemu_log.write(" ".join(command) + "\n")
        self._qemu_log.flush()
        self._process = subprocess.Popen(
            command, stdout=self._qemu_log, stderr=subprocess.STDOUT
        )

        deadline = time.monotonic() + 10
        while not os.path.exists(self.monitor_socket):
            if self._process.poll() is not None:
                raise HarnessError(
                    f"QEMU exited with {self._process.returncode}; see "
                    f"{os.path.join(self.workdir, 'qemu.log')}"
                )
            if time.monotonic() > deadline:
                raise HarnessError("QEMU did not open its monitor socket")
            time.sleep(0.05)

    def _close_device(self):
        if self._process is not None and self._process.poll() is None:
            try:
                self._monitor("quit")
            except OSError:
                pass
            try:
                self._process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self._process.kill()
                self._process.wait()
        self._process = None
        if self._sockdir is not None:
            shutil.rmtree(self._sockdir, ignore_errors=True)
            self._sockdir = None
        if self._qemu_log is not None:
            self._qemu_log.close()
            self._qemu_log = None
        if self._virtual_link is not None:
            self._virtual_link.stop()
            self._virtual_link = None

    def _monitor(self, command):
        monitor = _Monitor(self.monitor_socket)
        try:
            return monitor.command(command)
        finally:
            monitor.close()

    def _hard_reset(self):
        self._monitor("system_reset")
        return True

    def default_connections(self):
        console = f"socket://127.0.0.1:{self.console_port}"
        if self.build.config.get("CONFIG_PULSE_EVERYWHERE"):
            return [f"pulse:{console}"]
        return [f"serial:{console}", f"qemu:127.0.0.1:{self.pebble_port}"]

    def set_battery(self, percent, charging=False):
        from libpebble2.communication.transports.qemu import (
            MessageTargetQemu,
            QemuTransport,
        )
        from libpebble2.communication.transports.qemu.protocol import QemuBattery

        from harness.connections.qemu import QemuProtocolConnection

        packet = QemuBattery(percent=percent, charging=charging)
        for connection in self.connections:
            if isinstance(connection, QemuProtocolConnection):
                connection.send_to_qemu(packet)
                return
        # The port serves one client at a time; nothing holds it.
        transport = QemuTransport("127.0.0.1", self.pebble_port)
        transport.connect()
        try:
            transport.send_packet(packet, target=MessageTargetQemu())
        finally:
            transport.socket.close()

    def screenshot(self):
        from PIL import Image

        path = os.path.join(self._sockdir, "screendump.png")
        if os.path.exists(path):
            os.unlink(path)
        response = self._monitor(f"screendump {path} -f png")
        if not os.path.isfile(path):
            raise HarnessError(f"QEMU wrote no screenshot: {response.strip()}")
        with Image.open(path) as image:
            return image.convert("RGB")

    def _touch_display(self, qmp):
        stack = ["/machine"]
        while stack:
            path = stack.pop()
            for entry in qmp.execute("qom-list", path=path).get("return", []):
                if not entry.get("type", "").startswith("child<"):
                    continue
                child = f"{path}/{entry['name']}"
                if "touch" in entry["name"].lower() or "touch" in entry["type"].lower():
                    return tuple(
                        qmp.execute("qom-get", path=child, property=prop)["return"]
                        for prop in ("display-width", "display-height")
                    )
                stack.append(child)
        return None

    def tap(self, x, y):
        qmp = _Qmp(self.qmp_socket)
        try:
            size = self._touch_display(qmp)
            if size is None:
                return False
            width, height = size
            qmp.execute(
                "input-send-event",
                events=[
                    {
                        "type": "abs",
                        "data": {"axis": "x", "value": int(x / width * ABS_MAX)},
                    },
                    {
                        "type": "abs",
                        "data": {"axis": "y", "value": int(y / height * ABS_MAX)},
                    },
                    {"type": "btn", "data": {"button": "left", "down": True}},
                ],
            )
            time.sleep(0.05)
            qmp.execute(
                "input-send-event",
                events=[{"type": "btn", "data": {"button": "left", "down": False}}],
            )
            return True
        finally:
            qmp.close()
