# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Current measurement through the supply powering the watch
(:mod:`pbl.power`), which replaces its battery."""

import contextlib
import time

# An idle measurement: long enough to average out transients, after the
# watch has settled from whatever the test did.
IDLE_MEASURE_S = 60
IDLE_SETTLE_S = 10
IDLE_MARGIN_S = 5


class Power:
    """The ``power`` fixture: a :class:`pbl.power.Supply` powering ``dut``,
    saving what it measures in the test's results."""

    def __init__(self, supply, dut, results_dir):
        self.supply = supply
        self.dut = dut
        self.results_dir = results_dir
        self.measurements = []

    def on(self):
        self.supply.on()

    def off(self):
        self.supply.off()

    def cycle(self, off_s=1.0):
        self.supply.cycle(off_s)

    @contextlib.contextmanager
    def measure(self, name="measurement"):
        with self.supply.measure(name) as m:
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
