# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import logging
import os
import shlex
import subprocess
import time

from harness.connections import Capability
from harness.device import DeviceAdapter
from harness.errors import HarnessError, PromptError, Unsupported

logger = logging.getLogger(__name__)

# How long the firmware takes to come back after power is applied.
POWER_ON_SETTLE_S = 1.0
# How long the watch is left unpowered when repowering it.
POWER_OFF_S = 1.0
# How often a command run on a repowered watch is tried.
REPOWERED_ATTEMPTS = 3
# The smallest erasable unit, all a slot header needs.
SLOT_HEADER_SIZE = 0x1000
# How long a USB console may take to come back after the watch resets.
CONSOLE_RETURN_S = 15.0
CONSOLE_RETRY_S = 0.25
# The dialog a watch booting on a charger raises, and how long it may take to.
CHARGING_DIALOG = "Battery Status"
CHARGING_DIALOG_S = 5.0
CHARGING_POLL_S = 0.25


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
        baud = self.config.serial_baud or self.build.config.get(
            "CONFIG_CONSOLE_BAUDRATE"
        )
        return [f"serial:{tty}@{baud}" if baud else f"serial:{tty}"]

    def _flash_command(self):
        if self.config.flash_command:
            return shlex.split(self.config.flash_command)
        command = ["pbl", "-b", self.build.path, "run"]
        if self.build.variant != "prf":
            command.append("--resources")
        elif not self._has_slots:
            command.append("--provision")
        if self.config.serial:
            command += ["--tty", self.config.serial[0]]
        return command

    def connect(self):
        """Open the connections. A USB console goes away while the watch
        resets and comes back as it boots: it is retried meanwhile."""
        import serial

        deadline = time.monotonic() + CONSOLE_RETURN_S
        while True:
            try:
                self._hold_console()
                super().connect()
                return
            except (OSError, serial.SerialException):
                self._release_console()
                if time.monotonic() > deadline:
                    raise
                time.sleep(CONSOLE_RETRY_S)

    def wait_ready(self, timeout=None):
        super().wait_ready(timeout)
        self._close_charging_dialog()

    def _is_plugged(self):
        try:
            lines = self.prompt("battery status")
        except (PromptError, Unsupported):
            return False
        return any(line.strip() == "plugged: YES" for line in lines)

    def _close_charging_dialog(self):
        """A watch charging from the harness' cable, as one whose console is
        its USB port, raises the charging dialog as it boots. Its animation
        keeps the screen changing and the CPU busy, so it is closed as a user
        would."""
        if not self.has(Capability.PROMPT) or not self._is_plugged():
            return
        from harness.helpers.ui import Button, Ui

        ui = Ui(self)
        deadline = time.monotonic() + CHARGING_DIALOG_S
        while time.monotonic() < deadline:
            if CHARGING_DIALOG in ui.modal_stack():
                ui.press(Button.BACK)
                logger.info("closed the charging dialog")
                return
            time.sleep(CHARGING_POLL_S)

    def _hold_console(self):
        """Keep the debug port open, RTS released, while the harness runs.
        RTS resets the watch, and the OS asserts it whenever the port is
        first opened: without this, every reconnect restarted the watch."""
        if not self.config.serial:
            return
        import serial

        if self._console_holder is not None:
            try:
                # A USB console that went away leaves the port dead
                _ = self._console_holder.in_waiting
                return
            except (OSError, serial.SerialException):
                self._release_console()

        holder = serial.Serial()
        holder.port = self.config.serial[0]
        holder.rts = False
        holder.open()
        self._console_holder = holder

    def _release_console(self):
        if self._console_holder is not None:
            try:
                self._console_holder.close()
            except OSError:
                pass
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
                    supply.off()
                    time.sleep(POWER_OFF_S)
                    supply.on()
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

    @property
    def _has_slots(self):
        """Whether the bootloader boots the firmware from slots, falling back
        to PRF, rather than copying the firmware or PRF into place, which
        flashing PRF with --provision sets up."""
        return bool(self.build.config.get("CONFIG_PBLBOOT"))

    def flash(self):
        if self.build.variant == "prf" and self._has_slots:
            self.invalidate_firmware_slots()
        self._run_repowered(self._flash_command(), "flash")

    def _erase_regions(self, regions, what):
        """Erase flash regions (``NAME`` or ``NAME:SIZE``) with the board's
        runner."""
        command = ["pbl", "-b", self.build.path, "erase"]
        for region in regions:
            command += ["--region", region]
        if self.config.serial:
            command += ["--tty", self.config.serial[0]]
        self._run_repowered(command, what)

    def invalidate_firmware_slots(self):
        """Erase the normal firmware slots' headers, as PRF does when it
        boots: the bootloader only falls back to PRF without a valid slot."""
        regions = [
            f"{name}:{SLOT_HEADER_SIZE:#x}"
            for name in ("FIRMWARE_SLOT_0", "FIRMWARE_SLOT_1")
        ]
        self._erase_regions(regions, "invalidate")

    def boot_recovery(self):
        """Boot PRF again, e.g. after a test installed the normal firmware."""
        # The runner may need the serial port the connections hold.
        self.disconnect()
        if self._has_slots:
            self.invalidate_firmware_slots()
            if not self._hard_reset():
                raise HarnessError("booting PRF needs a power supply")
        else:
            self._run_repowered(self._flash_command(), "flash")
        self.connect()
        self.wait_ready()

    def erase_filesystem(self):
        """Erase the filesystem (bondings, settings, apps and data), and the
        bonding kept for PRF, which the firmware restores from otherwise."""
        self._erase_regions(["FILESYSTEM", "SHARED_PRF_STORAGE"], "erase")

    def wipe(self):
        # The runner may need the serial port the connections hold.
        self.disconnect()
        self.erase_filesystem()
        if not self._hard_reset():
            raise HarnessError("booting after an erase needs a power supply")
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
            self.config.power_supply.on()
            time.sleep(POWER_ON_SETTLE_S)

    def _close_device(self):
        self._release_console()

    def _hard_reset(self):
        supply = self.config.power_supply
        if supply is None:
            return False
        supply.cycle(POWER_OFF_S)
        time.sleep(POWER_ON_SETTLE_S)
        return True
