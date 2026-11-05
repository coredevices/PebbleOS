# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import json

from pbl import lab as labs
from pbl import lock
from pbl.command import PblCommand


def _port(port):
    present = labs.port_present(port)
    key = lock.serial_key(port)
    return {
        "port": port,
        "present": present,
        "locked_by": lock.holder(key) if key else None,
    }


class Lab(PblCommand):
    group = "device"

    def __init__(self):
        super().__init__(
            "lab",
            "Show the hardware wired to this host",
            "Show the lab: the watches, power supplies and Bluetooth dongles "
            "wired to this host, whether their ports are there, which session "
            "holds them, and the watch the build runs on.",
        )

    def do_add_parser(self, parser_adder):
        parser = self.add_subparser(parser_adder)
        parser.add_argument("--json", action="store_true", help="Print it as JSON")
        return parser

    def _status(self, args):
        lab = self.lab()
        build = self.build_dir(configured=False)
        board = build.board if build.configured else None
        status = {
            "path": lab.path if lab else None,
            "build": {"dir": str(build), "board": board} if board else None,
            "watch": None,
            "watch_error": None,
            "watches": [],
            "supplies": [],
            "dongles": [],
        }
        if lab is None:
            return status
        if board:
            try:
                watch = lab.watch_for(board, getattr(args, "watch", None))
                status["watch"] = watch.name if watch else None
            except labs.LabError as e:
                status["watch_error"] = str(e)
        for watch in lab.watches.values():
            status["watches"].append(
                {
                    "name": watch.name,
                    "board": watch.board,
                    "runs_build": bool(board) and watch.matches(board),
                    "serial": _port(watch.serial) if watch.serial else None,
                    "probe": watch.probe,
                    "probe_locked_by": lock.holder(lock.probe_key(watch.name)),
                    "supply": watch.supply,
                }
            )
        for supply in lab.supplies.values():
            status["supplies"].append(
                {"name": supply.name, "type": supply.type, **_port(supply.port)}
            )
        for dongle in lab.dongles.values():
            status["dongles"].append({"name": dongle.name, **_port(dongle.port)})
        return status

    @staticmethod
    def _port_text(entry):
        text = entry["port"]
        if entry["present"] is False:
            text += " (missing)"
        if entry["locked_by"] is not None:
            text += f", in use by {lock.describe(entry['locked_by'])}"
        return text

    def do_run(self, args, unknown):
        status = self._status(args)
        if args.json:
            print(json.dumps(status, indent=2))
            return 0
        if status["path"] is None:
            print(
                "No lab file: pass --lab, set $PBL_LAB or write ~/.config/pbl/lab.yaml"
            )
            return 0

        print(f"lab: {status['path']}")
        if status["build"]:
            build = status["build"]
            on = status["watch_error"] or (
                f"on watch {status['watch']}"
                if status["watch"]
                else "on no watch in the lab"
            )
            print(f"build: {build['dir']} ({build['board']}), {on}")
        if status["watches"]:
            print("watches:")
        for watch in status["watches"]:
            mark = "*" if watch["name"] == status["watch"] else " "
            print(f" {mark} {watch['name']}: {watch['board']}")
            if watch["serial"]:
                print(f"      serial: {self._port_text(watch['serial'])}")
            if watch["probe"] or watch["probe_locked_by"]:
                probe = watch["probe"] or "the board's default"
                if watch["probe_locked_by"]:
                    probe += f", in use by {lock.describe(watch['probe_locked_by'])}"
                print(f"      probe: {probe}")
            if watch["supply"]:
                print(f"      supply: {watch['supply']}")
        if status["supplies"]:
            print("supplies:")
        for supply in status["supplies"]:
            print(f"   {supply['name']}: {supply['type']}, {self._port_text(supply)}")
        if status["dongles"]:
            print("dongles:")
        for dongle in status["dongles"]:
            print(f"   {dongle['name']}: {self._port_text(dongle)}")
        return 0
