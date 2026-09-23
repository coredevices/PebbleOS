# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import os
import shlex
import subprocess

from harness.device import DeviceAdapter
from harness.errors import HarnessError


class HardwareAdapter(DeviceAdapter):
    """A real watch, optionally flashed by the harness."""

    type = "hardware"

    def default_connections(self):
        # The first serial port is the debug console; any further ones are
        # given explicitly as --connection.
        if not self.config.serial:
            return []
        tty = self.config.serial[0]
        if self.build.config.get("CONFIG_PULSE_EVERYWHERE"):
            return [f"pulse:{tty}"]
        return [f"serial:{tty}@{self.config.serial_baud}"]

    def _flash_command(self):
        if self.config.flash_command:
            return shlex.split(self.config.flash_command)
        command = ["pbl", "-b", self.build.path, "flash"]
        if self.build.variant != "prf":
            command.append("--resources")
        if self.config.serial:
            command += ["--tty", self.config.serial[0]]
        return command

    def flash(self):
        command = self._flash_command()
        log = os.path.join(self.config.results_dir, "flash.log")
        with open(log, "w") as f:
            f.write(shlex.join(command) + "\n")
            f.flush()
            result = subprocess.run(
                command,
                cwd=self.build.topdir,
                stdout=f,
                stderr=subprocess.STDOUT,
                check=False,
            )
        if result.returncode != 0:
            raise HarnessError(f"flashing failed ({result.returncode}); see {log}")

    def _device_launch(self):
        if self.config.flash_before:
            self.flash()

    def _close_device(self):
        pass
