# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import os
import sys
import tempfile
import unittest

import numpy as np

root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))
sys.path.insert(0, root_dir)

import fuel_gauge

MODEL = fuel_gauge.Model(
    name="test",
    capacity_ah=0.15,
    ocv_soc=np.linspace(0.0, 1.0, 11),
    ocv_v=np.array([3.30, 3.62, 3.69, 3.74, 3.78, 3.83, 3.90, 3.98, 4.07, 4.17, 4.30]),
    r0=0.4,
    r1=0.15,
    tau1=80.0,
    r_temp_coeff=0.03,
)


def _pulse_log(model, temp, seed):
    t, i = fuel_gauge.pulse_profile(model.capacity_ah, 0.5, 144.0, 600.0, 2.0)
    temps = np.full_like(t, temp)
    v, _ = fuel_gauge.simulate(model, t, i, temps)
    v += np.random.default_rng(seed).normal(0.0, 0.002, len(v))
    return fuel_gauge.Log(t, v, i, temps, np.zeros(len(t), dtype=int))


class TestFuelGaugeFit(unittest.TestCase):
    def test_recovers_model(self):
        logs = [_pulse_log(MODEL, 25.0, 0), _pulse_log(MODEL, 5.0, 1)]
        model, resid, _ = fuel_gauge.fit_model(logs, "fit", MODEL.ocv_soc, [1.0, 1.0])

        self.assertAlmostEqual(model.capacity_ah, MODEL.capacity_ah, delta=1e-4)
        self.assertLess(np.max(np.abs(model.ocv_v - MODEL.ocv_v)), 0.01)
        self.assertAlmostEqual(model.r0, MODEL.r0, delta=0.02)
        self.assertAlmostEqual(model.r1, MODEL.r1, delta=0.02)
        self.assertAlmostEqual(model.tau1, MODEL.tau1, delta=10.0)
        self.assertAlmostEqual(model.r_temp_coeff, MODEL.r_temp_coeff, delta=0.003)
        self.assertLess(np.sqrt(np.mean(resid**2)), 0.003)

    def test_replay_tracks_coulomb_count(self):
        log = _pulse_log(MODEL, 25.0, 2)
        with tempfile.TemporaryDirectory() as tmp:
            model_c = os.path.join(tmp, "model.c")
            fuel_gauge.write_model(model_c, MODEL)
            estimator = fuel_gauge.Estimator(model_c, tmp)
            self.assertEqual(estimator.model.name, "test")
            est, reported = estimator.run(log, 0.015, 0.005, 4.35, 0.015)

        ref = fuel_gauge.coulomb_count(log, 1.0, MODEL.capacity_ah)
        self.assertLess(np.max(np.abs(est - ref)), 0.02)
        self.assertTrue(np.all(np.diff(reported) <= 0.0))

    def test_extract(self):
        lines = [
            (
                "<D> battery_state.c:370> Battery state: v_mv: 4012, i_ua: 1500, t_mc: 24500, "
                "td_ms: 60000, fg: 0, soc: 80, tte: 0, ttf: 0"
            ),
            "unrelated line",
            (
                "<D> battery_state.c:370> Battery state: v_mv: 4010, i_ua: -90000, t_mc: 25000, "
                "td_ms: 30500, fg: 1, soc: 80, tte: 0, ttf: 0"
            ),
        ]
        with tempfile.TemporaryDirectory() as tmp:
            src = os.path.join(tmp, "log.txt")
            dst = os.path.join(tmp, "log.csv")
            with open(src, "w") as f:
                f.write("\n".join(lines))
            fuel_gauge.main(["extract", src, dst])
            log = fuel_gauge.read_log(dst)

        np.testing.assert_allclose(log.t, [60.0, 90.5])
        np.testing.assert_allclose(log.v, [4.012, 4.010])
        np.testing.assert_allclose(log.i, [0.0015, -0.09])
        np.testing.assert_allclose(log.temp, [24.5, 25.0])
        np.testing.assert_array_equal(log.state, [0, 1])


if __name__ == "__main__":
    unittest.main()
