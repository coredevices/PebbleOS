# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""CSV logs shared by the fuel gauge tools.

A log has one row per sample with the columns of LOG_COLUMNS: time [s],
terminal voltage [V], current [A, positive when discharging], temperature [C]
and, optionally, the charge state given to the estimator (ChargeState). Every
instrument writes this format, and the fit and the replay read it.
"""

import csv
import dataclasses
import enum

import numpy as np

LOG_COLUMNS = ("time_s", "voltage_v", "current_a", "temperature_c", "charge_state")
REQUIRED_COLUMNS = LOG_COLUMNS[:4]


class ChargeState(enum.IntEnum):
    """enum pbl_fuel_gauge_charge_state"""

    DISCHARGING = 0
    CHARGING_CC = 1
    CHARGING_CV = 2
    CHARGE_COMPLETE = 3


@dataclasses.dataclass
class Log:
    t: np.ndarray
    v: np.ndarray
    i: np.ndarray
    temp: np.ndarray
    # None when the log has no charge_state column
    state: np.ndarray | None

    @property
    def dt(self):
        return np.diff(self.t, prepend=self.t[0])


def read_log(path):
    with open(path, newline="") as f:
        reader = csv.DictReader(f)
        missing = [c for c in REQUIRED_COLUMNS if c not in (reader.fieldnames or ())]
        if missing:
            raise ValueError(f"{path}: missing columns {', '.join(missing)}")
        rows = list(reader)
    col = {c: np.array([float(r[c]) for r in rows]) for c in REQUIRED_COLUMNS}
    state = None
    if rows and rows[0].get("charge_state") not in (None, ""):
        state = np.array([int(float(r["charge_state"])) for r in rows])
    return Log(
        col["time_s"], col["voltage_v"], col["current_a"], col["temperature_c"], state
    )


class LogWriter:
    """Writes a log row by row, flushing each one."""

    def __init__(self, path):
        # Held open until close(), so a long run keeps what it logged.
        self._f = open(path, "w", newline="")  # noqa: SIM115
        self._w = csv.writer(self._f)
        self._w.writerow(LOG_COLUMNS)
        self.rows = 0

    def write(self, t, v, i, temp, state):
        self._w.writerow(
            [
                f"{t:.3f}",
                f"{v:.4f}",
                f"{i:.6f}",
                f"{temp:.2f}",
                "" if state is None else int(state),
            ]
        )
        self._f.flush()
        self.rows += 1

    def close(self):
        self._f.close()


def write_log(path, log):
    w = LogWriter(path)
    state = log.state if log.state is not None else [None] * len(log.t)
    for row in zip(log.t, log.v, log.i, log.temp, state):
        w.write(*row)
    w.close()
