# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Plots of the activity algorithm, drawn from the step sample fixtures."""

import re
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
SAMPLES = ROOT / "tests" / "fixtures" / "activity" / "step_samples"

RATE_HZ = 25
EPOCH_S = 5
EPOCH_LEN = RATE_HZ * EPOCH_S
FFT_LEN = 128

SAMPLE_RE = re.compile(r"//> TEST_NAME (\S+)(.*?)\n\s*};", re.DOTALL)
TUPLE_RE = re.compile(r"\{\s*(-?\d+),\s*(-?\d+),\s*(-?\d+)\s*\}")

COLORS = {"x": "#3b82f6", "y": "#22c55e", "z": "#a855f7", "combined": "#fa4a36"}


def load_sample(name):
    """Raw x, y, z readings of a step sample, in mG, as an (n, 3) array."""
    for path in sorted(SAMPLES.glob("*.c")):
        for m in SAMPLE_RE.finditer(path.read_text()):
            if m.group(1) == name:
                return np.array(TUPLE_RE.findall(m.group(2)), dtype=float)
    raise KeyError(name)


def epoch(samples, index):
    return samples[index * EPOCH_LEN : (index + 1) * EPOCH_LEN]


def spectrum(epoch_samples):
    """Amplitude of each axis and combined, per frequency, as the
    algorithm computes it: mean removed, zero-padded to 128 samples."""
    centered = epoch_samples - epoch_samples.mean(axis=0)
    padded = np.zeros((FFT_LEN, 3))
    padded[: len(centered)] = centered
    amp = np.abs(np.fft.fft(padded, axis=0))[: FFT_LEN // 2]
    return amp, np.sqrt((amp**2).sum(axis=1))


def _style(ax):
    for side in ("top", "right"):
        ax.spines[side].set_visible(False)
    ax.grid(axis="y", alpha=0.3)


def plot_raw(ax, name, index):
    data = epoch(load_sample(name), index)
    t = np.arange(len(data)) / RATE_HZ
    for i, axis in enumerate("xyz"):
        ax.plot(t, data[:, i], color=COLORS[axis], label=axis)
    ax.set_xlabel("Time (s)")
    ax.set_ylabel("Acceleration (mG)")
    ax.legend(loc="lower right", bbox_to_anchor=(1, 1), ncols=3, frameon=False)
    _style(ax)


def plot_fft(ax, name, index):
    _, combined = spectrum(epoch(load_sample(name), index))
    ax.bar(np.arange(len(combined)), combined, color="#fa4a36", width=0.8)
    ax.set_xlabel("Frequency index (cycles per 5 s epoch)")
    ax.set_ylabel("Energy")
    ax.set_xlim(-1, len(combined))
    _style(ax)


def plot_spectral_density(ax, name):
    """Average spectrum over every epoch of a sample, per axis and combined,
    each relative to its own average power."""
    data = load_sample(name)
    epochs = [spectrum(epoch(data, i)) for i in range(len(data) // EPOCH_LEN)]
    amp = np.mean([a for a, _ in epochs], axis=0)
    combined = np.mean([c for _, c in epochs], axis=0)
    freq = np.arange(len(combined)) / EPOCH_S
    for i, axis in enumerate("xyz"):
        power = amp[:, i] ** 2
        ax.plot(freq[1:], power[1:] / power[1:].mean(), color=COLORS[axis], label=axis)
    power = combined**2
    ax.plot(
        freq[1:],
        power[1:] / power[1:].mean(),
        color=COLORS["combined"],
        linewidth=2,
        label="combined",
    )
    ax.set_xlabel("Frequency (Hz)")
    ax.set_ylabel("Power / average power")
    ax.legend(loc="lower right", bbox_to_anchor=(1, 1), ncols=4, frameon=False)
    _style(ax)
