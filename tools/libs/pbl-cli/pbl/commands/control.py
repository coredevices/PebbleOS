# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

from pbl.command import PblCommand
from pbl.device import run_on_device, runner_options
from pbl.errors import CommandContextError


class _DeviceCommand(PblCommand):
    """A command that is one runner operation and nothing else."""

    group = "device"
    operation = None

    def do_add_parser(self, parser_adder):
        parser = self.add_subparser(parser_adder)
        runner_options(parser)
        return parser

    def do_run(self, args, unknown):
        run_on_device(self.build_dir(), args, self.operation)


class Reset(_DeviceCommand):
    operation = "reset"

    def __init__(self):
        super().__init__("reset", "Reset a connected device")


class Run(_DeviceCommand):
    operation = "run"

    def __init__(self):
        super().__init__("run", "Start or resume execution on a connected device")


class Bork(_DeviceCommand):
    operation = "erase"

    def __init__(self):
        super().__init__("bork", "Reset and wipe a connected device")


class OpenOcd(_DeviceCommand):
    operation = "debugserver"

    def __init__(self):
        super().__init__(
            "openocd",
            "Start a debug server and leave it running",
            "Start the debug server, resetting the board first to improve the "
            "chances of attaching successfully.",
        )


def _int(text, what):
    try:
        return int(text, 0)
    except ValueError:
        raise CommandContextError(f"invalid {what} '{text}'") from None


class Erase(PblCommand):
    group = "device"

    def __init__(self):
        super().__init__(
            "erase",
            "Erase parts of a connected device's flash",
            "Erase regions of the board's flash layout, e.g. FILESYSTEM, or address "
            "ranges in it. pbl bork erases everything instead.",
        )

    def do_add_parser(self, parser_adder):
        parser = self.add_subparser(parser_adder)
        parser.add_argument(
            "--region",
            action="append",
            default=[],
            metavar="NAME[:SIZE]",
            help="Erase a flash region, or only its first SIZE bytes (repeatable)",
        )
        parser.add_argument(
            "--range",
            action="append",
            default=[],
            metavar="ADDRESS:SIZE",
            help="Erase SIZE bytes from ADDRESS (repeatable)",
        )
        runner_options(parser)
        return parser

    def do_run(self, args, unknown):
        build = self.build_dir()
        ranges = []
        for spec in args.region:
            name, sep, size = spec.partition(":")
            address, region_size = build.flash_region(name)
            if sep:
                size = _int(size, "size")
                if not 0 < size <= region_size:
                    raise CommandContextError(
                        f"size {size:#x} outside region {name} ({region_size:#x})"
                    )
                region_size = size
            ranges.append((address, region_size))
        for spec in args.range:
            address, sep, size = spec.partition(":")
            if not sep:
                raise CommandContextError(
                    f"invalid range '{spec}': expected ADDRESS:SIZE"
                )
            ranges.append((_int(address, "address"), _int(size, "size")))
        if not ranges:
            raise CommandContextError("nothing to erase: give --region or --range")
        run_on_device(build, args, "erase", erase_ranges=ranges)
