# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

from pbl import runners
from pbl.command import PblCommand
from pbl.device import make_runner, runner_options
from pbl.errors import CommandContextError


class Run(PblCommand):
    group = "device"

    def __init__(self):
        super().__init__(
            "run",
            "Run the firmware",
            "Run the firmware with the board's runner: start it on the host, "
            "e.g. in an emulator, or flash it to a connected device. "
            "Arguments after '--' go to the program the runner starts.",
            accepts_unknown_args=True,
        )

    def do_add_parser(self, parser_adder):
        parser = self.add_subparser(parser_adder)
        parser.add_argument(
            "--resources",
            action="store_true",
            help="When flashing a device, also flash the system resources",
        )
        runner_options(parser)
        return parser

    def do_run(self, args, unknown):
        if unknown and unknown[0] != "--":
            self.parser.error(f"unrecognized arguments: {' '.join(unknown)}")
        extra_args = unknown[1:]

        build = self.build_dir()
        try:
            runner = make_runner(
                build,
                args,
                ("run", "flash"),
                resources=args.resources,
                extra_args=extra_args,
            )
            if "run" not in runner.capabilities().commands:
                if extra_args:
                    raise CommandContextError(
                        f"the {runner.name} runner flashes the firmware; "
                        "it takes no arguments after '--'"
                    )
                return runner.run("flash")
            if args.resources:
                raise CommandContextError(
                    f"the {runner.name} runner does not flash, --resources "
                    "does not apply"
                )
            return runner.run("run")
        except runners.RunnerError as e:
            raise CommandContextError(str(e)) from e
