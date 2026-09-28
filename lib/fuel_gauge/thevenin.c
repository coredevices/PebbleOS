/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "estimator.h"

#include <math.h>

#include <pbl/fuel_gauge/fuel_gauge.h>
#include <pbl/util/hash.h>

// SOC uncertainty when starting from a voltage that may be under load, with
// the polarization voltage unknown.
#define INIT_SOC_STD 0.1f
#define INIT_V1_STD  0.02f

// SOC uncertainty right after the charger terminated.
#define FULL_SOC_STD 0.01f

// Temperatures outside this range are clamped before scaling resistances.
#define T_MIN_C -20.0f
#define T_MAX_C 60.0f
#define T_REF_C 25.0f

static float prv_r_scale(const struct pbl_fuel_gauge_estimator *e, float t) {
  return expf(e->r_temp_coeff * (T_REF_C - pbl_fuel_gauge__clampf(t, T_MIN_C, T_MAX_C)));
}

bool pbl_fuel_gauge__est_valid(const struct pbl_fuel_gauge_config *config) {
  const struct pbl_fuel_gauge_estimator *e = config->estimator;

  return (e->r1 >= 0.0f) && ((e->tau1 > 0.0f) || (e->r1 == 0.0f)) && (e->voltage_noise > 0.0f) &&
         (e->current_noise > 0.0f);
}

uint32_t pbl_fuel_gauge__est_hash(const struct pbl_fuel_gauge_config *config, uint32_t h) {
  const struct pbl_fuel_gauge_estimator *e = config->estimator;
  const float params[] = {e->r1, e->tau1, e->r_temp_coeff};

  return hash_update(h, params, sizeof(params));
}

void pbl_fuel_gauge__est_init(const struct pbl_fuel_gauge_config *config,
                              struct pbl_fuel_gauge_estimator_state *st,
                              const struct pbl_fuel_gauge_meas *meas) {
  const float r0 = config->cell->r0 * prv_r_scale(config->estimator, meas->t);

  *st = (struct pbl_fuel_gauge_estimator_state){
    .soc = pbl_fuel_gauge__soc_from_ocv(config->cell, meas->v + r0 * meas->i),
    .p_soc = INIT_SOC_STD * INIT_SOC_STD,
    .p_v1 = INIT_V1_STD * INIT_V1_STD,
  };
}

bool pbl_fuel_gauge__est_state_valid(const struct pbl_fuel_gauge_estimator_state *st) {
  return (st->soc >= 0.0f) && (st->soc <= 1.0f) && isfinite(st->v1) && (st->p_soc > 0.0f) &&
         (st->p_v1 > 0.0f) && isfinite(st->p_cross);
}

float pbl_fuel_gauge__est_rest_voltage(const struct pbl_fuel_gauge_config *config,
                                       const struct pbl_fuel_gauge_estimator_state *st,
                                       const struct pbl_fuel_gauge_meas *meas) {
  return meas->v + st->v1 + config->cell->r0 * prv_r_scale(config->estimator, meas->t) * meas->i;
}

// Coulomb counting and polarization decay over dt, with the current held at
// the new sample. Both states are driven by the same current error, hence the
// noise covariance g * g^T * current_noise^2.
static void prv_predict(const struct pbl_fuel_gauge_config *config,
                        struct pbl_fuel_gauge_estimator_state *st,
                        const struct pbl_fuel_gauge_meas *meas, float dt) {
  const struct pbl_fuel_gauge_estimator *e = config->estimator;
  const float var_i = e->current_noise * e->current_noise;
  float a, g0, g1;

  a = (e->tau1 > 0.0f) ? expf(-dt / e->tau1) : 0.0f;
  g0 = -dt / (config->cell->capacity_ah * 3600.0f);
  g1 = e->r1 * prv_r_scale(e, meas->t) * (1.0f - a);

  st->soc += g0 * meas->i;
  st->v1 = a * st->v1 + g1 * meas->i;

  st->p_soc += g0 * g0 * var_i;
  st->p_cross = a * st->p_cross + g0 * g1 * var_i;
  st->p_v1 = a * a * st->p_v1 + g1 * g1 * var_i;
}

// Kalman correction with the terminal voltage v = OCV(soc) - v1 - R0 * i,
// linearized as H = [dOCV/dSOC, -1].
static void prv_correct(const struct pbl_fuel_gauge_config *config,
                        struct pbl_fuel_gauge_estimator_state *st,
                        const struct pbl_fuel_gauge_meas *meas) {
  const struct pbl_fuel_gauge_estimator *e = config->estimator;
  const float var_v = e->voltage_noise * e->voltage_noise;
  float h0, v_pred, ph0, ph1, var_e, k0, k1, err;

  v_pred = pbl_fuel_gauge__ocv(config->cell, st->soc, &h0) - st->v1 -
           config->cell->r0 * prv_r_scale(e, meas->t) * meas->i;

  ph0 = st->p_soc * h0 - st->p_cross;
  ph1 = st->p_cross * h0 - st->p_v1;
  var_e = h0 * ph0 - ph1 + var_v;
  k0 = ph0 / var_e;
  k1 = ph1 / var_e;
  err = meas->v - v_pred;

  st->soc = pbl_fuel_gauge__clampf(st->soc + k0 * err, 0.0f, 1.0f);
  st->v1 += k1 * err;

  st->p_soc -= k0 * k0 * var_e;
  st->p_cross -= k0 * k1 * var_e;
  st->p_v1 -= k1 * k1 * var_e;
}

void pbl_fuel_gauge__est_update(const struct pbl_fuel_gauge_config *config,
                                struct pbl_fuel_gauge_estimator_state *st,
                                const struct pbl_fuel_gauge_meas *meas, float dt) {
  if (dt > 0.0f) {
    prv_predict(config, st, meas, dt);
  }
  prv_correct(config, st, meas);
}

void pbl_fuel_gauge__est_full(struct pbl_fuel_gauge_estimator_state *st) {
  st->soc = 1.0f;
  st->p_soc = FULL_SOC_STD * FULL_SOC_STD;
  st->p_cross = 0.0f;
}

float pbl_fuel_gauge__est_soc(const struct pbl_fuel_gauge_estimator_state *st) {
  return st->soc;
}

float pbl_fuel_gauge__est_soc_var(const struct pbl_fuel_gauge_estimator_state *st) {
  return st->p_soc;
}

float pbl_fuel_gauge__est_dc_resistance(const struct pbl_fuel_gauge_config *config, float t) {
  return (config->cell->r0 + config->estimator->r1) * prv_r_scale(config->estimator, t);
}
