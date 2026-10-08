# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import logging
import os
import shlex
import subprocess
import time

from harness.device import DeviceAdapter
from harness.errors import HarnessError

logger = logging.getLogger(__name__)

# How long the firmware takes to come back after power is applied.
POWER_ON_SETTLE_S = 1.0
# How long the watch is left unpowered when repowering it.
POWER_OFF_S = 1.0
# How often a command run on a repowered watch is tried.
REPOWERED_ATTEMPTS = 3
# The smallest erasable unit, all a slot header needs.
FLASH_SUBSECTOR_SIZE = 0x1000


class HardwareAdapter(DeviceAdapter):
    """A real watch, optionally flashed and powered by the harness."""

    type = "hardware"
    _console_holder = None

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

    def connect(self):
        self._hold_console()
        super().connect()

    def _hold_console(self):
        """Keep the debug port open, RTS released, while the harness runs.
        RTS resets the watch, and the OS asserts it whenever the port is
        first opened: without this, every reconnect restarted the watch."""
        if self._console_holder is not None or not self.config.serial:
            return
        import serial

        holder = serial.Serial()
        holder.port = self.config.serial[0]
        holder.rts = False
        holder.open()
        self._console_holder = holder

    def _release_console(self):
        if self._console_holder is not None:
            self._console_holder.close()
            self._console_holder = None

    def _run_repowered(self, command, what):
        """Run ``command`` against the watch. With a power supply the watch
        is repowered and ``command`` started right away, before the firmware
        can deep sleep, which leaves its debug UART reachable only at random;
        missing the boot ROM's window that way is retried."""
        # sftool drives RTS itself.
        self._release_console()
        supply = self.config.power_supply
        attempts = REPOWERED_ATTEMPTS if supply is not None else 1
        log = os.path.join(self.config.results_dir, f"{what}.log")
        with open(log, "w") as f:
            for attempt in range(attempts):
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
                if result.returncode == 0:
                    return
                logger.warning(
                    "%s failed (%d), attempt %d", what, result.returncode, attempt + 1
                )
        raise HarnessError(f"{what} failed ({result.returncode}); see {log}")

    def flash(self):
        if self.build.variant == "prf":
            self.invalidate_firmware_slots()
        self._run_repowered(self._flash_command(), "flash")

    def _erase_regions(self, regions, what, purpose):
        sftool = self.build.tool("sftool")
        soc = self.build.config.get("CONFIG_SOC")
        if not (sftool and soc and self.config.serial):
            raise HarnessError(
                f"{purpose} needs sftool and --device-serial (board {self.build.board})"
            )
        command = [sftool, "-c", soc, "-p", self.config.serial[0], "erase_region"]
        command += [f"{address:#x}:{size:#x}" for address, size in regions]
        self._run_repowered(command, what)

    def invalidate_firmware_slots(self):
        """Erase the normal firmware slots' headers, as PRF does when it
        boots: the bootloader only falls back to PRF without a valid slot."""
        regions = []
        for name in ("FIRMWARE_SLOT_0", "FIRMWARE_SLOT_1"):
            address, _ = self.build.flash_region(name)
            regions.append((address, FLASH_SUBSECTOR_SIZE))
        self._erase_regions(regions, "invalidate", "booting PRF")

    def boot_recovery(self):
        """Boot PRF again, e.g. after a test installed the normal firmware."""
        # sftool needs the serial port the connections hold.
        self.disconnect()
        self.invalidate_firmware_slots()
        if not self._hard_reset():
            raise HarnessError("booting PRF needs a power supply (--ppk2)")
        self.connect()
        self.wait_ready()

    def erase_filesystem(self):
        """Erase the filesystem (bondings, settings, apps and data), and the
        bonding kept for PRF, which the firmware restores from otherwise."""
        regions = [self.build.flash_region("FILESYSTEM")]
        try:
            regions.append(self.build.flash_region("SHARED_PRF_STORAGE"))
        except HarnessError:
            pass
        self._erase_regions(regions, "erase", "erasing the filesystem")

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
        self._release_console()

    def _hard_reset(self):
        supply = self.config.power_supply
        if supply is None:
            return False
        supply.power_cycle(POWER_OFF_S)
        time.sleep(POWER_ON_SETTLE_S)
        return True
