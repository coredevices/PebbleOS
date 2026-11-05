# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""A Nordic Power Profiler Kit II in source-meter mode. It cuts the power
when its port closes."""

import contextlib
import re
import threading
import time

import numpy as np

from pbl.power import PowerError, Supply
from pbl.power.measurement import Measurement

SAMPLE_RATE_HZ = 100_000
READ_TIMEOUT_S = 0.05
METADATA_TIMEOUT_S = 2.0
SAMPLE_BYTES = 4
# Samples looked at to find where whole samples start.
ALIGN_SAMPLES = 1000
# Fewer samples than this share of the expected ones means data was lost.
MIN_SAMPLE_RATIO = 0.95
# How far, and how often, the supply is lowered at a time.
VOLTAGE_STEP_DOWN_MV = 50
VOLTAGE_STEP_S = 0.05


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


class Ppk2(Supply):
    measures = True

    def __init__(self, port="auto", voltage_mv=None):
        from ppk2_api.ppk2_api import PPK2_API

        super().__init__(port)
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
            raise PowerError(f"no PPK2 answering on {', '.join(ports) or 'any port'}")

        # ppk2-api's own reader keeps only the last chunk of the metadata,
        # losing the calibration.
        ppk._parse_metadata(metadata)
        ppk.use_source_meter()
        self.port = candidate
        vdd = re.search(r"^VDD: (\d+)$", metadata, re.MULTILINE)
        self.voltage_mv = int(vdd.group(1)) if vdd else None
        self._ppk = ppk
        self._lock = threading.Lock()
        # ppk2-api only measures once a voltage is set in this session.
        voltage_mv = voltage_mv or self.voltage_mv
        if voltage_mv is None:
            raise PowerError(f"the PPK2 on {candidate} reports no voltage: pass one")
        self.set_voltage(voltage_mv)

    def set_voltage(self, voltage_mv):
        # A large step down resets the PPK2 (it drops off USB); small ones do not.
        with self._lock:
            current = self.voltage_mv or voltage_mv
            while current - voltage_mv > VOLTAGE_STEP_DOWN_MV:
                current -= VOLTAGE_STEP_DOWN_MV
                self._ppk.set_source_voltage(current)
                time.sleep(VOLTAGE_STEP_S)
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

    def on(self):
        self._ppk.toggle_DUT_power("ON")
        self.powered = True

    def off(self):
        self._ppk.toggle_DUT_power("OFF")
        self.powered = False

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
        # Decoding samples in Python cannot keep up with the stream, so the
        # raw data is only collected while measuring and decoded after.
        measurement = Measurement(name, self.voltage_mv, SAMPLE_RATE_HZ)
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
            raise PowerError(
                f"the PPK2 returned {len(measurement.samples)} samples in "
                f"{elapsed:.2f} s, expected about {int(expected)}: samples were "
                "dropped"
            )
