# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import os
import sys

from pbl.command import PblCommand


class ITest(PblCommand):
    group = "build"

    def __init__(self):
        super().__init__(
            "itest",
            "Run the integration tests",
            "Run the pytest integration tests in tests/integration against the "
            "build, on the emulator or a real device. Anything this command "
            "does not recognize is passed straight to pytest, from "
            "tests/integration, e.g. --device-serial TTY, -m smoke, -k settings "
            "or ui/test_navigation.py.",
            accepts_unknown_args=True,
        )

    def do_add_parser(self, parser_adder):
        parser = self.add_subparser(parser_adder)
        parser.add_argument(
            "--no-build",
            action="store_true",
            help="Use the emulator's flash images as they are instead of building them",
        )
        return parser

    def do_run(self, args, unknown):
        build = self.build_dir()
        if build.config.CONFIG_QEMU and not args.no_build:
            self.cmake_build(
                build, "qemu_image_micro", "qemu_image_spi", msg="QEMU images failed"
            )

        tests = os.path.join(self.topdir, "tests", "integration")
        # Load the harness up front: pytest only loads conftest.py early when
        # no argument looks like a path outside tests/integration (a tty does).
        env = dict(os.environ)
        env["PYTHONPATH"] = os.pathsep.join(
            p for p in (tests, env.get("PYTHONPATH")) if p
        )
        return self.run_cmd(
            [
                sys.executable,
                "-m",
                "pytest",
                "-c",
                os.path.join(tests, "pytest.ini"),
                "--rootdir",
                tests,
                "-p",
                "harness.plugin",
                f"--build-dir={build}",
                *unknown,
            ],
            cwd=tests,
            env=env,
        )
