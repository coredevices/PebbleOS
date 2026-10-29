# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

from pbl.command import PblCommand
from pbl.device import run_on_device, runner_options


class _DebugCommand(PblCommand):
    group = "device"

    def do_add_parser(self, parser_adder):
        parser = self.add_subparser(parser_adder)
        parser.add_argument(
            "--gdb-port",
            type=int,
            metavar="PORT",
            help="Port of the gdb server (default: the runner's)",
        )
        runner_options(parser)
        return parser

    def do_run(self, args, unknown):
        run_on_device(self.build_dir(), args, self.name)


class Debug(_DebugCommand):
    def __init__(self):
        super().__init__(
            "debug",
            "Attach gdb to the firmware",
            "Start the runner's gdb server and attach gdb to the running "
            "firmware. A server already listening on the gdb port, e.g. one "
            "started with 'pbl debugserver', is used instead.",
        )


class DebugServer(_DebugCommand):
    def __init__(self):
        super().__init__(
            "debugserver",
            "Start a gdb server for the firmware",
            "Start the runner's gdb server in the foreground, for gdb, an IDE "
            "or 'pbl debug' to attach to.",
        )
