# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import os
import sys
import tempfile
import unittest
from unittest import mock

tools_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))
sys.path.insert(0, os.path.join(tools_dir, "fuel_gauge"))

import bench
import fuel_gauge_log
from instruments import npm1300_ek


class FakeShell:
    """The EK shell: records commands, reports full right away and ends the
    discharge after one sample."""

    def __init__(self, port):
        self.commands = []
        self.on_log = lambda *a: None
        self._s = mock.Mock()

    def reboot(self):
        pass

    def cmd(self, command, timeout=5.0):
        self.commands.append(command)
        if command == "cc_profile start":
            self.on_log(1.0, "inf", "module_cc_profiling", "vload=3.9,iload=-0.035")
            self.on_log(2.0, "inf", "module_cc_profiling", "vcutoff reached")
        return "Success: uA."

    def value(self, command):
        self.commands.append(command)
        return 0x02 if command == "npmx charger status all get" else 1.0

    def poll(self, seconds):
        pass


class TestBench(unittest.TestCase):
    def test_npm1300_ek(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = os.path.join(tmp, "cell.csv")
            with (
                mock.patch.object(npm1300_ek, "Shell", FakeShell),
                mock.patch("builtins.input"),
                mock.patch.object(npm1300_ek.Npm1300Ek, "close"),
                mock.patch.object(npm1300_ek.time, "sleep"),
            ):
                captured = []
                real = npm1300_ek.Npm1300Ek.__init__

                def init(self, args):
                    real(self, args)
                    captured.append(self._sh)

                with mock.patch.object(npm1300_ek.Npm1300Ek, "__init__", init):
                    bench.main(
                        [
                            "npm1300-ek",
                            "/dev/null",
                            out,
                            "--capacity-mah",
                            "70",
                            "--vterm",
                            "4.45",
                            "--ichg",
                            "70",
                            "--ntc",
                            "10k",
                        ]
                    )
            log = fuel_gauge_log.read_log(out)

        commands = captured[0].commands
        self.assertIn("npmx charger termination_voltage normal set 4450", commands)
        self.assertIn("npmx charger charging_current set 70000", commands)
        self.assertIn("npmx charger termination_current set 10", commands)
        self.assertIn("npmx adc ntc beta set 3380", commands)
        self.assertIn(
            'cc_profile profile set 1000 8000 3.000 "1000,1000,0,0,900" '
            '"144000,900000,0.0350,0,NaN,3.200" "576000,900000,0.0088,0,NaN"',
            commands,
        )
        self.assertEqual(list(log.v), [3.9])
        self.assertEqual(list(log.i), [0.035])


if __name__ == "__main__":
    unittest.main()
