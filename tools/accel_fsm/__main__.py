# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Record raw accelerometer data and run LSM6DSO FSM programs on it.

python -m tools.accel_fsm record start walk --url /dev/cu.usbserial-XXXX
python -m tools.accel_fsm record pull -o recordings/ --url ...
python -m tools.accel_fsm sim flick.fsm recordings/*.bin --odr 26
"""

import argparse
import collections
import datetime
import itertools
import os
import pathlib
import sys

from . import boards, fsm, recording, ucf


def _device(args):
    from .device import Device

    url = args.url or os.environ.get("ACCEL_FSM_URL")
    if not url:
        sys.exit("pass --url (tty or socket://host:port) or set ACCEL_FSM_URL")
    return Device(url)


def cmd_record(args):
    with _device(args) as dev:
        if args.action == "start":
            line = f"accelrec start {args.label} {args.rate} {args.max_kib}"
        elif args.action == "remove":
            line = f"accelrec remove {args.name}"
        elif args.action == "pull":
            names = args.names or dev.list()
            out = pathlib.Path(args.output)
            out.mkdir(parents=True, exist_ok=True)
            for name in names:
                data = dev.pull(name)
                rec = recording.parse(data)
                stamp = datetime.datetime.fromtimestamp(
                    rec.start_time, datetime.timezone.utc
                )
                path = out / f"{stamp:%Y%m%d-%H%M%S}-{rec.label or name}.bin"
                path.write_bytes(data)
                print(
                    f"{name}: {len(rec.samples)} samples, {rec.duration_s:.1f} s -> {path}"
                )
            return
        else:
            line = f"accelrec {args.action}"
        for out_line in dev.command(line):
            print(out_line)


def cmd_info(args):
    for path in args.recordings:
        rec = recording.load(path)
        samples = rec.samples
        print(f"{path}:")
        print(f"  label {rec.label!r}, board {rec.board!r}, {rec.rate_hz:.2f} Hz")
        print(
            f"  {len(samples)} samples, {rec.duration_s:.1f} s, complete {rec.complete}"
        )
        stamp = datetime.datetime.fromtimestamp(rec.start_time, datetime.timezone.utc)
        print(f"  started {stamp:%Y-%m-%d %H:%M:%S} UTC")
        for index, missing in rec.gaps():
            print(f"  gap at sample {index}: {missing:+d} samples")
        for interval in rec.rate_changes():
            print(f"  rate changed to {1e6 / interval:.2f} Hz in some chunks")
        if samples:
            for axis, name in enumerate("xyz"):
                values = [s[axis] for s in samples]
                print(f"  {name}: min {min(values)} max {max(values)} mg")


def cmd_export(args):
    rec = recording.load(args.recording)
    with open(args.output, "w") as f:
        f.write("t_s,x_mg,y_mg,z_mg\n")
        f.writelines(
            f"{i * rec.interval_us / 1e6:.4f},{x},{y},{z}\n"
            for i, (x, y, z) in enumerate(rec.samples)
        )


def _sweep(settings):
    """Expand ["thresh2=0.4,0.5", ...] into a list of override dicts."""
    keys, values = [], []
    for item in settings or []:
        key, _, vals = item.partition("=")
        keys.append(key)
        values.append(vals.split(","))
    return [dict(zip(keys, combo)) for combo in itertools.product(*values)]


def _load_programs(paths, odr=None, settings=None):
    programs = []
    for path in paths:
        text = pathlib.Path(path).read_text()
        stem = pathlib.Path(path).stem
        if path.endswith(".ucf"):
            for p in ucf.parse(text).programs:
                p.name = f"{stem}:{p.name}"
                programs.append(p)
            continue
        for overrides in _sweep(settings):
            name = stem
            if overrides:
                name += "[" + ",".join(f"{k}={v}" for k, v in overrides.items()) + "]"
            programs.append(fsm.assemble(text, name=name, odr=odr, overrides=overrides))
    return programs


def cmd_disasm(args):
    for program in _load_programs(args.programs, args.odr):
        print(f"; {program.name}")
        print(fsm.disassemble(program))
        print()


def cmd_asm(args):
    for program in _load_programs(args.programs, args.odr, args.set):
        if program.frame == "watch":
            if not args.board:
                raise SystemExit(f"{program.name} is in watch axes: pass --board")
            program = boards.remap_program(program, boards.get(args.board))
        data = program.to_bytes()
        print(f"/* {program.name}: {len(data)} bytes */")
        for i in range(0, len(data), 12):
            print("  " + " ".join(f"0x{b:02x}," for b in data[i : i + 12]))


def watch_segments(rec, odr_hz, fs_g):
    """Contiguous runs of samples at odr_hz, in g and watch axes."""
    step = rec.rate_hz / odr_hz
    if abs(step - round(step)) > 0.05 or round(step) < 1:
        raise SystemExit(
            f"cannot run at {odr_hz} Hz from a {rec.rate_hz:.2f} Hz recording"
        )
    step = round(step)
    segments = []
    for first, samples in rec.segments():
        out = [
            tuple(max(-fs_g, min(fs_g, v / 1000.0)) for v in s) for s in samples[::step]
        ]
        segments.append((first * rec.interval_us / 1e6, out))
    return segments, rec.interval_us * step / 1e6


def cmd_sim(args):
    totals = collections.Counter()
    durations = collections.Counter()
    loaded = {}
    for path in args.recordings:
        rec = recording.load(path)
        odr = args.odr or rec.rate_hz
        if odr not in loaded:
            loaded[odr] = _load_programs(args.programs, odr, args.set)
        segments, period = watch_segments(rec, odr, args.fs)
        board = None
        if any(p.frame == "sensor" for p in loaded[odr]):
            board = boards.get(args.board or rec.board)
        duration = sum(len(samples) for _, samples in segments) * period
        durations[rec.label] += duration
        gaps = f", {len(segments) - 1} gaps" if len(segments) > 1 else ""
        print(f"{path} ({rec.label}, {duration:.1f} s at {odr:g} Hz{gaps}):")
        for program in loaded[odr]:
            events = []
            for start_s, samples in segments:
                if program.frame == "sensor":
                    samples = [board.to_sensor(s) for s in samples]
                sim = fsm.Simulator(program)
                events += [start_s + e.sample * period for e in sim.run(samples)]
            totals[(rec.label, program.name)] += len(events)
            times = " ".join(f"{t:.2f}" for t in events[: args.max_events])
            more = " ..." if len(events) > args.max_events else ""
            print(f"  {program.name}: {len(events)} events {times}{more}")
    print("summary (events per label):")
    for (label, name), count in sorted(totals.items()):
        minutes = durations[label] / 60
        print(f"  {label:16s} {name:40s} {count:5d}  ({count / minutes:.1f}/min)")


def main(argv=None):
    parser = argparse.ArgumentParser(
        prog="accel_fsm", description=__doc__.split("\n")[0]
    )
    sub = parser.add_subparsers(dest="cmd", required=True)

    url = argparse.ArgumentParser(add_help=False)
    url.add_argument("--url", help="PULSE tty or socket://host:port (or ACCEL_FSM_URL)")
    p = sub.add_parser("record", help="control the on-watch recorder")
    rsub = p.add_subparsers(dest="action", required=True)
    s = rsub.add_parser("start", parents=[url])
    s.add_argument("label", help="activity label, e.g. walk or flick")
    s.add_argument("--rate", type=int, default=52, help="sampling rate in Hz")
    s.add_argument("--max-kib", type=int, default=1024, help="file size limit")
    rsub.add_parser("stop", parents=[url])
    rsub.add_parser("status", parents=[url])
    rsub.add_parser("list", parents=[url])
    s = rsub.add_parser("pull", parents=[url])
    s.add_argument("names", nargs="*", help="recordings to pull (default: all)")
    s.add_argument("-o", "--output", default=".", help="output directory")
    s = rsub.add_parser("remove", parents=[url])
    s.add_argument("name", help="recording name, or all")
    p.set_defaults(func=cmd_record)

    p = sub.add_parser("info", help="describe recordings")
    p.add_argument("recordings", nargs="+")
    p.set_defaults(func=cmd_info)

    p = sub.add_parser("export", help="convert a recording to CSV")
    p.add_argument("recording")
    p.add_argument("-o", "--output", required=True)
    p.set_defaults(func=cmd_export)

    sweep = argparse.ArgumentParser(add_help=False)
    sweep.add_argument(
        "--set",
        action="append",
        metavar="KEY=V1[,V2...]",
        help="override thresh1..3, TI1..4 or hyst; several values sweep",
    )

    p = sub.add_parser("disasm", help="disassemble .ucf or .fsm programs")
    p.add_argument("programs", nargs="+")
    p.add_argument("--odr", type=float, help="FSM rate in Hz, for timers in ms")
    p.set_defaults(func=cmd_disasm)

    p = sub.add_parser("asm", help="assemble .fsm programs to bytes", parents=[sweep])
    p.add_argument("programs", nargs="+")
    p.add_argument("--board", help="board to remap watch-axes programs to")
    p.add_argument("--odr", type=float, help="FSM rate in Hz, for timers in ms")
    p.set_defaults(func=cmd_asm)

    p = sub.add_parser("sim", help="run programs on recordings", parents=[sweep])
    p.add_argument("programs", nargs="+", help=".fsm or .ucf files, then recordings")
    p.add_argument(
        "--board",
        help="axis mapping for sensor-axes programs (default: the recording's)",
    )
    p.add_argument("--odr", type=float, help="FSM rate in Hz (default: recording rate)")
    p.add_argument(
        "--fs", type=float, default=4.0, help="accelerometer full scale in g"
    )
    p.add_argument("--max-events", type=int, default=10)
    p.set_defaults(func=cmd_sim)

    args = parser.parse_args(argv)
    if args.cmd == "sim":
        args.recordings = [a for a in args.programs if a.endswith(".bin")]
        args.programs = [a for a in args.programs if not a.endswith(".bin")]
        if not args.recordings or not args.programs:
            parser.error("sim needs at least one program and one .bin recording")
    try:
        args.func(args)
    except (fsm.FsmError, recording.RecordingError, boards.BoardError) as e:
        sys.exit(f"error: {e}")


if __name__ == "__main__":
    main()
