# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Current measurement with a Nordic Power Profiler Kit II.

The PPK2 is used as a source meter: it replaces the battery, supplying
VBAT, so the harness can also power the watch on, off and through a cycle.
"""

import contextlib
import json
import os
import threading
import time
from dataclasses import dataclass

import numpy as np

from harness.errors import HarnessError

SAMPLE_RATE_HZ = 100_000
READ_TIMEOUT_S = 0.05
METADATA_TIMEOUT_S = 2.0
SAMPLE_BYTES = 4
# Samples looked at to find where whole samples start.
ALIGN_SAMPLES = 1000
# Fewer samples than this share of the expected ones means data was lost.
MIN_SAMPLE_RATIO = 0.95
# An idle measurement: long enough to average out transients, after the
# watch has settled from whatever the test did.
IDLE_MEASURE_S = 60
IDLE_SETTLE_S = 10
IDLE_MARGIN_S = 5
# Samples averaged into each row of the saved CSV (1 ms).
CSV_DECIMATION = 100


@dataclass
class Measurement:
    """Current samples, in microamps, at :data:`SAMPLE_RATE_HZ`."""

    name: str
    voltage_mv: int
    samples: np.ndarray = None

    @property
    def duration_s(self):
        return len(self.samples) / SAMPLE_RATE_HZ

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
        usable = len(self.samples) // CSV_DECIMATION * CSV_DECIMATION
        means = self.samples[:usable].reshape(-1, CSV_DECIMATION).mean(axis=1)
        times = np.arange(len(means)) * CSV_DECIMATION / SAMPLE_RATE_HZ
        np.savetxt(
            os.path.join(directory, f"{self.name}.csv"),
            np.column_stack((times, means)),
            delimiter=",",
            header="time_s,current_ua",
            comments="",
            fmt=("%.3f", "%.2f"),
        )


def _aligned(data):
    """``data`` from the first whole sample on. A restarted stream can begin
    with the tail of a previous one; each sample carries a 6-bit counter
    (bits 18-23) that only increments by one at the right offset."""
    best, best_score = 0, -1.0
    for offset in range(SAMPLE_BYTES):
        count = min((len(data) - offset) // SAMPLE_BYTES, ALIGN_SAMPLES)
        if count < 2:
            continue
        words = np.frombuffer(data, dtype="<u4", count=count, offset=offset)
        counter = (words >> 18) & 0x3F
        score = float(np.mean(((counter[1:] - counter[:-1]) & 0x3F) == 1))
        if score > best_score:
            best, best_score = offset, score
    return data[best:]


class Ppk2:
    """A PPK2 in source-meter mode powering the watch."""

    def __init__(self, port, voltage_mv):
        from ppk2_api.ppk2_api import PPK2_API

        ports = PPK2_API.list_devices() if port == "auto" else [port]
        for candidate in ports:
            ppk = PPK2_API(
                candidate, timeout=READ_TIMEOUT_S, write_timeout=1, exclusive=True
            )
            metadata = self._read_metadata(ppk)
            if metadata is not None:
                break
            ppk.ser.close()
        else:
            raise HarnessError(f"no PPK2 answering on {', '.join(ports) or 'any port'}")

        # ppk2-api's own reader keeps only the last chunk of the metadata,
        # losing the calibration.
        ppk._parse_metadata(metadata)
        ppk.use_source_meter()
        ppk.set_source_voltage(voltage_mv)
        self.port = candidate
        self.voltage_mv = voltage_mv
        self._ppk = ppk
        self._lock = threading.Lock()

    def set_voltage(self, voltage_mv):
        with self._lock:
            self._ppk.set_source_voltage(voltage_mv)
        self.voltage_mv = voltage_mv

    @staticmethod
    def _read_metadata(ppk):
        """The PPK2's metadata (calibration), or None if the port is not a
        PPK2's data port; each PPK2 also exposes a port that never answers."""
        from ppk2_api.ppk2_api import PPK2_Command

        ppk.stop_measuring()
        time.sleep(READ_TIMEOUT_S)
        ppk.ser.reset_input_buffer()
        ppk._write_serial((PPK2_Command.GET_META_DATA,))
        data = b""
        deadline = time.monotonic() + METADATA_TIMEOUT_S
        while b"END" not in data:
            if time.monotonic() > deadline:
                return None
            data += ppk.ser.read(max(1, ppk.ser.in_waiting))
        return data.decode(errors="replace")

    def close(self):
        with contextlib.suppress(Exception):
            self._ppk.stop_measuring()
        self._ppk.ser.close()

    def power_on(self):
        self._ppk.toggle_DUT_power("ON")

    def power_off(self):
        self._ppk.toggle_DUT_power("OFF")

    def power_cycle(self, off_s=1.0):
        self.power_off()
        time.sleep(off_s)
        self.power_on()

    def _reset_decoder(self):
        """Forget what the decoder carried over from the last stream: its
        partial sample would misalign every sample of the next one."""
        ppk = self._ppk
        ppk.remainder = {"sequence": b"", "len": 0}
        ppk.rolling_avg = None
        ppk.rolling_avg4 = None
        ppk.prev_range = None
        ppk.consecutive_range_samples = 0
        ppk.after_spike = 0

    @contextlib.contextmanager
    def measure(self, name="measurement"):
        """Sample the current for the duration of the ``with`` block.

        Decoding samples in Python cannot keep up with the stream, so the
        raw data is only collected while measuring and decoded after.
        """
        measurement = Measurement(name, self.voltage_mv)
        chunks = []
        stop = threading.Event()

        port = self._ppk.ser

        def poll():
            # Sleeping between reads overflows the host's buffer; block instead.
            while not stop.is_set():
                chunks.append(port.read(max(1, port.in_waiting)))

        with self._lock:
            port.reset_input_buffer()
            self._ppk.start_measuring()
            started = time.monotonic()
            reader = threading.Thread(target=poll, daemon=True)
            reader.start()
            try:
                yield measurement
            finally:
                stop.set()
                reader.join()
                elapsed = time.monotonic() - started
                self._ppk.stop_measuring()
                time.sleep(READ_TIMEOUT_S)
                port.reset_input_buffer()
            self._reset_decoder()
            samples, _ = self._ppk.get_samples(_aligned(b"".join(chunks)))
            measurement.samples = np.asarray(samples, dtype=np.float32)

        expected = elapsed * SAMPLE_RATE_HZ
        if len(measurement.samples) < expected * MIN_SAMPLE_RATIO:
            raise HarnessError(
                f"the PPK2 returned {len(measurement.samples)} samples in {elapsed:.2f} s, "
                f"expected about {int(expected)}: samples were dropped"
            )


class Power:
    """The ``power`` fixture: a :class:`Ppk2` powering ``dut``, saving what
    it measures in the test's results."""

    def __init__(self, ppk2, dut, results_dir):
        self.ppk2 = ppk2
        self.dut = dut
        self.results_dir = results_dir
        self.measurements = []

    def on(self):
        self.ppk2.power_on()

    def off(self):
        self.ppk2.power_off()

    def cycle(self, off_s=1.0):
        self.ppk2.power_cycle(off_s)

    @contextlib.contextmanager
    def measure(self, name="measurement"):
        with self.ppk2.measure(name) as m:
            yield m
        m.save(self.results_dir)
        self.measurements.append(m)

    def measure_for(self, seconds, name="measurement"):
        with self.measure(name) as m:
            time.sleep(seconds)
        return m

    def measure_idle(self, name, seconds=IDLE_MEASURE_S, settle_s=IDLE_SETTLE_S):
        """Measure the watch as if unplugged: its console stops listening
        (so it can sleep) and every connection is dropped, it is left
        ``settle_s`` to settle, then measured for ``seconds``."""
        with self.dut.quiesce(settle_s + seconds + IDLE_MARGIN_S):
            time.sleep(settle_s)
            return self.measure_for(seconds, name)
