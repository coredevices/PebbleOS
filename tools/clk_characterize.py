# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Derive the noise model of a calibrated clock from `clk cal` shell output.

noise   logs of `clk cal sample` at several gate lengths -> white FM (wfm)
drift   a long `clk cal sample` run of short gates -> wander and random walk (rw)
steady  steady-state gain, time constant and deviation of the tracker
bias    CSV of host_ms,uptime_ms taken with the rate held -> true mean rate
"""

import argparse
import csv
import itertools
import math
import re
import statistics

GATE_RE = re.compile(r"gate (\d+) cycles")
SAMPLE_RE = re.compile(r"^\s*(\d+) (\d+)\s*$")


def parse(paths):
    """Return {cycles: [(t_s, hz), ...]} from sample logs."""
    runs = {}
    cycles = None
    for path in paths:
        with open(path, errors="replace") as f:
            for line in f:
                m = GATE_RE.search(line)
                if m:
                    cycles = int(m.group(1))
                    runs.setdefault(cycles, [])
                    continue
                m = SAMPLE_RE.match(line)
                if m and cycles is not None:
                    runs[cycles].append(
                        (int(m.group(1)) / 1000.0, int(m.group(2)) / 1000.0)
                    )
    return runs


def two_sample_dev(hz):
    mid = statistics.median(hz)
    y = [h / mid - 1.0 for h in hz]
    return math.sqrt(
        0.5 * statistics.fmean((b - a) ** 2 for a, b in itertools.pairwise(y))
    )


def cmd_noise(args):
    runs = parse(args.logs)
    points = []
    print("cycles  gates  gate (s)  deviation (ppm)  wfm (ppm*sqrt(s))")
    for cycles in sorted(runs):
        hz = [h for _, h in runs[cycles]]
        if len(hz) < 3:
            continue
        gate_s = cycles / statistics.median(hz)
        dev = two_sample_dev(hz)
        points.append((gate_s, dev))
        print(
            f"{cycles:6d}  {len(hz):5d}  {gate_s:8.3f}  {dev * 1e6:15.2f}  {dev * math.sqrt(gate_s) * 1e6:17.2f}"
        )
    if len(points) < 2:
        return
    # Variance against 1/T: white FM is the slope, slower fluctuations the intercept.
    fit = statistics.linear_regression(
        [1.0 / t for t, _ in points], [d * d for _, d in points]
    )
    print(f"wfm = {math.sqrt(max(fit.slope, 0.0)):.2e}")
    print(
        f"floor = {math.sqrt(max(fit.intercept, 0.0)):.2e} (fluctuations a longer gate does not average)"
    )


def cmd_drift(args):
    runs = parse(args.logs)
    cycles = max(runs, key=lambda c: len(runs[c]))
    t = [s for s, _ in runs[cycles]]
    hz = [h for _, h in runs[cycles]]
    mid = statistics.median(hz)
    y = [h / mid - 1.0 for h in hz]
    gate_s = cycles / mid
    tau0 = statistics.median(b - a for a, b in itertools.pairwise(t))
    half = max(1, round(args.period / tau0 / 2))

    # Spread of single gates around the mean over one period, less their white noise.
    res = []
    for i in range(half, len(y) - half):
        res.append(y[i] - statistics.fmean(y[i - half : i + half + 1]))
    var = statistics.fmean(r * r for r in res) - args.wfm**2 / gate_s
    wander = math.sqrt(max(var, 0.0))

    # Allan variance of averages over tau: white FM falls as 1/tau, a random walk grows as tau/3.
    rws = []
    print("tau (s)  deviation (ppm)")
    m = half * 2
    while m * tau0 * 4 <= t[-1] - t[0]:
        avg = [statistics.fmean(y[i : i + m]) for i in range(0, len(y) - m + 1, m)]
        avar = 0.5 * statistics.fmean((b - a) ** 2 for a, b in itertools.pairwise(avg))
        tau = m * tau0
        print(f"{tau:7.0f}  {math.sqrt(avar) * 1e6:15.2f}")
        excess = avar - args.wfm**2 / tau
        if excess > 0:
            rws.append(math.sqrt(3.0 * excess / tau))
        m *= 2
    print(f"{len(y)} gates of {cycles} cycles every {tau0:.1f} s")
    print(f"wander = {wander:.2e}")
    if rws:
        print(f"rw = {statistics.median(rws):.2e}")


def cmd_steady(args):
    q = args.rw**2 * args.period
    r = args.wfm**2 / args.gate + args.wander**2
    prior = 0.5 * (q + math.sqrt(q * q + 4.0 * q * r))
    k = prior / (prior + r)
    sigma = math.sqrt((1.0 - k) * prior)
    tau = -args.period / math.log(1.0 - k)
    print(f"K = {k:.3f}, tau = {tau / 60:.1f} min, sigma = {sigma * 1e6:.1f} ppm")
    for days in (1, 7):
        drift = math.sqrt(2.0 * sigma**2 * tau * days * 86400)
        print(f"time error from estimate noise over {days} d: {drift:.2f} s")


def cmd_bias(args):
    with open(args.csv) as f:
        rows = [(float(r["host_ms"]), float(r["uptime_ms"])) for r in csv.DictReader(f)]
    xs = [(h - rows[0][0]) / 1000.0 for h, _ in rows]
    ys = [((u - rows[0][1]) - (h - rows[0][0])) / 1000.0 for h, u in rows]
    fit = statistics.linear_regression(xs, ys)
    print(
        f"clock - host: {fit.slope * 1e6:+.2f} ppm ({fit.slope * 86400:+.2f} s/day) over {xs[-1] / 60:.0f} min"
    )
    print("the oscillator runs this much faster than the held rate on average")


def main():
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    sub = parser.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("noise", help="white frequency noise from several gate lengths")
    p.add_argument("logs", nargs="+")
    p.set_defaults(fn=cmd_noise)

    p = sub.add_parser("drift", help="wander and random walk from one long run")
    p.add_argument("logs", nargs="+")
    p.add_argument("--wfm", type=float, required=True, help="white FM, from noise")
    p.add_argument("--period", type=float, default=60.0, help="calibration period (s)")
    p.set_defaults(fn=cmd_drift)

    p = sub.add_parser("steady", help="tracker steady state")
    p.add_argument("--wfm", type=float, required=True)
    p.add_argument("--wander", type=float, required=True)
    p.add_argument("--rw", type=float, required=True)
    p.add_argument("--gate", type=float, required=True, help="gate length (s)")
    p.add_argument("--period", type=float, default=60.0, help="calibration period (s)")
    p.set_defaults(fn=cmd_steady)

    p = sub.add_parser("bias", help="mean rate from clock time against host time")
    p.add_argument("csv", help="columns host_ms,uptime_ms")
    p.set_defaults(fn=cmd_bias)

    args = parser.parse_args()
    args.fn(args)


if __name__ == "__main__":
    main()
