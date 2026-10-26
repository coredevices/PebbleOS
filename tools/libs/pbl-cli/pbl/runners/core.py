# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Core binary-runner abstraction, modeled on Zephyr's west runners.

A runner knows how to talk to a target for a small set of commands
(flash/run/reset/erase/debug/debugserver): a connected device, or a program
on the host, such as an emulator, that runs the firmware. The pbl CLI builds a
:class:`RunnerConfig`, registers runner-specific CLI arguments via
``do_add_parser()``, instantiates the selected runner with ``create()`` and
dispatches a command with ``run()``.
"""

import shlex
import subprocess
from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from typing import NamedTuple

# Commands a runner may implement.
COMMANDS = ("flash", "run", "reset", "erase", "debug", "debugserver")


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
    # Whether erase() can erase address ranges instead of the whole device.
    erase_ranges: bool = False

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
        if command == "erase" and self.cfg.erase_ranges and not caps.erase_ranges:
            raise UnsupportedOperation(
                f"{self.name} runner does not support erasing ranges"
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

    def run_program(self, argv, cwd=None):
        """Run a program in the foreground, honoring dry-run; returns its status."""
        cmd = self.quote(*argv)
        if self.cfg.dry_run:
            print("[dry-run] " + cmd)
            return 0
        print(cmd, flush=True)
        return subprocess.call([str(a) for a in argv], cwd=cwd)

    @staticmethod
    def quote(*args):
        return " ".join(shlex.quote(str(a)) for a in args)
