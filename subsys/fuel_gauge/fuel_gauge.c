/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/fuel_gauge/fuel_gauge.h>

#include <errno.h>
#include <math.h>
#include <string.h>

// SOC uncertainty when starting from a voltage that may be under load, with
// the polarization voltage unknown.
#define INIT_SOC_STD 0.1f
#define INIT_V1_STD  0.02f

// Below this SOC uncertainty the reported SOC only moves in the direction the
// battery is going; above it, it follows the estimate while it converges.
#define SETTLED_SOC_STD 0.02f

// SOC uncertainty right after the charger terminated.
#define COMPLETE_SOC_STD 0.01f

// A saved state further than this from the SOC implied by the voltage belongs
// to another battery, or to one that sat without power for long.
#define RESTORE_MAX_SOC_ERROR 0.25f

// Time constant and settling time of the average current used for TTE/TTF.
#define AVG_TAU_S      (3.0f * 3600.0f)
#define AVG_MIN_TIME_S (10.0f * 60.0f)

// Temperatures outside this range are clamped before scaling resistances.
#define T_MIN_C -20.0f
#define T_MAX_C 60.0f
#define T_REF_C 25.0f

static float prv_clampf(float x, float lo, float hi) {
  return (x < lo) ? lo : ((x > hi) ? hi : x);
}

static float prv_capacity_as(const struct pbl_fuel_gauge_model *m) {
  return m->capacity_ah * 3600.0f;
}

static float prv_r_scale(const struct pbl_fuel_gauge_model *m, float t) {
  return expf(m->r_temp_coeff * (T_REF_C - prv_clampf(t, T_MIN_C, T_MAX_C)));
}

// Linear interpolation of the OCV curve. The slope dOCV/dSOC is the
// measurement Jacobian of the filter.
static float prv_ocv(const struct pbl_fuel_gauge_model *m, float soc, float *slope) {
  const struct pbl_fuel_gauge_ocv_point *p = m->ocv;
  uint8_t j = 0;

  while ((j < m->ocv_count - 2) && (soc > p[j + 1].soc)) {
    j++;
  }

  *slope = (p[j + 1].v - p[j].v) / (p[j + 1].soc - p[j].soc);
  return p[j].v + *slope * (prv_clampf(soc, p[0].soc, p[m->ocv_count - 1].soc) - p[j].soc);
}

static float prv_soc_from_ocv(const struct pbl_fuel_gauge_model *m, float v) {
  const struct pbl_fuel_gauge_ocv_point *p = m->ocv;
  uint8_t j = 0;

  if (v <= p[0].v) {
    return p[0].soc;
  }
  if (v >= p[m->ocv_count - 1].v) {
    return p[m->ocv_count - 1].soc;
  }

  while (v > p[j + 1].v) {
    j++;
  }

  return p[j].soc + (v - p[j].v) * (p[j + 1].soc - p[j].soc) / (p[j + 1].v - p[j].v);
}

static uint32_t prv_fnv1a(uint32_t hash, const void *data, size_t len) {
  const uint8_t *bytes = data;

  for (size_t i = 0; i < len; i++) {
    hash = (hash ^ bytes[i]) * 16777619U;
  }

  return hash;
}

static uint32_t prv_model_id(const struct pbl_fuel_gauge_model *m) {
  const uint32_t layout = sizeof(struct pbl_fuel_gauge_state);
  const float params[] = {m->capacity_ah, m->r0, m->r1, m->tau1, m->r_temp_coeff};
  uint32_t hash = 2166136261U;

  hash = prv_fnv1a(hash, &layout, sizeof(layout));
  hash = prv_fnv1a(hash, params, sizeof(params));
  hash = prv_fnv1a(hash, m->ocv, m->ocv_count * sizeof(m->ocv[0]));

  return hash;
}

static bool prv_config_valid(const struct pbl_fuel_gauge_config *c) {
  const struct pbl_fuel_gauge_model *m = c->model;

  if ((m == NULL) || (m->ocv == NULL) || (m->ocv_count < 2U) || !(m->capacity_ah > 0.0f) ||
      !(m->r0 >= 0.0f) || !(m->r1 >= 0.0f) || !(m->tau1 > 0.0f) || !(c->voltage_noise > 0.0f) ||
      !(c->current_noise > 0.0f) || !(c->term_current > 0.0f)) {
    return false;
  }

  for (uint8_t j = 1; j < m->ocv_count; j++) {
    if (!(m->ocv[j].soc > m->ocv[j - 1].soc) || !(m->ocv[j].v > m->ocv[j - 1].v)) {
      return false;
    }
  }

  return (m->ocv[0].soc == 0.0f) && (m->ocv[m->ocv_count - 1].soc == 1.0f);
}

static bool prv_saved_usable(const struct pbl_fuel_gauge *fg,
                             const struct pbl_fuel_gauge_meas *meas,
                             const struct pbl_fuel_gauge_state *s) {
  const struct pbl_fuel_gauge_model *m = fg->config->model;
  float soc_v;

  if ((s->model_id != prv_model_id(m)) || !(s->soc >= 0.0f && s->soc <= 1.0f) || !isfinite(s->v1) ||
      !(s->p_soc > 0.0f) || !(s->p_v1 > 0.0f) || !isfinite(s->p_cross) ||
      !(s->soc_reported >= 0.0f && s->soc_reported <= 1.0f) || !isfinite(s->avg_charge) ||
      !(s->avg_time >= 0.0f)) {
    return false;
  }

  soc_v = prv_soc_from_ocv(m, meas->v + s->v1 + m->r0 * prv_r_scale(m, meas->t) * meas->i);

  return fabsf(soc_v - s->soc) <= RESTORE_MAX_SOC_ERROR;
}

int pbl_fuel_gauge_init(struct pbl_fuel_gauge *fg, const struct pbl_fuel_gauge_config *config,
                        const struct pbl_fuel_gauge_meas *meas,
                        const struct pbl_fuel_gauge_state *saved) {
  const struct pbl_fuel_gauge_model *m = config->model;
  struct pbl_fuel_gauge_state *s = &fg->state;

  if (!prv_config_valid(config)) {
    return -EINVAL;
  }

  fg->config = config;
  fg->charge_state = PBL_FUEL_GAUGE_DISCHARGING;
  fg->last = *meas;

  if ((saved != NULL) && prv_saved_usable(fg, meas, saved)) {
    *s = *saved;
    return 1;
  }

  memset(s, 0, sizeof(*s));
  s->model_id = prv_model_id(m);
  s->soc = prv_soc_from_ocv(m, meas->v + m->r0 * prv_r_scale(m, meas->t) * meas->i);
  s->p_soc = INIT_SOC_STD * INIT_SOC_STD;
  s->p_v1 = INIT_V1_STD * INIT_V1_STD;
  s->soc_reported = s->soc;

  return 0;
}

// Coulomb counting and polarization decay over dt, with the current held at
// the new sample. Both states are driven by the same current error, hence the
// noise covariance g * g^T * current_noise^2.
static void prv_predict(struct pbl_fuel_gauge *fg, const struct pbl_fuel_gauge_meas *meas,
                        float dt) {
  const struct pbl_fuel_gauge_model *m = fg->config->model;
  const float var_i = fg->config->current_noise * fg->config->current_noise;
  struct pbl_fuel_gauge_state *s = &fg->state;
  float a, g0, g1;

  a = expf(-dt / m->tau1);
  g0 = -dt / prv_capacity_as(m);
  g1 = m->r1 * prv_r_scale(m, meas->t) * (1.0f - a);

  s->soc += g0 * meas->i;
  s->v1 = a * s->v1 + g1 * meas->i;

  s->p_soc += g0 * g0 * var_i;
  s->p_cross = a * s->p_cross + g0 * g1 * var_i;
  s->p_v1 = a * a * s->p_v1 + g1 * g1 * var_i;
}

// Kalman correction with the terminal voltage v = OCV(soc) - v1 - R0 * i,
// linearized as H = [dOCV/dSOC, -1].
static void prv_correct(struct pbl_fuel_gauge *fg, const struct pbl_fuel_gauge_meas *meas) {
  const struct pbl_fuel_gauge_model *m = fg->config->model;
  const float var_v = fg->config->voltage_noise * fg->config->voltage_noise;
  struct pbl_fuel_gauge_state *s = &fg->state;
  float h0, v_pred, ph0, ph1, var_e, k0, k1, e;

  v_pred = prv_ocv(m, s->soc, &h0) - s->v1 - m->r0 * prv_r_scale(m, meas->t) * meas->i;

  ph0 = s->p_soc * h0 - s->p_cross;
  ph1 = s->p_cross * h0 - s->p_v1;
  var_e = h0 * ph0 - ph1 + var_v;
  k0 = ph0 / var_e;
  k1 = ph1 / var_e;
  e = meas->v - v_pred;

  s->soc = prv_clampf(s->soc + k0 * e, 0.0f, 1.0f);
  s->v1 += k1 * e;

  s->p_soc -= k0 * k0 * var_e;
  s->p_cross -= k0 * k1 * var_e;
  s->p_v1 -= k1 * k1 * var_e;
}

float pbl_fuel_gauge_update(struct pbl_fuel_gauge *fg, const struct pbl_fuel_gauge_meas *meas,
                            float dt, enum pbl_fuel_gauge_charge_state charge_state) {
  struct pbl_fuel_gauge_state *s = &fg->state;
  bool settled;

  if (charge_state != fg->charge_state) {
    fg->charge_state = charge_state;
    s->avg_charge = 0.0f;
    s->avg_time = 0.0f;
  }

  if (dt > 0.0f) {
    const float w = expf(-dt / AVG_TAU_S);

    prv_predict(fg, meas, dt);
    s->avg_charge = s->avg_charge * w + meas->i * dt;
    s->avg_time = s->avg_time * w + dt;
  }

  fg->last = *meas;
  prv_correct(fg, meas);
  settled = s->p_soc <= SETTLED_SOC_STD * SETTLED_SOC_STD;

  switch (charge_state) {
    case PBL_FUEL_GAUGE_CHARGE_COMPLETE:
      s->soc = 1.0f;
      s->p_soc = COMPLETE_SOC_STD * COMPLETE_SOC_STD;
      s->p_cross = 0.0f;
      s->soc_reported = 1.0f;
      break;
    case PBL_FUEL_GAUGE_CHARGING_CC:
    case PBL_FUEL_GAUGE_CHARGING_CV:
      s->soc_reported = settled ? fmaxf(s->soc_reported, s->soc) : s->soc;
      break;
    case PBL_FUEL_GAUGE_DISCHARGING:
      s->soc_reported = settled ? fminf(s->soc_reported, s->soc) : s->soc;
      break;
  }

  return s->soc_reported * 100.0f;
}

float pbl_fuel_gauge_soc_get(const struct pbl_fuel_gauge *fg) {
  return fg->state.soc;
}

bool pbl_fuel_gauge_tte_get(const struct pbl_fuel_gauge *fg, uint32_t *seconds) {
  const struct pbl_fuel_gauge_state *s = &fg->state;
  float i_avg;

  if ((fg->charge_state != PBL_FUEL_GAUGE_DISCHARGING) || (s->avg_time < AVG_MIN_TIME_S)) {
    return false;
  }

  i_avg = s->avg_charge / s->avg_time;
  if (!(i_avg > 0.0f)) {
    return false;
  }

  *seconds = (uint32_t)(s->soc_reported * prv_capacity_as(fg->config->model) / i_avg);

  return true;
}

// A constant-voltage phase drains the current exponentially from i0 to the
// termination current: charging q takes q / (i0 - i_term) * ln(i0 / i_term).
static float prv_cv_time(float q, float i0, float i_term) {
  return (i0 > i_term) ? (q / (i0 - i_term) * logf(i0 / i_term)) : 0.0f;
}

// In constant current, the charger switches to constant voltage when
// OCV + i * (R0 + R1) reaches the termination voltage.
bool pbl_fuel_gauge_ttf_get(const struct pbl_fuel_gauge *fg, uint32_t *seconds) {
  const struct pbl_fuel_gauge_config *c = fg->config;
  const struct pbl_fuel_gauge_model *m = c->model;
  const struct pbl_fuel_gauge_state *s = &fg->state;
  const float q = prv_capacity_as(m);
  float i, soc_cv, t;

  switch (fg->charge_state) {
    case PBL_FUEL_GAUGE_CHARGING_CV:
      i = -fg->last.i;
      t = prv_cv_time((1.0f - s->soc) * q, i, c->term_current);
      break;
    case PBL_FUEL_GAUGE_CHARGING_CC:
      if (s->avg_time < AVG_MIN_TIME_S) {
        return false;
      }
      i = -s->avg_charge / s->avg_time;
      if (!(i > 0.0f)) {
        return false;
      }
      soc_cv =
          prv_soc_from_ocv(m, c->term_voltage - i * (m->r0 + m->r1) * prv_r_scale(m, fg->last.t));
      t = fmaxf(soc_cv - s->soc, 0.0f) * q / i +
          prv_cv_time((1.0f - fmaxf(s->soc, soc_cv)) * q, i, c->term_current);
      break;
    default:
      return false;
  }

  *seconds = (uint32_t)t;

  return true;
}

void pbl_fuel_gauge_state_get(const struct pbl_fuel_gauge *fg, struct pbl_fuel_gauge_state *state) {
  *state = fg->state;
}
