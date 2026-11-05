# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import itertools
import os

from pbl import emulator, mic
from pbl.command import PblCommand, global_options
from pbl.feeds import Watch, builtin_feeds


class Emulator(PblCommand):
    group = "device"

    def __init__(self):
        super().__init__(
            "emulator",
            "Interact with the running emulator",
            "Capture the screen of, inject touches into, play audio into the "
            "microphone of, or feed phone data to the emulator 'pbl run' is "
            "running: QEMU, or the firmware as a native program.",
        )
        self.feeds = builtin_feeds()

    def do_add_parser(self, parser_adder):
        parser = self.add_subparser(parser_adder)
        subparsers = parser.add_subparsers(
            dest="action", metavar="ACTION", required=True
        )

        def add(name, help, description=None):
            return subparsers.add_parser(
                name,
                help=help,
                description=description or help,
                parents=[global_options()],
            )

        screenshot = add("screenshot", "Capture the screen to a PNG")
        screenshot.add_argument(
            "-o",
            "--output",
            type=os.path.abspath,
            help="Where to write the PNG (default: screenshot.png in the build)",
        )

        touch = add("touch", "Tap the screen at a pixel")
        touch.add_argument("x", type=int, help="x coordinate in screen pixels")
        touch.add_argument("y", type=int, help="y coordinate in screen pixels")
        touch.add_argument(
            "--duration",
            type=float,
            default=0.05,
            help="how long the finger stays down, in seconds (default: %(default)s)",
        )

        swipe = add("swipe", "Swipe across the screen")
        swipe.add_argument("x1", type=int, help="start x in screen pixels")
        swipe.add_argument("y1", type=int, help="start y in screen pixels")
        swipe.add_argument("x2", type=int, help="end x in screen pixels")
        swipe.add_argument("y2", type=int, help="end y in screen pixels")
        swipe.add_argument(
            "--duration",
            type=float,
            default=0.25,
            help="total duration in seconds (default: %(default)s)",
        )

        microphone = add(
            "mic",
            "Play audio into the microphone",
            "Play audio into the emulator's microphone, which captures silence "
            "otherwise. It only takes audio while something captures, so start "
            "this before or while dictation or an app listens.",
        )
        source = microphone.add_mutually_exclusive_group()
        source.add_argument(
            "--file",
            metavar="WAV",
            help="a WAV file to play; any rate and channel count is converted",
        )
        source.add_argument(
            "--tone",
            type=float,
            default=440,
            metavar="HZ",
            help="a sine tone at this frequency (the default, at %(default)s Hz)",
        )
        source.add_argument("--noise", action="store_true", help="white noise")
        source.add_argument(
            "--live",
            action="store_true",
            help="relay your computer's microphone (needs the sounddevice package)",
        )
        microphone.add_argument(
            "--loop", action="store_true", help="repeat the WAV file until stopped"
        )
        microphone.add_argument(
            "--duration",
            type=float,
            metavar="SECONDS",
            help="stop after this long (default: the end of the file, or Ctrl-C)",
        )
        microphone.add_argument(
            "--volume",
            type=float,
            default=25,
            metavar="PERCENT",
            help="amplitude of the tone or noise, as a percentage of full scale "
            "(default: %(default)s)",
        )
        microphone.add_argument(
            "--host",
            default=f"127.0.0.1:{emulator.PEBBLE_TOOL_PORT}",
            help="host:port of the emulator's Pebble protocol serial port "
            "(default: %(default)s)",
        )

        feed = add(
            "feed",
            "Feed simulated phone data",
            "Write into the running emulator what the phone app would, so "
            "features that depend on the phone can be exercised without one.",
        )
        feed.add_argument(
            "--host",
            default=f"127.0.0.1:{emulator.PEBBLE_TOOL_PORT}",
            help="host:port of the emulator's Pebble protocol serial port "
            "(default: %(default)s)",
        )
        feeds = feed.add_subparsers(dest="feed", metavar="FEED", required=True)
        for f in self.feeds.values():
            subparser = feeds.add_parser(
                f.name,
                help=f.help,
                description=f.description or f.help,
                parents=[global_options()],
            )
            f.add_arguments(subparser)
        self.feed_parser = feed
        return parser

    def do_run(self, args, unknown):
        target = emulator.running(self.build_dir())
        getattr(self, "_" + args.action)(target, args)

    def _screenshot(self, target, args):
        output = args.output or target.build.join("screenshot.png")
        if not output.lower().endswith(".png"):
            self.die("the screenshot's output path must end in .png")
        if os.path.exists(output):
            os.unlink(output)

        response = target.screenshot(output)

        if not os.path.exists(output) or os.path.getsize(output) == 0:
            self.die(f"no screenshot written to {output}\n{response}")
        self.inf("wrote", output)

    def _touch(self, target, args):
        target.touch(args.x, args.y, args.x, args.y, args.duration)
        self.inf(f"tapped ({args.x}, {args.y})")

    def _swipe(self, target, args):
        target.touch(args.x1, args.y1, args.x2, args.y2, args.duration)
        self.inf(f"swiped ({args.x1}, {args.y1}) -> ({args.x2}, {args.y2})")

    def _mic(self, target, args):
        amplitude = int(32767 * args.volume / 100)
        if args.file is not None:
            source = mic.blocks(mic.read_wav(args.file), args.loop)
            what = args.file
        elif args.noise:
            source, what = mic.noise(amplitude), "white noise"
        elif args.live:
            source, what = mic.live(self.wrn), "your computer's microphone"
        else:
            source, what = mic.tone(args.tone, amplitude), f"a {args.tone:g} Hz tone"
        if args.duration is not None:
            source = itertools.islice(
                source, int(args.duration * mic.SAMPLE_RATE / mic.BLOCK_SAMPLES)
            )
        if self.dry_run:
            self.inf(f"would play {what} into the emulator's microphone")
            return

        host, _, port = args.host.rpartition(":")
        pebble = emulator.connect_watch(host or "127.0.0.1", int(port))
        self.inf(f"playing {what} into the emulator's microphone; Ctrl-C stops")
        try:
            mic.stream(pebble, source, paced=not args.live)
        except KeyboardInterrupt:
            pass

    def _feed(self, target, args):
        feed = self.feeds[args.feed]

        if self.dry_run:
            watch = Watch()
        else:
            host, _, port = args.host.rpartition(":")
            watch = Watch(emulator.connect_watch(host or "127.0.0.1", int(port)))

        try:
            feed.run(args, watch, self.inf)
        except ValueError as e:
            self.feed_parser.error(str(e))
