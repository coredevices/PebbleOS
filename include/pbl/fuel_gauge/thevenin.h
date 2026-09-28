/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

//! Thevenin estimator: an extended Kalman filter over the cell's R0 in series
//! with one R1||C1 polarization pair, combining coulomb counting (prediction)
//! with the measured terminal voltage (correction).

//! Estimator parameters.
struct pbl_fuel_gauge_estimator {
  //! Polarization resistance at 25 C [ohm], 0 without an RC pair.
  float r1;
  //! Polarization time constant R1 * C1 [s], 0 when r1 is 0.
  float tau1;
  //! Resistances scale by exp(r_temp_coeff * (25 - T)) [1/C].
  float r_temp_coeff;
  //! Standard deviation of the voltage model and measurement error [V].
  float voltage_noise;
  //! Standard deviation of the error of a current sample as the average
  //! current over the time since the previous sample [A].
  float current_noise;
};

//! Estimator state.
struct pbl_fuel_gauge_estimator_state {
  //! SOC and polarization voltage.
  float soc;
  float v1;
  //! Covariance: var(soc), cov(soc, v1), var(v1).
  float p_soc;
  float p_cross;
  float p_v1;
};

//! Estimator parameters from Kconfig.
#define PBL_FUEL_GAUGE_ESTIMATOR_KCONFIG                                              \
  {                                                                                   \
    .r1 = (float)CONFIG_FUEL_GAUGE_THEVENIN_RC_MILLIOHM / 1000.0f,                    \
    .tau1 = (float)CONFIG_FUEL_GAUGE_THEVENIN_RC_TAU_MS / 1000.0f,                    \
    .r_temp_coeff = (float)CONFIG_FUEL_GAUGE_THEVENIN_R_TEMP_COEFF_PPM / 1000000.0f,  \
    .voltage_noise = (float)CONFIG_FUEL_GAUGE_THEVENIN_VOLTAGE_NOISE_UV / 1000000.0f, \
    .current_noise = (float)CONFIG_FUEL_GAUGE_THEVENIN_CURRENT_NOISE_UA / 1000000.0f, \
  }
