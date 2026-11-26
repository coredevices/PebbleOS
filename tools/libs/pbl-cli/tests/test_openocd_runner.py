# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import os
import subprocess
import sys
import unittest
from unittest import mock

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from pbl.runners import openocd
from pbl.runners.core import RunnerError


def _banner(text):
    return subprocess.CompletedProcess(
        ["openocd", "--version"], 0, stdout="", stderr=text
    )


class TestOpenOcdVersion(unittest.TestCase):
    def check(self, banner):
        with mock.patch.object(openocd.subprocess, "run", return_value=_banner(banner)):
            openocd._check_openocd_version()

    def test_accepts_the_minimum(self):
        self.check("Open On-Chip Debugger 0.12.0\nLicensed under GNU GPL v2\n")

    def test_accepts_development_builds_after_it(self):
        self.check(
            "Open On-Chip Debugger 0.12.0+dev-01850-g8f2f6c3 (2026-11-10-12:00)\n"
        )
        self.check("Open On-Chip Debugger 1.0.0\n")

    def test_rejects_older_versions(self):
        with self.assertRaisesRegex(RunnerError, "0.11.0 is too old: 0.12.0 or later"):
            self.check("Open On-Chip Debugger 0.11.0\n")

    def test_lets_an_unknown_banner_through(self):
        self.check("xPack OpenOCD something\n")

    def test_reports_a_missing_openocd(self):
        with (
            mock.patch.object(openocd.subprocess, "run", side_effect=FileNotFoundError),
            self.assertRaisesRegex(RunnerError, "not found"),
        ):
            openocd._check_openocd_version()


if __name__ == "__main__":
    unittest.main()
