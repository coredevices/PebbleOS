# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0

import os
import re
import socket
import subprocess
import time

from .core import Runner, RunnerCaps, RunnerError, UnsupportedOperation

OPENOCD_TELNET_PORT = 4444
OPENOCD_GDB_PORT = 3333
OPENOCD_LOG = ".openocd.log"
#: TCP port of RTT channel 0, for 'pbl console --rtt'
RTT_PORT = 9090


def rtt_address(elf):
    """Address of the RTT control block, ``_SEGGER_RTT``, in ``elf``."""
    from elftools.elf.elffile import ELFFile

    with open(elf, "rb") as f:
        symtab = ELFFile(f).get_section_by_name(".symtab")
        symbols = symtab.get_symbol_by_name("_SEGGER_RTT") if symtab else None
        if not symbols:
            raise RunnerError(f"no _SEGGER_RTT in {elf}")
        return symbols[0]["st_value"]


# How long the daemon may take to answer each expectation
DAEMON_EXPECT_S = 40

# read_memory, which the flash scripts use, came with 0.12.0
OPENOCD_MIN_VERSION = (0, 12, 0)


def _expect(sock, received, regex, timeout):
    """Read from ``sock`` until ``regex`` matches what came; returns what came
    after the match, and whether it matched, as telnetlib's expect did."""
    pattern = re.compile(regex.encode())
    deadline = time.monotonic() + timeout
    while (match := pattern.search(received)) is None:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            return received, False
        sock.settimeout(remaining)
        try:
            data = sock.recv(4096)
        except TimeoutError:
            return received, False
        if not data:
            return received, False
        received += data
    return received[match.end() :], True


def _openocd_version():
    """The version of the openocd in PATH, or None if it does not say."""
    try:
        result = subprocess.run(
            ["openocd", "--version"], capture_output=True, text=True, check=False
        )
    except FileNotFoundError:
        raise RunnerError("openocd not found in PATH") from None
    # It prints its banner to stderr
    match = re.search(
        r"Open On-Chip Debugger (\d+)\.(\d+)\.(\d+)", result.stdout + result.stderr
    )
    return tuple(int(n) for n in match.groups()) if match else None


def _check_openocd_version():
    version = _openocd_version()
    if version is not None and version < OPENOCD_MIN_VERSION:
        found = ".".join(map(str, version))
        required = ".".join(map(str, OPENOCD_MIN_VERSION))
        raise RunnerError(
            f"OpenOCD {found} is too old: {required} or later is required"
        )


def _is_openocd_running():
    # Binding the port fails too while one that just exited leaves it in TIME_WAIT: connect
    try:
        with socket.create_connection(("localhost", OPENOCD_TELNET_PORT), timeout=1):
            return True
    except OSError:
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
        # A running daemon is used as it is; only an openocd started here is checked
        if not self.cfg.dry_run and not _is_openocd_running():
            _check_openocd_version()

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
        argv = ["openocd", "-f", self._cfg_file, "-c", f"gdb_port {port}"]
        build = self.cfg.build
        if build is not None and build.config.get("CONFIG_RTT"):
            # Also serve RTT channel 0, from the control block of the build
            if self.cfg.dry_run and not os.path.exists(self.cfg.elf_file):
                address = "$RTT_ADDR"
            else:
                address = f"{rtt_address(self.cfg.elf_file):#x}"
            rtt_cfg = os.path.join(build.topdir, "tools", "openocd", "rtt.cfg")
            argv += [
                "-c", f"set RTT_ADDR {address}",
                "-c", f"set RTT_PORT {RTT_PORT}",
                "-f", rtt_cfg,
            ]  # fmt: skip
        return argv

    def _run_command(
        self, cmd, ignore_fail=False, expect=(), shutdown=True, enforce_expect=False
    ):
        if _is_openocd_running():
            print(f"Sending commands to OpenOCD daemon:\n{cmd}\n...")
            with socket.create_connection(("localhost", OPENOCD_TELNET_PORT)) as sock:
                sock.sendall(b"%s\n" % cmd.encode())
                received = b""
                # The daemon keeps running: it never shuts down
                for regex in (r for r in expect if r != "shutdown"):
                    received, found = _expect(sock, received, regex, DAEMON_EXPECT_S)
                    if enforce_expect and not found:
                        raise RunnerError(f"OpenOCD expectation '{regex}' unfulfilled")
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
