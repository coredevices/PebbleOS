# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import json
import os
from dataclasses import dataclass

import numpy as np

# Rows of the saved CSV: 1 ms means.
CSV_PERIOD_S = 0.001


@dataclass
class Measurement:
    """Current samples, in microamps, at ``sample_rate_hz``."""

    name: str
    voltage_mv: int
    sample_rate_hz: int
    samples: np.ndarray = None

    @property
    def duration_s(self):
        return len(self.samples) / self.sample_rate_hz

    @property
    def mean_ua(self):
        return float(self.samples.mean())

    @property
    def max_ua(self):
        return float(self.samples.max())

    @property
    def min_ua(self):
        return float(self.samples.min())

    def percentile_ua(self, q):
        return float(np.percentile(self.samples, q))

    @property
    def charge_uah(self):
        return self.mean_ua * self.duration_s / 3600

    @property
    def energy_uwh(self):
        return self.charge_uah * self.voltage_mv / 1000

    def summary(self):
        return {
            "name": self.name,
            "voltage_mv": self.voltage_mv,
            "duration_s": round(self.duration_s, 3),
            "samples": len(self.samples),
            "mean_ua": round(self.mean_ua, 2),
            "min_ua": round(self.min_ua, 2),
            "max_ua": round(self.max_ua, 2),
            "p99_ua": round(self.percentile_ua(99), 2),
            "charge_uah": round(self.charge_uah, 4),
            "energy_uwh": round(self.energy_uwh, 4),
        }

    def __str__(self):
        s = self.summary()
        return (
            f"{self.name}: mean {s['mean_ua']} uA, max {s['max_ua']} uA, "
            f"p99 {s['p99_ua']} uA over {s['duration_s']} s"
        )

    def save(self, directory):
        """Write ``<name>.json`` (the summary) and ``<name>.csv`` (1 ms means)."""
        os.makedirs(directory, exist_ok=True)
        with open(os.path.join(directory, f"{self.name}.json"), "w") as f:
            json.dump(self.summary(), f, indent=2)
        decimation = max(1, round(self.sample_rate_hz * CSV_PERIOD_S))
        usable = len(self.samples) // decimation * decimation
        means = self.samples[:usable].reshape(-1, decimation).mean(axis=1)
        times = np.arange(len(means)) * decimation / self.sample_rate_hz
        np.savetxt(
            os.path.join(directory, f"{self.name}.csv"),
            np.column_stack((times, means)),
            delimiter=",",
            header="time_s,current_ua",
            comments="",
            fmt=("%.3f", "%.2f"),
        )
