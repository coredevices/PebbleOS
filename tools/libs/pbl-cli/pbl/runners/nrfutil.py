# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import os
import tempfile

from .core import Runner, RunnerCaps, UnsupportedOperation

# Where the nRF52840 maps its QSPI flash, the addresses nrfutil programs it at.
QSPI_XIP_BASE = 0x12000000


class NrfUtilRunner(Runner):
    name = "nrfutil"
    lab_ports = ("probe",)

    @classmethod
    def capabilities(cls):
        return RunnerCaps(
            commands={"flash", "erase"},
            flash_data=True,
            erase_code=True,
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

    def _program_data(self, file, offset):
        with tempfile.TemporaryDirectory() as tmp:
            hex_file = os.path.join(tmp, "data.hex")
            if not self.cfg.dry_run:
                from intelhex import IntelHex

                image = IntelHex()
                image.loadbin(file, offset=QSPI_XIP_BASE + offset)
                image.write_hex_file(hex_file)
            self._nrfutil(
                f"program --firmware {self.quote(hex_file)} --options "
                "chip_erase_mode=ERASE_NONE,"
                "ext_mem_erase_mode=ERASE_RANGES_TOUCHED_BY_FIRMWARE,verify=VERIFY_READ",
                ext_mem=True,
            )

    def do_run(self, command):
        if command == "flash":
            if self.cfg.data_files:
                self._check_ext_mem("write files to the flash")
            if self.cfg.erase_code:
                # The end address is inclusive
                address, size = self.cfg.erase_code
                self._nrfutil(f"erase --pages {address:#x}-{address + size - 1:#x}")
            for hex_file in self.cfg.flash_files:
                self._program(hex_file)
            for file, offset in self.cfg.data_files:
                self._program_data(file, offset)
            self._nrfutil("reset")
        elif command == "erase":
            self._erase_ranges()
