# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import os

import yaml

from .core import Runner, RunnerCaps, RunnerError, port_in_use

GDB_PORT = 1337


def _support_file(board_dir):
    return os.path.join(board_dir, "support", "probe-rs.yml")


class ProbeRsRunner(Runner):
    name = "probe-rs"
    lab_ports = ("serial", "probe")

    @property
    def tty(self):
        # Only a debugger reached over a serial port takes one
        return self.cfg.tty if self._support().get("serial") else None

    @classmethod
    def capabilities(cls):
        return RunnerCaps(commands={"debug", "debugserver"})

    @classmethod
    def supports(cls, cfg):
        return os.path.exists(_support_file(cfg.board_dir))

    @classmethod
    def do_add_parser(cls, parser):
        parser.add_argument(
            "--probe",
            metavar="VID:PID[:SERIAL]",
            help="Debug probe for the probe-rs runner (default: the lab's)",
        )

    def locks(self, command):
        # 'pbl debug' attaches to a running 'pbl debugserver', which holds them
        if command == "debug" and port_in_use(self.gdb_port(GDB_PORT)):
            return []
        locks = super().locks(command)
        return locks[:1] if self.tty else locks[1:]

    def _support(self):
        with open(_support_file(self.cfg.board_dir)) as f:
            support = yaml.safe_load(f) or {}
        if not support.get("chip"):
            raise RunnerError("the board's support/probe-rs.yml names no chip")
        return support

    def gdb_init_commands(self):
        flash = self._support().get("gdb-flash")
        if not flash:
            return []
        start, end = flash
        # User regions replace the server's memory map, which would also
        # describe the rest of the address space.
        return [
            "set mem inaccessible-by-default off",
            f"mem {start:#x} {end:#x} ro",
        ]

    def _serial_probe(self):
        from serial.tools import list_ports

        for port in list_ports.comports():
            if port.device == self.tty and port.vid is not None:
                return f"{port.vid:04x}:{port.pid:04x}:{self.tty}"
        raise RunnerError(f"{self.tty} is not a USB serial port")

    def _server(self, port):
        support = self._support()
        if support.get("serial") and not self.tty:
            raise RunnerError(
                "the board is debugged over a serial port, use --tty or a lab file"
            )
        argv = [
            "probe-rs", "gdb",
            "--chip", support["chip"],
            "--gdb-connection-string", f"127.0.0.1:{port}",
            "--non-interactive",
        ]  # fmt: skip
        env = None
        probe = self.cfg.probe
        if self.tty:
            # A debugger reached over a serial port. Without this, probe-rs
            # only takes ports with a matching USB product name.
            env = {"SIFLI_UART_DEBUG": "1"}
            if probe is None and not self.cfg.dry_run:
                probe = self._serial_probe()
            elif probe is None:
                probe = f"VID:PID:{self.tty}"
        if probe:
            argv += ["--probe", probe]
        return argv, env

    def do_run(self, command):
        port = self.gdb_port(GDB_PORT)
        server, env = self._server(port)
        if command == "debug":
            return self.run_server_and_gdb(server, port, env=env)
        return self.run_program(server, env=env)
