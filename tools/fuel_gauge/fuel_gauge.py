#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Cell model fitting and replay for the fuel gauge (lib/fuel_gauge).

See docs/libraries/fuel_gauge/models/thevenin.md for the model and the method.
Logs are the CSV files described in fuel_gauge_log.py.
"""

import argparse
import csv
import ctypes
import dataclasses
import json
import pathlib
import re
import subprocess
import sys
import tempfile

import numpy as np
from fuel_gauge_log import LOG_COLUMNS, ChargeState, Log, read_log, write_log
from scipy import optimize

PBL_BASE = pathlib.Path(__file__).resolve().parents[2]

# Re-exported: the log format is part of this module's interface
__all__ = ["LOG_COLUMNS", "ChargeState", "Log", "read_log", "write_log"]

T_REF_C = 25.0


@dataclasses.dataclass
class Model:
    name: str
    capacity_ah: float
    ocv_soc: np.ndarray
    ocv_v: np.ndarray
    r0: float
    r1: float
    tau1: float
    r_temp_coeff: float

    def ocv(self, soc):
        return np.interp(soc, self.ocv_soc, self.ocv_v)

    def r_scale(self, temp):
        return np.exp(self.r_temp_coeff * (T_REF_C - np.clip(temp, -20.0, 60.0)))


# The parameters of the estimator the battery's devicetree node does not describe, and
# their defaults
KCONFIG_PREFIX = "FUEL_GAUGE_THEVENIN_"
KCONFIG_DEFAULTS = {"RC_MILLIOHM": 0, "RC_TAU_MS": 0, "R_TEMP_COEFF_PPM": 30000}


def write_model(path, m):
    data = {
        "name": m.name,
        "capacity_ah": m.capacity_ah,
        "ocv": [[float(s), float(v)] for s, v in zip(m.ocv_soc, m.ocv_v)],
        "r0": m.r0,
        "r1": m.r1,
        "tau1": m.tau1,
        "r_temp_coeff": m.r_temp_coeff,
    }
    pathlib.Path(path).write_text(json.dumps(data, indent=2) + "\n")


def read_model(path):
    data = json.loads(pathlib.Path(path).read_text())
    ocv = np.array(data["ocv"])
    return Model(
        data["name"],
        data["capacity_ah"],
        ocv[:, 0],
        ocv[:, 1],
        data["r0"],
        data["r1"],
        data["tau1"],
        data["r_temp_coeff"],
    )


def board_model(board, workdir):
    """The model the firmware of a board runs: its battery in the devicetree, and Kconfig."""
    sys.path.insert(0, str(PBL_BASE / "tools/devicetree"))
    import dtmap
    import fdt

    name, _, revision = board.partition("@")
    dts = dtmap.find_dts(PBL_BASE, name, revision or None)
    if dts is None:
        sys.exit(f"no devicetree for {board}: give the revision, as {name}@<revision>")
    include_dirs = [PBL_BASE / "boards", PBL_BASE / "dts", PBL_BASE / "include"]
    dtb, _ = dtmap.compile_dts(dts, "cc", include_dirs, workdir)
    found = [
        n
        for n in fdt.load(dtb).nodes()
        if "simple-battery" in n.compatibles and dtmap.enabled(n)
    ]
    if len(found) != 1:
        sys.exit(f"{board}: {len(found)} batteries in the devicetree, not 1")
    battery = found[0]
    props = (
        "charge-full-design-microamp-hours",
        "factory-internal-resistance-micro-ohms",
        "ocv-capacity-table-0",
    )
    missing = [prop for prop in props if not battery.has(prop)]
    if missing:
        sys.exit(f"{board}: {battery.path} lacks {', '.join(missing)}")
    table = battery.u32s("ocv-capacity-table-0")
    rows = sorted(zip(table[1::2], table[::2]))

    # The board's defaults, then the application configuration
    config = dict(KCONFIG_DEFAULTS)
    defconfig = PBL_BASE / "boards" / name / "Kconfig.defconfig"
    if defconfig.exists():
        for key, value in re.findall(
            rf"^config {KCONFIG_PREFIX}(\w+)\n\s+default (\d+)$",
            defconfig.read_text(),
            re.MULTILINE,
        ):
            if key in config:
                config[key] = int(value)
    conf = PBL_BASE / "pebble/boards" / f"{name}.conf"
    if conf.exists():
        for line in conf.read_text().splitlines():
            key, _, value = line.removeprefix("CONFIG_").partition("=")
            if key.startswith(KCONFIG_PREFIX) and key[len(KCONFIG_PREFIX) :] in config:
                config[key[len(KCONFIG_PREFIX) :]] = int(value)

    return Model(
        board,
        battery.u32("charge-full-design-microamp-hours") / 1e6,
        np.array([pct / 100.0 for pct, _ in rows]),
        np.array([uv / 1e6 for _, uv in rows]),
        battery.u32("factory-internal-resistance-micro-ohms") / 1e6,
        config["RC_MILLIOHM"] / 1000.0,
        config["RC_TAU_MS"] / 1000.0,
        config["R_TEMP_COEFF_PPM"] / 1e6,
    )


def load_model(spec, workdir):
    """A model file, or the model of a board, as getafix@dvt2."""
    path = pathlib.Path(spec)
    if path.suffix == ".json" or path.exists():
        return read_model(path)
    return board_model(spec, workdir)


def board_config(m):
    """The battery node properties and the Kconfig of a model, or None when the OCV points are
    not at whole percents, which the devicetree takes."""
    pct = np.round(m.ocv_soc * 100.0)
    if np.any(np.abs(pct - m.ocv_soc * 100.0) > 1e-6):
        return None
    rows = [f"<{round(v * 1e6)} {int(p)}>" for p, v in zip(pct[::-1], m.ocv_v[::-1])]
    table = ",\n\t\t\t\t       ".join(
        ", ".join(rows[i : i + 4]) for i in range(0, len(rows), 4)
    )
    return "\n".join(
        [
            "/* battery node */",
            f"charge-full-design-microamp-hours = <{round(m.capacity_ah * 1e6)}>;",
            f"factory-internal-resistance-micro-ohms = <{round(m.r0 * 1e6)}>;",
            "ocv-capacity-celsius = <25>;",
            f"ocv-capacity-table-0 = {table};",
            "",
            "# Kconfig.defconfig",
            "if FUEL_GAUGE_THEVENIN",
            "",
            f"config {KCONFIG_PREFIX}RC_MILLIOHM",
            f"    default {round(m.r1 * 1000.0)}",
            "",
            f"config {KCONFIG_PREFIX}RC_TAU_MS",
            f"    default {round(m.tau1 * 1000.0)}",
            "",
            f"config {KCONFIG_PREFIX}R_TEMP_COEFF_PPM",
            f"    default {round(m.r_temp_coeff * 1e6)}",
            "",
            "endif",
        ]
    )


def _write_c_model(path, m):
    points = [f"{{{s:.4f}f, {v:.6f}f}}," for s, v in zip(m.ocv_soc, m.ocv_v)]
    lines = [
        "#include <pbl/fuel_gauge/fuel_gauge.h>",
        "",
        "static const struct pbl_fuel_gauge_ocv_point s_ocv[] = {",
        *(f"  {p}" for p in points),
        "};",
        "",
        "const struct pbl_fuel_gauge_cell battery_cell = {",
        f"  .capacity_ah = {m.capacity_ah:.7f}f,",
        "  .ocv = s_ocv,",
        "  .ocv_count = sizeof(s_ocv) / sizeof(s_ocv[0]),",
        f"  .r0 = {m.r0:.6f}f,",
        "};",
        "",
        "struct pbl_fuel_gauge_estimator battery_estimator = {",
        f"  .r1 = {m.r1:.6f}f,",
        f"  .tau1 = {m.tau1:.3f}f,",
        f"  .r_temp_coeff = {m.r_temp_coeff:.6f}f,",
        "};",
        "",
    ]
    pathlib.Path(path).write_text("\n".join(lines))


# The estimator itself, built for the host and driven through ctypes, so the
# replay runs exactly the firmware code.


class _OcvPoint(ctypes.Structure):
    _fields_ = [("soc", ctypes.c_float), ("v", ctypes.c_float)]


class _Cell(ctypes.Structure):
    _fields_ = [
        ("capacity_ah", ctypes.c_float),
        ("ocv", ctypes.POINTER(_OcvPoint)),
        ("ocv_count", ctypes.c_uint8),
        ("r0", ctypes.c_float),
    ]


class _Estimator(ctypes.Structure):
    _fields_ = [
        ("r1", ctypes.c_float),
        ("tau1", ctypes.c_float),
        ("r_temp_coeff", ctypes.c_float),
        ("voltage_noise", ctypes.c_float),
        ("current_noise", ctypes.c_float),
    ]


class _Config(ctypes.Structure):
    _fields_ = [
        ("cell", ctypes.POINTER(_Cell)),
        ("estimator", ctypes.POINTER(_Estimator)),
        ("term_voltage", ctypes.c_float),
        ("term_current", ctypes.c_float),
    ]


class _Meas(ctypes.Structure):
    _fields_ = [("v", ctypes.c_float), ("i", ctypes.c_float), ("t", ctypes.c_float)]


class Estimator:
    """The C estimator, with a cell model."""

    def __init__(self, model, workdir):
        self.model = model
        model_c = pathlib.Path(workdir) / "model.c"
        _write_c_model(model_c, model)
        lib = pathlib.Path(workdir) / "libfuel_gauge.so"
        subprocess.run(
            [
                "cc",
                "-shared",
                "-fPIC",
                "-O2",
                f"-I{PBL_BASE / 'include'}",
                "-DCONFIG_FUEL_GAUGE_THEVENIN",
                str(PBL_BASE / "lib/fuel_gauge/fuel_gauge.c"),
                str(PBL_BASE / "lib/fuel_gauge/thevenin.c"),
                str(PBL_BASE / "lib/util/hash.c"),
                str(model_c),
                "-o",
                str(lib),
                "-lm",
            ],
            check=True,
        )
        self._lib = ctypes.CDLL(str(lib))
        self._lib.pbl_fuel_gauge_update.restype = ctypes.c_float
        self._lib.pbl_fuel_gauge_soc_get.restype = ctypes.c_float
        self._c_cell = _Cell.in_dll(self._lib, "battery_cell")
        self._c_estimator = _Estimator.in_dll(self._lib, "battery_estimator")
        self._fg = ctypes.create_string_buffer(256)

    def run(self, log, voltage_noise, current_noise, term_voltage, term_current):
        """Returns the estimated and the reported SOC [0-1] of every sample.

        The termination voltage and current only affect time to full.
        """
        if log.state is None:
            raise ValueError("the log has no charge_state column, which replay needs")
        self._c_estimator.voltage_noise = voltage_noise
        self._c_estimator.current_noise = current_noise
        self._config = _Config(
            ctypes.pointer(self._c_cell),
            ctypes.pointer(self._c_estimator),
            term_voltage,
            term_current,
        )
        meas = _Meas(log.v[0], log.i[0], log.temp[0])
        if (
            self._lib.pbl_fuel_gauge_init(
                self._fg, ctypes.byref(self._config), ctypes.byref(meas), None
            )
            < 0
        ):
            raise ValueError("invalid model or configuration")

        est = np.empty(len(log.t))
        reported = np.empty(len(log.t))
        for k, dt in enumerate(log.dt):
            meas = _Meas(log.v[k], log.i[k], log.temp[k])
            reported[k] = (
                self._lib.pbl_fuel_gauge_update(
                    self._fg, ctypes.byref(meas), ctypes.c_float(dt), int(log.state[k])
                )
                / 100.0
            )
            est[k] = self._lib.pbl_fuel_gauge_soc_get(self._fg)
        return est, reported


def coulomb_count(log, soc0, capacity_ah):
    """SOC from integrating the current, each sample covering the time since
    the previous one, as the estimator does."""
    return soc0 - np.cumsum(log.i * log.dt) / (capacity_ah * 3600.0)


def simulate(model, t, i, temp):
    """Terminal voltage and SOC of a cell starting full and at rest."""
    dt = np.diff(t, prepend=t[0])
    soc = coulomb_count(Log(t, None, i, temp, None), 1.0, model.capacity_ah)
    scale = model.r_scale(temp)
    v1 = _rc_filter(i * scale, dt, model.tau1) * model.r1
    return model.ocv(soc) - v1 - model.r0 * scale * i, soc


def _rc_filter(x, dt, tau):
    """First-order lag with x held over each step, as in the estimator."""
    a = np.exp(-dt / tau)
    y = np.empty_like(x)
    acc = 0.0
    for k in range(len(x)):
        acc = a[k] * acc + (1.0 - a[k]) * x[k]
        y[k] = acc
    return y


def _hat_weights(soc, grid):
    """Linear interpolation weights: OCV(soc) = W @ ocv_at_grid."""
    soc = np.clip(soc, grid[0], grid[-1])
    j = np.clip(np.searchsorted(grid, soc, side="right") - 1, 0, len(grid) - 2)
    f = (soc - grid[j]) / (grid[j + 1] - grid[j])
    w = np.zeros((len(soc), len(grid)))
    rows = np.arange(len(soc))
    w[rows, j] = 1.0 - f
    w[rows, j + 1] = f
    return w


class Fit:
    """Least-squares fit of the cell model to logs with a known SOC path.

    For a given tau1 and temperature coefficient the terminal voltage is
    linear in the unknowns:

        v = W(soc) @ ocv - R0 * s(T) * i - R1 * lag_tau1(s(T) * i)

    so each candidate is solved with bounded linear least squares (BVLS,
    scipy.optimize.lsq_linear), and tau1 and the temperature coefficient are
    searched in an outer loop (variable projection).

    The OCV points are written as a base voltage plus steps of at least
    MIN_STEP_V, ocv = T @ u with T lower triangular, so the curve comes out
    strictly increasing. R0 and R1 are bounded at 0.

    A second-difference penalty on the OCV fills the SOC ranges the logs do
    not visit. Its rows are appended to the system as w * D2 @ T @ u = 0, with
    w = smoothing * sqrt(N / n) for N samples and n grid points, so the
    penalty weight on the squared norm is smoothing**2 * N / n: it grows with
    the number of samples like the data term, and the same smoothing works
    for any log length.
    """

    MIN_STEP_V = 1e-4

    def __init__(self, logs, socs, grid, smoothing):
        self.logs = logs
        self.grid = grid
        self.w = np.vstack([_hat_weights(s, grid) for s in socs])
        self.v = np.concatenate([log.v for log in logs])
        n = len(grid)
        self.steps = np.tril(np.ones((n, n)))
        d2 = np.diff(np.eye(n), 2, axis=0)
        weight = smoothing * np.sqrt(len(self.v) / n)
        self.reg = weight * d2 @ self.steps
        self.coverage = self.w.sum(axis=0)

    def solve(self, tau1, r_temp_coeff):
        n = len(self.grid)
        cols_r0, cols_r1 = [], []
        for log in self.logs:
            si = log.i * np.exp(
                r_temp_coeff * (T_REF_C - np.clip(log.temp, -20.0, 60.0))
            )
            cols_r0.append(-si)
            cols_r1.append(-_rc_filter(si, log.dt, tau1))
        a = np.column_stack(
            [self.w @ self.steps, np.concatenate(cols_r0), np.concatenate(cols_r1)]
        )
        a = np.vstack([a, np.column_stack([self.reg, np.zeros((len(self.reg), 2))])])
        b = np.concatenate([self.v, np.zeros(len(self.reg))])
        lb = np.full(n + 2, self.MIN_STEP_V)
        lb[0] = -np.inf
        lb[n:] = 0.0
        res = optimize.lsq_linear(a, b, bounds=(lb, np.inf), method="bvls")
        x = res.x
        resid = self.v - a[: len(self.v)] @ x
        return self.steps @ x[:n], x[n], x[n + 1], resid

    def best_tau(self, r_temp_coeff):
        def cost(log_tau):
            return np.mean(self.solve(np.exp(log_tau), r_temp_coeff)[3] ** 2)

        res = optimize.minimize_scalar(
            cost, bounds=(np.log(5.0), np.log(5000.0)), method="bounded"
        )
        return np.exp(res.x), res.fun


def fit_model(
    logs,
    name,
    grid,
    start_socs,
    capacity_ah=None,
    end_soc=0.0,
    r_temp_coeff=None,
    smoothing=0.01,
):
    """Fits a model to logs whose SOC path is known from coulomb counting.

    Without an explicit capacity, the first log must go from its start SOC to
    end_soc, and its net discharged charge defines the capacity.
    """
    if capacity_ah is None:
        log = logs[0]
        charge_ah = np.sum(log.i * log.dt) / 3600.0
        capacity_ah = charge_ah / (start_socs[0] - end_soc)

    socs = [coulomb_count(log, s0, capacity_ah) for log, s0 in zip(logs, start_socs)]
    fit = Fit(logs, socs, grid, smoothing)

    temps = np.concatenate([log.temp for log in logs])
    if r_temp_coeff is not None:
        coeff = r_temp_coeff
    elif np.ptp(temps) >= 10.0:
        res = optimize.minimize_scalar(
            lambda k: fit.best_tau(k)[1], bounds=(0.0, 0.08), method="bounded"
        )
        coeff = res.x
    else:
        coeff = 0.03
        print(
            "logs span less than 10 C: using the default resistance temperature coefficient",
            file=sys.stderr,
        )

    tau1, _ = fit.best_tau(coeff)
    ocv, r0, r1, resid = fit.solve(tau1, coeff)
    model = Model(name, capacity_ah, grid, ocv, r0, r1, tau1, coeff)
    return model, resid, fit.coverage


# Commands


def cmd_extract(args):
    """Battery state log lines from a firmware log to a CSV log."""
    line = re.compile(
        r"Battery state: v_mv: (-?\d+), i_ua: (-?\d+), t_mc: (-?\d+), td_ms: (\d+), fg: (\d+)"
    )
    t = 0.0
    rows = []
    for text in args.input.read_text(errors="replace").splitlines():
        m = line.search(text)
        if m is None:
            continue
        v_mv, i_ua, t_mc, td_ms, state = (int(g) for g in m.groups())
        t += td_ms / 1000.0
        rows.append((t, v_mv / 1000.0, i_ua / 1e6, t_mc / 1000.0, state))
    if not rows:
        sys.exit("no battery state lines found (enable verbose battery service logs)")
    write_log(args.output, Log(*(np.array(c) for c in zip(*rows))))
    print(f"{len(rows)} samples, {rows[-1][0] / 3600.0:.1f} h")


def cmd_fit(args):
    logs = [read_log(p) for p in args.logs]
    start_socs = args.start_soc or [1.0] * len(logs)
    if len(start_socs) != len(logs):
        sys.exit("give one --start-soc per log")
    grid = np.array(args.grid) if args.grid else np.linspace(0.0, 1.0, args.grid_points)

    model, resid, coverage = fit_model(
        logs,
        args.name,
        grid,
        start_socs,
        capacity_ah=args.capacity_ah,
        end_soc=args.end_soc,
        r_temp_coeff=args.r_temp_coeff,
        smoothing=args.smoothing,
    )

    write_model(args.output, model)
    print(f"capacity    {model.capacity_ah * 1000.0:.1f} mAh")
    print(f"r0          {model.r0 * 1000.0:.0f} mohm")
    print(f"r1          {model.r1 * 1000.0:.0f} mohm")
    print(f"tau1        {model.tau1:.0f} s")
    print(f"temp coeff  {model.r_temp_coeff:.4f} 1/C")
    print(
        f"residual    {np.sqrt(np.mean(resid**2)) * 1000.0:.1f} mV rms, {np.max(np.abs(resid)) * 1000.0:.1f} mV max"
    )
    config = board_config(model)
    if config is None:
        print(
            "warning: OCV points not at whole percents, no devicetree", file=sys.stderr
        )
    else:
        print()
        print(config)
    for s, c in zip(grid, coverage):
        if c < 1.0:
            print(
                f"warning: no data near SOC {s:.2f}, OCV extrapolated", file=sys.stderr
            )


def _write_replay(path, log, est, reported, ref):
    with open(path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["time_s", "soc", "soc_reported", "soc_reference"])
        for row in zip(
            log.t, est, reported, ref if ref is not None else [""] * len(est)
        ):
            w.writerow([f"{x:.4f}" if isinstance(x, float) else x for x in row])


def cmd_replay(args):
    log = read_log(args.log)
    if log.state is None:
        sys.exit(f"{args.log}: no charge_state column, which replay needs")
    with tempfile.TemporaryDirectory() as tmp:
        model = load_model(args.model, tmp)
        est_lib = Estimator(model, tmp)
        # Only time to full uses these, and replay doesn't report it: by
        # default, the top of the OCV curve and C/10.
        term_voltage = args.term_voltage or float(model.ocv_v[-1])
        term_current = args.term_current or 0.1 * model.capacity_ah
        est, reported = est_lib.run(
            log,
            args.voltage_noise,
            args.current_noise,
            term_voltage,
            term_current,
        )

    ref = None
    if args.start_soc is not None:
        ref = coulomb_count(log, args.start_soc, model.capacity_ah)
        err = est - ref
        print(
            f"SOC error   {np.sqrt(np.mean(err**2)) * 100.0:.2f} % rms, {np.max(np.abs(err)) * 100.0:.2f} % max"
        )
        err = reported - ref
        print(
            f"reported    {np.sqrt(np.mean(err**2)) * 100.0:.2f} % rms, {np.max(np.abs(err)) * 100.0:.2f} % max"
        )
    print(
        f"final SOC   {est[-1] * 100.0:.1f} % (reported {reported[-1] * 100.0:.1f} %)"
    )

    if args.output:
        _write_replay(args.output, log, est, reported, ref)

    if args.plot:
        import matplotlib.pyplot as plt

        hours = log.t / 3600.0
        fig, (ax_v, ax_i, ax_soc) = plt.subplots(3, 1, sharex=True, figsize=(10, 8))
        ax_v.plot(hours, log.v)
        ax_v.set_ylabel("V")
        ax_i.plot(hours, log.i * 1000.0)
        ax_i.set_ylabel("mA")
        ax_soc.plot(hours, est * 100.0, label="estimate")
        ax_soc.plot(hours, reported * 100.0, label="reported")
        if ref is not None:
            ax_soc.plot(hours, ref * 100.0, "--", label="coulomb count")
        ax_soc.set_ylabel("SOC %")
        ax_soc.set_xlabel("h")
        ax_soc.legend()
        fig.tight_layout()
        if args.plot == "-":
            plt.show()
        else:
            fig.savefig(args.plot)


def pulse_profile(capacity_ah, rate_c, pulse_s, rest_s, sample_s, tail=None):
    """Current of a pulsed discharge: a rest, then pulses at rate_c separated
    by rests, down to empty and a final rest.

    tail(soc, i) is asked before each pulse at rate_c, with the SOC and the
    pulse current. Once it returns True, the remaining pulses are at a
    quarter of the current and four times as long, as on the bench.
    """
    capacity_as = capacity_ah * 3600.0
    i_pulse, length = rate_c * capacity_ah, pulse_s
    in_tail = False
    segments = [(rest_s, 0.0)]
    left = capacity_as
    while left > 0.0:
        if not in_tail and tail is not None and tail(left / capacity_as, i_pulse):
            in_tail = True
            i_pulse, length = i_pulse / 4.0, length * 4.0
        segments += [(min(length, left / i_pulse), i_pulse), (rest_s, 0.0)]
        left -= i_pulse * length

    ends = np.cumsum([d for d, _ in segments])
    t = np.arange(0.0, ends[-1] + sample_s, sample_s)
    # Each sample holds the current of the interval that ends at it
    seg = np.minimum(np.searchsorted(ends, t, side="left"), len(segments) - 1)
    i = np.array([segments[k][1] for k in seg])
    i[0] = 0.0
    delivered = np.cumsum(i * np.diff(t, prepend=0.0))
    i[delivered > capacity_as] = 0.0
    return t, i


def cmd_synth(args):
    """A pulsed discharge log of a known model, for testing the fit."""
    with tempfile.TemporaryDirectory() as tmp:
        model = load_model(args.model, tmp)

    def tail(soc, i):
        # Loaded voltage at the end of the next pulse, from a rested cell
        soc_end = soc - i * args.pulse / (model.capacity_ah * 3600.0)
        r = model.r0 + model.r1 * (1.0 - np.exp(-args.pulse / model.tau1))
        v = model.ocv(soc_end) - r * model.r_scale(args.temp) * i
        return v < args.cutoff + args.tail_margin

    t, i = pulse_profile(
        model.capacity_ah, args.rate, args.pulse, args.rest, args.sample, tail
    )
    temp = np.full_like(t, args.temp)
    v, _ = simulate(model, t, i, temp)
    rng = np.random.default_rng(args.seed)
    v = v + rng.normal(0.0, args.noise, len(v))
    if args.lsb > 0.0:
        v = np.round(v / args.lsb) * args.lsb
    state = np.full(len(t), ChargeState.DISCHARGING)
    write_log(args.output, Log(t, v, i, temp, state))
    print(f"{len(t)} samples, {t[-1] / 3600.0:.1f} h")


def main(argv=None):
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    sub = parser.add_subparsers(dest="command", required=True)

    p = sub.add_parser("extract", help="convert firmware battery logs to a CSV log")
    p.add_argument("input", type=pathlib.Path)
    p.add_argument("output", type=pathlib.Path)
    p.set_defaults(func=cmd_extract)

    p = sub.add_parser("fit", help="fit a cell model to CSV logs")
    p.add_argument("logs", nargs="+", type=pathlib.Path)
    p.add_argument(
        "-o", "--output", type=pathlib.Path, required=True, help="model file (JSON)"
    )
    p.add_argument("--name", required=True)
    p.add_argument(
        "--start-soc",
        type=float,
        action="append",
        help="SOC at the start of each log (default 1)",
    )
    p.add_argument(
        "--end-soc",
        type=float,
        default=0.0,
        help="SOC at the end of the first log (default 0)",
    )
    p.add_argument(
        "--capacity-ah",
        type=float,
        help="capacity, instead of deriving it from the first log",
    )
    p.add_argument(
        "--r-temp-coeff",
        type=float,
        help="resistance temperature coefficient, instead of fitting it",
    )
    p.add_argument(
        "--grid-points",
        type=int,
        default=21,
        help="evenly spaced OCV points (default 21)",
    )
    p.add_argument(
        "--grid", type=float, nargs="+", help="explicit OCV SOC points, from 0 to 1"
    )
    p.add_argument(
        "--smoothing",
        type=float,
        default=0.01,
        help="OCV curvature penalty, scaled by the number of samples per grid "
        "point so that it doesn't depend on the log length (default 0.01)",
    )
    p.set_defaults(func=cmd_fit)

    p = sub.add_parser("replay", help="run the estimator over a CSV log")
    p.add_argument("model", help="model file (JSON), or a board, as getafix@dvt2")
    p.add_argument("log", type=pathlib.Path)
    p.add_argument(
        "--start-soc",
        type=float,
        help="known SOC at the start, for a coulomb-count reference",
    )
    p.add_argument("--voltage-noise", type=float, default=0.015)
    p.add_argument("--current-noise", type=float, default=0.005)
    p.add_argument(
        "--term-voltage",
        type=float,
        help="charge termination voltage [V], for time to full "
        "(default: the top of the OCV curve)",
    )
    p.add_argument(
        "--term-current",
        type=float,
        help="charge termination current [A], for time to full (default: C/10)",
    )
    p.add_argument(
        "-o", "--output", type=pathlib.Path, help="CSV with the SOC of every sample"
    )
    p.add_argument("--plot", help="plot to this image file, or - to show it")
    p.set_defaults(func=cmd_replay)

    p = sub.add_parser("synth", help="simulate the bench discharge of a model")
    p.add_argument("model", help="model file (JSON), or a board, as getafix@dvt2")
    p.add_argument("output", type=pathlib.Path)
    p.add_argument(
        "--rate", type=float, default=0.5, help="pulse current [C] (default 0.5)"
    )
    p.add_argument(
        "--pulse",
        type=float,
        default=144.0,
        help="pulse length [s] (default 144: 2%% at 0.5C)",
    )
    p.add_argument(
        "--rest",
        type=float,
        default=900.0,
        help="rest after each pulse [s] (default 900)",
    )
    p.add_argument(
        "--sample", type=float, default=1.0, help="sample period [s] (default 1)"
    )
    p.add_argument(
        "--cutoff", type=float, default=3.0, help="discharge end [V] (default 3.0)"
    )
    p.add_argument(
        "--tail-margin",
        type=float,
        default=0.2,
        help="switch to quarter-rate pulses this far above the cutoff [V] "
        "(default 0.2)",
    )
    p.add_argument("--temp", type=float, default=25.0)
    p.add_argument(
        "--noise", type=float, default=0.002, help="voltage noise [V] (default 0.002)"
    )
    p.add_argument(
        "--lsb", type=float, default=0.0, help="voltage quantization [V] (default none)"
    )
    p.add_argument("--seed", type=int, default=0)
    p.set_defaults(func=cmd_synth)

    args = parser.parse_args(argv)
    args.func(args)


if __name__ == "__main__":
    main()
