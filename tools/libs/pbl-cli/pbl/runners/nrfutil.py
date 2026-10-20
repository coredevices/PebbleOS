# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import os
import tempfile

from .core import Runner, RunnerCaps, UnsupportedOperation

# Where the nRF52840 maps its QSPI flash, the addresses nrfutil programs it at.
QSPI_XIP_BASE = 0x12000000


class NrfUtilRunner(Runner):
    name = "nrfutil"

    @classmethod
    def capabilities(cls):
        return RunnerCaps(
            commands={"flash", "run", "reset", "erase"},
            flash_resources=True,
            erase_ranges=True,
        )

    @property
    def _ext_mem_config(self):
        return os.path.join(self.cfg.board_dir, "support", "nrfutil_ext_mem.json")

    def _nrfutil(self, command, ext_mem=False):
        prefix = "nrfutil device "
        if ext_mem:
            prefix += f"--x-ext-mem-config-file {self.quote(self._ext_mem_config)} "
        # Only debug probes: other Nordic USB devices, e.g. a PPK2, enumerate too
        self.call(f"{prefix}{command} --traits jlink")

    def _program(self, hex_file):
        self._nrfutil(
            f"program --firmware {self.quote(hex_file)} "
            "--options chip_erase_mode=ERASE_RANGES_TOUCHED_BY_FIRMWARE"
        )

    def _check_ext_mem(self, what):
        if not os.path.exists(self._ext_mem_config):
            raise UnsupportedOperation(
                f"the board has no support/nrfutil_ext_mem.json to {what}"
            )

    def _erase_ranges(self):
        self._check_ext_mem("erase ranges")
        for address, size in self.cfg.erase_ranges:
            # The end address is inclusive
            start = QSPI_XIP_BASE + address
            end = start + size - 1
            self._nrfutil(f"erase --pages {start:#x}-{end:#x}", ext_mem=True)

    def _program_resources(self):
        with tempfile.TemporaryDirectory() as tmp:
            hex_file = os.path.join(tmp, "resources.hex")
            if not self.cfg.dry_run:
                from intelhex import IntelHex

                image = IntelHex()
                image.loadbin(
                    self.cfg.resources_file,
                    offset=QSPI_XIP_BASE + self.cfg.resources_address,
                )
                image.write_hex_file(hex_file)
            self._nrfutil(
                f"program --firmware {self.quote(hex_file)} --options "
                "chip_erase_mode=ERASE_NONE,"
                "ext_mem_erase_mode=ERASE_RANGES_TOUCHED_BY_FIRMWARE,verify=VERIFY_READ",
                ext_mem=True,
            )

    def do_run(self, command):
        if command == "flash":
            if self.cfg.resources_file:
                self._check_ext_mem("flash resources")
            self._program(self.cfg.hex_file)
            if self.cfg.resources_file:
                self._program_resources()
            self._nrfutil("reset")
        elif command in ("run", "reset"):
            self._nrfutil("reset")
        elif command == "erase" and self.cfg.erase_ranges:
            self._erase_ranges()
        elif command == "erase":
            self._nrfutil("erase")
