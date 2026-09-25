# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import os
import shlex
import subprocess
import time

from harness.device import DeviceAdapter
from harness.errors import HarnessError

# How long the firmware takes to come back after power is applied.
POWER_ON_SETTLE_S = 1.0
# How long the watch is left unpowered when repowering it.
POWER_OFF_S = 1.0


class HardwareAdapter(DeviceAdapter):
    """A real watch, optionally flashed and powered by the harness."""

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

    def _run_repowered(self, command, what):
        """Run ``command`` against the watch. With a power supply the watch
        is repowered and ``command`` started right away, before the firmware
        can deep sleep, which leaves its debug UART reachable only at random."""
        supply = self.config.power_supply
        log = os.path.join(self.config.results_dir, f"{what}.log")
        with open(log, "w") as f:
            f.write(shlex.join(command) + "\n")
            f.flush()
            if supply is not None:
                supply.power_off()
                time.sleep(POWER_OFF_S)
                supply.power_on()
            result = subprocess.run(
                command,
                cwd=self.build.topdir,
                stdout=f,
                stderr=subprocess.STDOUT,
                check=False,
            )
        if result.returncode != 0:
            raise HarnessError(f"{what} failed ({result.returncode}); see {log}")

    def flash(self):
        self._run_repowered(self._flash_command(), "flash")

    def erase_filesystem(self):
        """Erase the filesystem (bondings, settings, apps and data), and the
        bonding kept for PRF, which the firmware restores from otherwise."""
        sftool = self.build.tool("sftool")
        soc = self.build.config.get("CONFIG_SOC")
        if not (sftool and soc and self.config.serial):
            raise HarnessError(
                f"erasing the filesystem needs sftool and --device-serial (board {self.build.board})"
            )
        regions = [self.build.flash_region("FILESYSTEM")]
        try:
            regions.append(self.build.flash_region("SHARED_PRF_STORAGE"))
        except HarnessError:
            pass
        command = [sftool, "-c", soc, "-p", self.config.serial[0], "erase_region"]
        command += [f"{address:#x}:{size:#x}" for address, size in regions]
        self._run_repowered(command, "erase")

    def wipe(self):
        # sftool needs the serial port the connections hold.
        self.disconnect()
        self.erase_filesystem()
        if not self._hard_reset():
            raise HarnessError("booting after an erase needs a power supply (--ppk2)")
        self.connect()
        self.wait_ready()

    def _device_launch(self):
        if self.config.erase_fs:
            self.erase_filesystem()
        if self.config.flash_before:
            self.flash()
        elif self.config.erase_fs and self._hard_reset():
            pass
        elif self.config.power_supply is not None:
            self.config.power_supply.power_on()
            time.sleep(POWER_ON_SETTLE_S)

    def _close_device(self):
        pass

    def _hard_reset(self):
        supply = self.config.power_supply
        if supply is None:
            return False
        supply.power_cycle(POWER_OFF_S)
        time.sleep(POWER_ON_SETTLE_S)
        return True
