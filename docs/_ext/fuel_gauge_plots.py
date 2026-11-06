# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Figures of the fuel gauge pages, drawn from the getafix cell model."""

import math
import pathlib
import re

import numpy as np
import schemdraw
import schemdraw.elements as elm

MODEL_C = (
    pathlib.Path(__file__).resolve().parents[2]
    / "fw/services/battery/fuel_gauge/models/getafix.c"
)

# Reads in both light and dark themes, like the plot_rcparams in conf.py
COLOR = "#8a8a8a"


def _model():
    text = MODEL_C.read_text()
    ocv = np.array(
        [(float(s), float(v)) for s, v in re.findall(r"\{([\d.]+)f, ([\d.]+)f\}", text)]
    )

    def field(name):
        return float(re.search(rf"\.{name} = ([\d.]+)f", text).group(1))

    return {
        "soc": ocv[:, 0],
        "ocv": ocv[:, 1],
        "capacity_ah": field("capacity_ah"),
        "r0": field("r0"),
        "r1": field("r1"),
        "tau1": field("tau1"),
    }


def draw_circuit(ax):
    """The equivalent circuit of the cell."""
    d = schemdraw.Drawing(canvas=ax, show=False)
    d.config(color=COLOR, fontsize=12)
    src = d.add(elm.SourceV().up().label(r"$\mathrm{OCV}(z)$"))
    d.add(elm.Resistor().right().label("$R_0$"))
    d.push()
    d.add(elm.Line().up(0.8))
    d.add(elm.Resistor().right().label("$R_1$"))
    d.add(elm.Line().down(0.8))
    d.pop()
    d.add(elm.Line().down(0.8))
    d.add(elm.Capacitor().right().label("$C_1$", loc="bottom"))
    d.add(elm.Line().up(0.8))
    d.add(elm.Line().right(1.5))
    d.add(elm.Dot(open=True).label("$v$", loc="right"))
    d.add(elm.Ground().at(src.start))
    d.draw(show=False)
    ax.set_aspect("equal")
    ax.axis("off")


def plot_ocv(ax):
    """The OCV curve of the getafix model."""
    m = _model()
    ax.plot(m["soc"] * 100.0, m["ocv"], marker=".", color="C0")
    ax.set_xlabel("SOC (%)")
    ax.set_ylabel("OCV (V)")
    ax.grid(alpha=0.3)


def plot_pulse(ax):
    """Terminal voltage of the getafix model through one bench pulse."""
    m = _model()
    i_pulse = 0.5 * m["capacity_ah"]
    t_pulse, t_rest = 144.0, 600.0
    ocv0 = float(np.interp(0.5, m["soc"], m["ocv"]))
    slope = float(np.gradient(m["ocv"], m["soc"])[np.searchsorted(m["soc"], 0.5)])

    t = np.arange(-60.0, t_pulse + t_rest, 1.0)
    on = (t > 0.0) & (t <= t_pulse)
    i = np.where(on, i_pulse, 0.0)
    a = math.exp(-1.0 / m["tau1"])
    v1 = np.zeros_like(t)
    for k in range(1, len(t)):
        v1[k] = a * v1[k - 1] + m["r1"] * (1.0 - a) * i[k]
    z = -np.cumsum(i) / (m["capacity_ah"] * 3600.0)
    v = ocv0 + slope * z - v1 - m["r0"] * i

    ax.plot(t, v, color="C0")
    ax.set_xlabel("time (s)")
    ax.set_ylabel("terminal voltage (V)")
    ax.grid(alpha=0.3)
    drop = m["r0"] * i_pulse
    ax.annotate(
        r"$R_0\,\Delta i$",
        xy=(t_pulse + 1, v[on][-1] + drop / 2),
        xytext=(t_pulse + 60, v[on][-1] + drop / 2),
        arrowprops={"arrowstyle": "-", "color": COLOR},
        va="center",
    )
    ax.annotate(
        r"relaxation, $\tau_1$",
        xy=(t_pulse + m["tau1"], v[np.searchsorted(t, t_pulse + m["tau1"])]),
        xytext=(t_pulse + 220, v[0] - 0.02),
        arrowprops={"arrowstyle": "->", "color": COLOR},
    )
    ax.annotate(
        "OCV at the new SOC",
        xy=(t[-1], v[-1]),
        xytext=(t_pulse + 300, v[-1] - 0.12),
        arrowprops={"arrowstyle": "->", "color": COLOR},
    )
