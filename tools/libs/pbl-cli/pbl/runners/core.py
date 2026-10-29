# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Core binary-runner abstraction, modeled on Zephyr's west runners.

A runner knows how to talk to a target for a small set of commands
(flash/run/erase/debug/debugserver): a connected device, or a program
on the host, such as an emulator, that runs the firmware. The pbl CLI builds a
:class:`RunnerConfig`, registers runner-specific CLI arguments via
``do_add_parser()``, instantiates the selected runner with ``create()`` and
dispatches a command with ``run()``.
"""

import errno
import os
import shlex
import signal
import socket
import subprocess
import sys
import time
from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from typing import NamedTuple

# Commands a runner may implement.
COMMANDS = ("flash", "run", "erase", "debug", "debugserver")

# How long a debug server gets to start listening for gdb.
GDB_SERVER_TIMEOUT = 15


class RunnerError(Exception):
    pass


class UnsupportedOperation(RunnerError):
    pass


@dataclass
class RunnerCaps:
    """Declares what a runner class can do."""

    commands: set = field(default_factory=lambda: {"flash"})
    # Whether flash() can also program system resources.
    flash_resources: bool = False

    def __post_init__(self):
        invalid = self.commands - set(COMMANDS)
        if invalid:
            raise ValueError(f"invalid runner commands: {sorted(invalid)}")


class RunnerConfig(NamedTuple):
    """Inputs a runner needs to execute a command, independent of the frontend."""

    board_dir: str  # boards/<board>
    soc: str | None = None  # CONFIG_SOC, e.g. "NRF52", "SF32LB52"
    hex_file: str | None = None
    elf_file: str | None = None
    bin_file: str | None = None
    pbpack_file: str | None = None  # None on PRF builds
    firmware_slot: int | None = None  # None without multiple slots
    resources_file: str | None = None
    resources_address: int | None = None  # in the board's flash layout
    erase_ranges: tuple = ()  # (address, size) in the board's flash layout
    sftool: str = "sftool"
    gdb: str | None = None
    gdb_port: int | None = None  # None for the runner's default
    build: object | None = None  # pbl.build.BuildDir, for runners on the host
    extra_args: tuple = ()  # passed on to the program a runner starts
    dry_run: bool = False


class Runner(ABC):
    name = None

    def __init__(self, cfg):
        self.cfg = cfg

    @classmethod
    def capabilities(cls):
        return RunnerCaps()

    @classmethod
    def supports(cls, cfg):
        """Whether the runner can handle the build, e.g. its toolchain."""
        return True

    @classmethod
    def do_add_parser(cls, parser):
        """Add runner-specific CLI arguments. Override as needed."""

    @classmethod
    def add_parser(cls, parser):
        cls.do_add_parser(parser)

    @classmethod
    def create(cls, cfg, args):
        return cls.do_create(cfg, args)

    @classmethod
    def do_create(cls, cfg, args):
        return cls(cfg)

    def run(self, command):
        caps = self.capabilities()
        if command not in caps.commands:
            raise UnsupportedOperation(
                f"{self.name} runner does not support {command}"
            )
        if command == "flash" and self.cfg.resources_file and not caps.flash_resources:
            raise UnsupportedOperation(
                f"{self.name} runner does not support flashing resources"
            )
        return self.do_run(command)

    @abstractmethod
    def do_run(self, command):
        """Concrete runner; run() delegates here once the command is validated."""

    def call(self, cmd):
        """Run a shell command, honoring dry-run and raising RunnerError on failure."""
        if self.cfg.dry_run:
            print("[dry-run] " + cmd)
            return 0
        print(cmd)
        try:
            return subprocess.check_call(cmd, shell=True)
        except subprocess.CalledProcessError as exc:
            raise RunnerError(
                f"Command failed with exit status {exc.returncode}: {cmd}"
            ) from exc

    def run_program(self, argv, cwd=None, env=None):
        """Run a program in the foreground, honoring dry-run; returns its status."""
        cmd = self.quote(*argv)
        if self.cfg.dry_run:
            print("[dry-run] " + cmd)
            return 0
        print(cmd, flush=True)
        return subprocess.call(
            [str(a) for a in argv], cwd=cwd, env=self._env(env)
        )

    def gdb_port(self, default):
        return self.cfg.gdb_port or default

    def gdb_argv(self, port):
        """gdb on the firmware ELF, with the repository's extensions, attached
        to the gdb server on ``port``."""
        from tools.gdb_driver import find_gdb_path

        gdb = self.cfg.gdb or find_gdb_path()
        if gdb is None:
            raise RunnerError("no arm-none-eabi-gdb found, is the SDK installed?")
        if gdb.endswith("-py") and not self.cfg.dry_run and not _runs(gdb):
            # The Python-enabled gdb links against a specific Python version
            print(
                f"warning: {gdb} does not start, falling back to the gdb "
                "without Python and the PebbleOS GDB commands",
                file=sys.stderr,
            )
            gdb = gdb.removesuffix("-py")
        argv = [gdb, self.cfg.elf_file, "-ex", f"target remote :{port}"]
        if self.cfg.build is not None:
            argv[1:1] = ["-ix", os.path.join(self.cfg.build.topdir, ".gdbinit")]
        return argv

    def run_gdb(self, port):
        """Run gdb in the foreground against the server on ``port``."""
        argv = self.gdb_argv(port)
        cwd = self.cfg.build.topdir if self.cfg.build is not None else None
        if self.cfg.dry_run:
            print("[dry-run] " + self.quote(*argv))
            return 0
        print(self.quote(*argv), flush=True)
        # Ctrl-C belongs to gdb, which interrupts the target with it.
        handler = signal.signal(signal.SIGINT, signal.SIG_IGN)
        try:
            return subprocess.call([str(a) for a in argv], cwd=cwd)
        finally:
            signal.signal(signal.SIGINT, handler)

    def run_server_and_gdb(self, server, port, env=None):
        """Start a gdb server, run gdb against it, then stop the server.

        A server already listening on ``port``, e.g. one started with
        ``pbl debugserver``, is used as it is.
        """
        if not self.cfg.dry_run and port_in_use(port):
            print(f"Using the gdb server already on port {port}")
            return self.run_gdb(port)

        if self.cfg.dry_run:
            print("[dry-run] " + self.quote(*server))
            return self.run_gdb(port)

        print(self.quote(*server), flush=True)
        # In its own session, so the Ctrl-C meant for gdb does not reach it.
        proc = subprocess.Popen(
            [str(a) for a in server], env=self._env(env), start_new_session=True
        )
        # Nor a hangup, so it is stopped below even when the terminal goes.
        handlers = {
            sig: signal.signal(sig, _exit_on_signal)
            for sig in (signal.SIGHUP, signal.SIGTERM)
        }
        try:
            deadline = time.monotonic() + GDB_SERVER_TIMEOUT
            while not port_in_use(port):
                if proc.poll() is not None:
                    raise RunnerError(
                        f"{server[0]} exited with status {proc.returncode}"
                    )
                if time.monotonic() > deadline:
                    raise RunnerError(
                        f"{server[0]} did not listen on port {port} in time"
                    )
                time.sleep(0.1)
            return self.run_gdb(port)
        finally:
            proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()
            for sig, handler in handlers.items():
                signal.signal(sig, handler)

    @staticmethod
    def _env(extra):
        return {**os.environ, **extra} if extra else None

    @staticmethod
    def quote(*args):
        return " ".join(shlex.quote(str(a)) for a in args)


def _exit_on_signal(signum, frame):
    raise SystemExit(128 + signum)


def _runs(program):
    try:
        subprocess.run([program, "--version"], capture_output=True, check=True)
    except (OSError, subprocess.CalledProcessError):
        return False
    return True


def port_in_use(port):
    """Whether something on the host listens on TCP ``port``."""
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        try:
            s.bind(("", port))
        except OSError as e:
            return e.errno == errno.EADDRINUSE
    return False
