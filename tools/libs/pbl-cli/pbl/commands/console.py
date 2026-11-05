# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import shlex
import sys

from pbl import lock
from pbl.command import PblCommand
from pbl.emulator import CONSOLE_PORT

DEFAULT_BAUDRATE = 115200


class Console(PblCommand):
    group = "device"

    def __init__(self):
        super().__init__(
            "console",
            "Open the firmware console",
            "Attach to the firmware's log console, over serial on a real "
            "device and over TCP under the emulator.",
        )

    def do_add_parser(self, parser_adder):
        parser = self.add_subparser(parser_adder)
        parser.add_argument(
            "--tty", help="tty to use for the console (default: the lab's)"
        )
        parser.add_argument(
            "--baudrate",
            type=int,
            help="Baudrate for the target's uart (default: the lab's, else "
            f"{DEFAULT_BAUDRATE})",
        )
        parser.add_argument(
            "--qemu-host",
            default=f"localhost:{CONSOLE_PORT}",
            help="host:port for the emulator's or the simulator's console "
            "(default: %(default)s)",
        )
        parser.add_argument(
            "--reconnect",
            action="store_true",
            help="Wrap the console in a keep-alive driver that reconnects",
        )
        return parser

    def do_run(self, args, unknown):
        build = self.build_dir()
        # The native simulator serves its console over TCP under pbl debug
        emulated = bool(build.config.CONFIG_QEMU or build.config.CONFIG_SOC_POSIX)

        watch = self.lab_watch(build, args.tty)
        baudrate = args.baudrate or (watch.serial_baud if watch else DEFAULT_BAUDRATE)
        if emulated:
            tty = f"socket://{args.qemu_host}"
        elif args.tty:
            tty = args.tty
        elif watch is not None and watch.serial:
            tty = watch.serial
            self.inf(f"lab: watch {watch.name} ({watch.board})")
        else:
            self.die("no console to attach to: pass --tty, or add the watch to a lab")

        python = shlex.quote(sys.executable)
        if build.config.CONFIG_PULSE_EVERYWHERE:
            console = f"{python} {self.script('tools', 'pulse_console.py')} -t {tty}"
        else:
            miniterm = self.script("tools", "log_hashing", "miniterm_co.py")
            console = f"{python} {miniterm} {tty}"
            if not emulated:
                # Force RTS de-asserted: on some boards RTS resets the SoC.
                console += f" {baudrate} --rts 0"

        if args.reconnect:
            keepalive = self.script("tools", "console_keepalive.py")
            console = f"{python} {keepalive} -t {tty} -- {console}"

        with self.holding([(lock.serial_key(tty), f"serial port {tty}")]):
            return self.run_shell(console)
