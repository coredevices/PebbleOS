# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import json
import os

from pbl import power
from pbl.command import PblCommand
from pbl.power import daemon

ACTIONS = ("on", "off", "cycle", "status", "measure", "start", "stop")
DEFAULT_MEASURE_S = 10.0


class Power(PblCommand):
    group = "device"

    def __init__(self):
        super().__init__(
            "power",
            "Power the watch from the lab's supply",
            "Power the watch on, off or through a cycle, or measure the "
            "current it draws, with the supply the lab wires to its battery "
            "terminals. A background process holds the supply from 'on' to "
            "'off', so other commands run while the watch stays powered.",
        )

    def do_add_parser(self, parser_adder):
        parser = self.add_subparser(parser_adder)
        parser.add_argument(
            "action",
            choices=ACTIONS,
            help="on, off, cycle; status; measure for --seconds, or start "
            "and stop a measurement around other work",
        )
        parser.add_argument(
            "--supply",
            metavar="NAME",
            help="The lab's supply to use (default: the watch's)",
        )
        parser.add_argument(
            "--voltage",
            type=int,
            metavar="MV",
            help="Voltage to supply, in millivolts (default: leave it as is)",
        )
        parser.add_argument(
            "--off-time",
            type=float,
            default=power.DEFAULT_OFF_S,
            metavar="S",
            help="How long cycle leaves the watch off (default: %(default)s s)",
        )
        parser.add_argument(
            "--seconds",
            type=float,
            default=DEFAULT_MEASURE_S,
            metavar="S",
            help="How long measure measures (default: %(default)s s)",
        )
        parser.add_argument(
            "--name",
            default="measurement",
            help="Name of the measurement, and of its saved files",
        )
        parser.add_argument(
            "--save",
            metavar="DIR",
            help="Save the measurement in DIR: a JSON summary and a CSV of 1 ms means",
        )
        parser.add_argument("--json", action="store_true", help="Print JSON")
        return parser

    def _lab_supply(self, args):
        lab = self.lab()
        if lab is None:
            self.die(
                "no lab file: pass --lab, set $PBL_LAB or write ~/.config/pbl/lab.yaml"
            )
        if args.supply:
            if args.supply not in lab.supplies:
                self.die(f"{lab.path}: no supply {args.supply!r}")
            return lab.supplies[args.supply], None

        name = getattr(args, "watch", None)
        if name is not None:
            watch = lab.watches.get(name)
            if watch is None:
                self.die(f"{lab.path}: no watch {name!r}")
        else:
            build = self.build_dir(configured=False)
            if not build.configured:
                self.die("no build to find the watch from: pass --watch or --supply")
            watch = self.lab_watch(build)
            if watch is None:
                self.die(f"no watch in the lab runs {build.board}")
        if watch.supply is None:
            self.die(f"watch {watch.name} has no supply in the lab")
        return lab.supplies[watch.supply], watch

    def do_run(self, args, unknown):
        entry, watch = self._lab_supply(args)
        if not args.json:
            what = f"supply {entry.name} ({entry.type}, {entry.port})"
            if watch is not None:
                what += f", powering watch {watch.name}"
            self.inf(f"lab: {what}")
        if self.dry_run:
            return 0

        self.wait_for_supply(entry)
        key = power.lock_key(entry.type, entry.port)
        try:
            if args.action in ("on", "cycle"):
                daemon.start(key, entry.type, entry.port, entry.name, args.voltage)
            elif not daemon.running(key):
                if args.action == "status":
                    return self._print({"supply": entry.name, "running": False}, args)
                if args.action == "off":
                    # Nothing holds the supply: switch it off directly.
                    with (
                        self.holding([(key, f"supply {entry.name}")]),
                        power.open_supply(entry.type, entry.port) as supply,
                    ):
                        supply.off()
                    return 0
                self.die(f"supply {entry.name} is not on: run 'pbl power on' first")
            return self._act(key, args)
        except (daemon.DaemonError, power.PowerError) as e:
            self.die(str(e))

    def _act(self, key, args):
        if args.voltage is not None and args.action != "off":
            daemon.request(key, "voltage", voltage_mv=args.voltage)
        if args.action == "cycle":
            reply = daemon.request(key, "cycle", off_s=args.off_time)
        elif args.action == "measure":
            reply = daemon.request(
                key,
                "measure",
                timeout=args.seconds + 30,
                seconds=args.seconds,
                name=args.name,
                save=self._save_dir(args),
            )
        elif args.action == "start":
            reply = daemon.request(key, "start", name=args.name)
        elif args.action == "stop":
            reply = daemon.request(key, "stop", save=self._save_dir(args))
        else:
            reply = daemon.request(key, args.action)
        return self._print(reply, args)

    @staticmethod
    def _save_dir(args):
        return os.path.abspath(args.save) if args.save else None

    def _print(self, reply, args):
        if args.json:
            print(json.dumps(reply.get("measurement", reply), indent=2))
        elif "text" in reply:
            print(reply["text"])
        elif reply.get("running") is False:
            print(f"{reply['supply']}: no daemon holds it")
        else:
            state = {True: "on", False: "off", None: "unknown"}[reply["powered"]]
            line = f"{reply['supply']}: {state} at {reply['voltage_mv']} mV"
            if reply["recording"]:
                line += f", recording {reply['recording']}"
            print(line + f" (daemon pid {reply['pid']})")
        return 0
