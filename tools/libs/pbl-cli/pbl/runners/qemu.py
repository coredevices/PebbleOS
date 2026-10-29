# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import os
import platform
import sys

from pbl import emulator

from .core import Runner, RunnerCaps, RunnerError

# The image the emulator boots from, and the one it keeps stored apps in.
MICRO_FLASH_TARGET = "qemu_image_micro"
SPI_FLASH_TARGET = "qemu_image_spi"
SPI_FLASH_IMAGE = "qemu_spi_flash.bin"

NO_DECORATION = "none"

# Where the gdb proxy, which makes the kernel's threads visible to gdb,
# listens; it forwards to the emulator's own gdb server.
GDB_PROXY_PORT = 1233


class QemuRunner(Runner):
    name = "qemu"

    def __init__(self, cfg, keep_flash_image=False, decoration=None, bt_hci=None):
        super().__init__(cfg)
        self.keep_flash_image = keep_flash_image
        self.decoration = decoration
        self.bt_hci = bt_hci

    @classmethod
    def capabilities(cls):
        return RunnerCaps(commands={"run", "debug", "debugserver"})

    @classmethod
    def supports(cls, cfg):
        return bool(cfg.build.config.CONFIG_QEMU)

    @classmethod
    def do_add_parser(cls, parser):
        parser.add_argument(
            "--keep-flash-image",
            action="store_true",
            help="Keep the emulator's SPI flash image, and the apps and data "
            "in it, instead of rebuilding it",
        )
        parser.add_argument(
            "--decoration",
            metavar="NAME",
            help="SDL decoration the emulator draws around the screen, or "
            "'none' (default: the board's first)",
        )
        parser.add_argument(
            "--bt-hci",
            metavar="CHARDEV",
            help="H4 Bluetooth controller for builds with CONFIG_BT_HCI_UART, "
            "e.g. the serial port /dev/cu.usbmodem1101",
        )

    @classmethod
    def do_create(cls, cfg, args):
        return cls(
            cfg,
            keep_flash_image=getattr(args, "keep_flash_image", False),
            decoration=getattr(args, "decoration", None),
            bt_hci=getattr(args, "bt_hci", None),
        )

    def do_run(self, command):
        if command == "debug":
            port = self.gdb_port(GDB_PROXY_PORT)
            return self.run_server_and_gdb(self._gdb_proxy(port), port)
        if command == "debugserver":
            return self.run_program(self._gdb_proxy(self.gdb_port(GDB_PROXY_PORT)))

        build = self.cfg.build
        decoration = self._decoration(build)

        hci_uart = bool(build.config.CONFIG_BT_HCI_UART)
        if hci_uart and not self.bt_hci:
            raise RunnerError(
                "the build uses CONFIG_BT_HCI_UART; pass --bt-hci with the "
                "controller's serial port"
            )
        if self.bt_hci and not hci_uart:
            raise RunnerError("--bt-hci needs a build with CONFIG_BT_HCI_UART=y")

        # The flash images are build artifacts. The micro flash is always
        # rebuilt; the SPI flash only unless the caller wants to keep it.
        targets = [MICRO_FLASH_TARGET]
        spi_flash = build.join(SPI_FLASH_IMAGE)
        if not self.keep_flash_image or not os.path.isfile(spi_flash):
            targets.append(SPI_FLASH_TARGET)
        self.call(
            self.quote("cmake", "--build", build, *(f"--target={t}" for t in targets))
        )

        return self.run_program(
            self._command_line(build, decoration, spi_flash), cwd=build.topdir
        )

    def _gdb_proxy(self, port):
        build = self.cfg.build
        return [
            sys.executable,
            os.path.join(build.topdir, "tools", "qemu", "qemu_gdb_proxy.py"),
            f"--port={port}",
            f"--target=localhost:{emulator.GDB_PORT}",
        ]

    def _decoration(self, build):
        available = build.board_spec.qemu.get("decorations", [])
        if self.decoration is None:
            return available[0] if available else None
        if self.decoration == NO_DECORATION:
            return None
        if self.decoration not in available:
            raise RunnerError(
                f"board {build.board} has no '{self.decoration}' decoration; "
                f"available: {', '.join(available + [NO_DECORATION])}"
            )
        return self.decoration

    def _machine_args(self, build, spi_flash):
        machine = build.config.CONFIG_QEMU_MACHINE
        if not machine or machine == "unknown":
            raise RunnerError(f"board {build.board} declares no QEMU machine")

        if build.config.CONFIG_PLATFORM_EMERY or build.config.CONFIG_PLATFORM_FLINT:
            driver = "coreaudio" if platform.system() == "Darwin" else "sdl"
            machine_args = [
                "-machine", f"{machine},audiodev=snd0",
                "-audiodev", f"{driver},id=snd0",
            ]
        else:
            machine_args = ["-machine", machine]

        # The ELF is loaded as the kernel so QEMU sets the vector table up.
        return machine_args + [
            "-kernel", build.elf,
            "-drive", f"if=mtd,format=raw,file={spi_flash}",
        ]

    def _command_line(self, build, decoration, spi_flash):
        qemu = os.getenv("PEBBLE_QEMU_BIN")
        if not qemu or not (os.path.isfile(qemu) and os.access(qemu, os.X_OK)):
            qemu = build.tool("qemu") or "qemu-pebble"

        machine_args = self._machine_args(build, spi_flash)

        display = f"sdl,decoration={decoration}" if decoration else "sdl"
        machine_args += ["-display", f"{display},show-cursor=on"]

        # Both control sockets are recreated on every launch.
        monitor = build.join(emulator.MONITOR_SOCKET)
        qmp = build.join(emulator.QMP_SOCKET)
        if not self.cfg.dry_run:
            for socket in (monitor, qmp):
                if os.path.exists(socket):
                    os.unlink(socket)

        launch = [
            qemu,
            "-rtc", "base=localtime",
            "-monitor", "stdio",
            "-monitor", f"unix:{monitor},server=on,wait=off",
            "-qmp", f"unix:{qmp},server=on,wait=off",
            "-s",
            "-serial", "file:uart1.log",
            "-serial", f"tcp::{emulator.PEBBLE_TOOL_PORT},server=on,wait=off",
            "-serial", f"tcp::{emulator.CONSOLE_PORT},server=on,wait=off",
        ]
        if self.bt_hci:
            launch += ["-serial", self.bt_hci]
        return launch + machine_args + list(self.cfg.extra_args)
