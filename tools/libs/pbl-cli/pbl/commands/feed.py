# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

from pbl import emulator
from pbl.command import global_options
from pbl.commands.qemu import _QemuCommand
from pbl.feeds import Watch, builtin_feeds


class Feed(_QemuCommand):
    def __init__(self):
        super().__init__(
            "feed",
            "Feed the emulator simulated phone data",
            "Write into the running emulator what the phone app would, so "
            "features that depend on the phone can be exercised without one.",
        )
        self.feeds = builtin_feeds()

    def do_add_parser(self, parser_adder):
        parser = self.add_subparser(parser_adder)
        parser.add_argument(
            "--host",
            default=f"127.0.0.1:{emulator.PEBBLE_TOOL_PORT}",
            help="host:port of the emulator's Pebble protocol serial port "
            "(default: %(default)s)",
        )
        subparsers = parser.add_subparsers(dest="feed", metavar="FEED", required=True)
        for feed in self.feeds.values():
            subparser = subparsers.add_parser(
                feed.name,
                help=feed.help,
                description=feed.description or feed.help,
                parents=[global_options()],
            )
            feed.add_arguments(subparser)
        return parser

    def do_run(self, args, unknown):
        self.emulated_build()
        feed = self.feeds[args.feed]

        if self.dry_run:
            watch = Watch()
        else:
            host, _, port = args.host.rpartition(":")
            watch = Watch(emulator.connect_watch(host or "127.0.0.1", int(port)))

        try:
            feed.run(args, watch, self.inf)
        except ValueError as e:
            self.parser.error(str(e))
