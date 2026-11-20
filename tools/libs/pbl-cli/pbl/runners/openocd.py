# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0

import errno
import os
import re
import socket
import subprocess

from .core import Runner, RunnerCaps, RunnerError, UnsupportedOperation

OPENOCD_TELNET_PORT = 4444
OPENOCD_GDB_PORT = 3333
OPENOCD_LOG = ".openocd.log"


def _is_openocd_running():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        s.bind(("", OPENOCD_TELNET_PORT))
        s.close()
    except OSError as e:
        s.close()
        return e.errno == errno.EADDRINUSE
    return False


class OpenOcdRunner(Runner):
    name = "openocd"
    lab_ports = ("probe",)

    @classmethod
    def capabilities(cls):
        return RunnerCaps(
            commands={"flash", "erase", "debug", "debugserver"},
            flash_data=True,
            erase_code=True,
        )

    def locks(self, command):
        # 'pbl debug' attaches to a running 'pbl debugserver', which holds them
        if command == "debug" and _is_openocd_running():
            return []
        return super().locks(command)

    @property
    def _cfg_file(self):
        return os.path.join(self.cfg.board_dir, "support", "openocd.cfg")

    @property
    def _flash_cfg(self):
        return os.path.join(self.cfg.board_dir, "support", "flash.cfg")

    def _check_flash_cfg(self, what):
        if not os.path.exists(self._flash_cfg):
            raise UnsupportedOperation(f"the board has no support/flash.cfg to {what}")

    def _erase_ranges(self):
        self._check_flash_cfg("erase ranges")
        erase = "".join(f"pbl_erase {a:#x} {s:#x}; " for a, s in self.cfg.erase_ranges)
        self._run_command(
            f"init; reset halt; source {{{self._flash_cfg}}}; {erase}"
            "echo {Erase Finished}; reset;",
            expect=["Erase Finished", "shutdown"],
            enforce_expect=True,
        )

    def _program(self):
        return "".join(f"program {{{f}}}; " for f in self.cfg.flash_files)

    def _flash_data(self):
        self._check_flash_cfg("write files to the flash")
        erase = ""
        if self.cfg.erase_code:
            address, size = self.cfg.erase_code
            erase = f"flash erase_address {address:#x} {size:#x}; "
        writes = "".join(f"pbl_write {{{f}}} {o:#x}; " for f, o in self.cfg.data_files)
        programmed = ["Programming Started", "Programming Finished"]
        self._run_command(
            f"init; reset halt; {erase}{self._program()}"
            f"source {{{self._flash_cfg}}}; {writes}"
            "echo {Data Finished}; reset;",
            expect=[
                *(programmed if self.cfg.flash_files else []),
                "Data Finished",
                "shutdown",
            ],
            enforce_expect=True,
        )

    def do_run(self, command):
        if command == "flash" and (self.cfg.data_files or self.cfg.erase_code):
            self._flash_data()
        elif command == "flash":
            self._run_command(
                f"init; reset halt; {self._program()}reset;",
                expect=["Programming Started", "Programming Finished", "shutdown"],
                enforce_expect=True,
            )
        elif command == "erase":
            self._erase_ranges()
        elif command == "debug":
            port = self.gdb_port(OPENOCD_GDB_PORT)
            return self.run_server_and_gdb(self._server(port), port)
        elif command == "debugserver":
            return self.run_program(self._server(self.gdb_port(OPENOCD_GDB_PORT)))

    def _server(self, port):
        return ["openocd", "-f", self._cfg_file, "-c", f"gdb_port {port}"]

    def _run_command(
        self, cmd, ignore_fail=False, expect=(), shutdown=True, enforce_expect=False
    ):
        if _is_openocd_running():
            import telnetlib

            t = telnetlib.Telnet("", OPENOCD_TELNET_PORT)
            print(f"Sending commands to OpenOCD daemon:\n{cmd}\n...")
            t.write(b"%s\n" % cmd.encode())
            for regex in expect:
                idx, _match, _text = t.expect([regex.encode()], 40)
                if enforce_expect and idx == -1:
                    raise RunnerError(f"OpenOCD expectation '{regex}' unfulfilled")
            t.close()
            return

        if shutdown:
            if self.cfg.soc == "NRF52":
                # On nRF5, shut down the tracing modules in DEMCR, take the core
                # out of debug in DHCSR, and then shut down the AP to get back
                # down to baseline power.
                cmd = (
                    f"{cmd} ; mww 0xe000edfc 0x0; mww 0xe000edf0 0xa05f0000 ; "
                    "nrf52.dap dpreg 4 0 ; shutdown"
                )
            else:
                cmd = f"{cmd} ; shutdown"

        fail_handling = " || true " if ignore_fail else ""
        shell_cmd = f'openocd -f {self._cfg_file} -c "{cmd}" 2>&1 | tee {OPENOCD_LOG} {fail_handling}'
        if self.cfg.dry_run:
            print("[dry-run] " + shell_cmd)
            return

        subprocess.call(shell_cmd, shell=True)

        if enforce_expect:
            with open(OPENOCD_LOG, "r") as result_file:
                result = result_file.read()
            match_start = 0
            for regex in expect:
                expect_match = re.search(regex, result[match_start:])
                if not expect_match:
                    raise RunnerError(f"OpenOCD expectation '{regex}' unfulfilled")
                match_start += expect_match.end()
