# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

from .core import Runner, RunnerCaps, RunnerError


def flash_xip_base(build):
    """Where the SoC maps the boot flash, the addresses sftool programs it at."""
    return build.config.CONFIG_FLASH_BASE


class SfToolRunner(Runner):
    name = "sftool"
    lab_ports = ("serial",)

    @classmethod
    def capabilities(cls):
        return RunnerCaps(commands={"flash", "erase"}, flash_data=True)

    @classmethod
    def do_add_parser(cls, parser):
        parser.add_argument(
            "--tty",
            help="Serial port for the sftool and probe-rs runners (default: the lab's)",
        )

    def _sftool(self, command):
        tty = self.cfg.tty
        if not tty:
            raise RunnerError("Port not specified, use --tty or a lab file")
        self.call(
            f"{self.quote(self.cfg.sftool, '-c', self.cfg.soc, '-p', tty)} {command}"
        )

    def do_run(self, command):
        build = self.cfg.build
        if not build:
            raise RunnerError("sftool needs the build, for the flash address")
        base = flash_xip_base(build)
        if command == "flash":
            files = list(self.cfg.flash_files)
            files += [f"{f}@{base + offset:#x}" for f, offset in self.cfg.data_files]
            self._sftool("write_flash " + self.quote(*files))
        elif command == "erase":
            ranges = [f"{base + a:#x}:{s:#x}" for a, s in self.cfg.erase_ranges]
            self._sftool("erase_region " + self.quote(*ranges))
