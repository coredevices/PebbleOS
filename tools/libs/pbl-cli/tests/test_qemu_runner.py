# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import os
import socket
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from pbl import emulator
from pbl.runners.core import RunnerConfig, RunnerError
from pbl.runners.qemu import QemuRunner


class FakeBuild:
    def __init__(self, path):
        self.path = path

    def join(self, *parts):
        return os.path.join(self.path, *parts)


class TestClaimSockets(unittest.TestCase):
    def setUp(self):
        # macOS limits socket paths to 104 bytes, so not TMPDIR
        self.temp = tempfile.TemporaryDirectory(dir="/tmp")
        self.addCleanup(self.temp.cleanup)
        self.build = FakeBuild(self.temp.name)

    def runner(self, dry_run=False):
        return QemuRunner(RunnerConfig(board_dir="", build=self.build, dry_run=dry_run))

    def bind(self, name, listen):
        sock = socket.socket(socket.AF_UNIX)
        self.addCleanup(sock.close)
        sock.bind(self.build.join(name))
        if listen:
            sock.listen()
        else:
            sock.close()

    def test_no_sockets(self):
        self.runner()._claim_sockets(self.build)

    def test_stale_sockets_are_removed(self):
        for name in (emulator.MONITOR_SOCKET, emulator.QMP_SOCKET):
            self.bind(name, listen=False)
        self.runner()._claim_sockets(self.build)
        for name in (emulator.MONITOR_SOCKET, emulator.QMP_SOCKET):
            self.assertFalse(os.path.lexists(self.build.join(name)))

    def test_dry_run_keeps_stale_sockets(self):
        self.bind(emulator.MONITOR_SOCKET, listen=False)
        self.runner(dry_run=True)._claim_sockets(self.build)
        self.assertTrue(os.path.lexists(self.build.join(emulator.MONITOR_SOCKET)))

    def test_running_emulator_is_refused(self):
        for name in (emulator.MONITOR_SOCKET, emulator.QMP_SOCKET):
            with self.subTest(name=name):
                self.bind(name, listen=True)
                with self.assertRaisesRegex(RunnerError, "already running"):
                    self.runner()._claim_sockets(self.build)
                self.assertTrue(os.path.lexists(self.build.join(name)))
                os.unlink(self.build.join(name))

    def test_non_socket_is_kept(self):
        path = self.build.join(emulator.MONITOR_SOCKET)
        with open(path, "w") as f:
            f.write("not a socket")
        with self.assertRaisesRegex(RunnerError, "not a socket"):
            self.runner()._claim_sockets(self.build)
        with open(path) as f:
            self.assertEqual(f.read(), "not a socket")


if __name__ == "__main__":
    unittest.main()
