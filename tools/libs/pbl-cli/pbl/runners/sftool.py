# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

from .core import Runner, RunnerCaps, RunnerError


class SfToolRunner(Runner):
    name = "sftool"
    lab_ports = ("serial",)

    @classmethod
    def capabilities(cls):
        return RunnerCaps(commands={"flash", "erase"}, flash_resources=True)

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
        if command == "flash":
            files = [self.cfg.hex_file]
            if self.cfg.resources_file:
                files.append(
                    f"{self.cfg.resources_file}@{self.cfg.resources_address:#x}"
                )
            self._sftool("write_flash " + self.quote(*files))
        elif command == "erase":
            ranges = [f"{a:#x}:{s:#x}" for a, s in self.cfg.erase_ranges]
            self._sftool("erase_region " + self.quote(*ranges))
