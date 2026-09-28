/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "estimator.h"

#include <errno.h>
#include <math.h>
#include <string.h>

#include <pbl/fuel_gauge/fuel_gauge.h>
#include <pbl/util/hash.h>

// Below this SOC uncertainty the reported SOC only moves in the direction the
// battery is going; above it, it follows the estimate while it converges.
#define SETTLED_SOC_STD 0.02f

// A saved state further than this from the SOC implied by the voltage belongs
// to another battery, or to one that sat without power for long.
#define RESTORE_MAX_SOC_ERROR 0.25f

// A saved reported level whose OCV is further than this below the voltage
// would not rise again while discharging, e.g. a stale 0% on a charged cell.
#define RESTORE_MAX_REPORTED_LAG_V 0.05f

// Time constant and settling time of the average current used for TTE/TTF.
#define AVG_TAU_S      (3.0f * 3600.0f)
#define AVG_MIN_TIME_S (10.0f * 60.0f)

float pbl_fuel_gauge__clampf(float x, float lo, float hi) {
  return (x < lo) ? lo : ((x > hi) ? hi : x);
}

float pbl_fuel_gauge__ocv(const struct pbl_fuel_gauge_cell *cell, float soc, float *slope) {
  const struct pbl_fuel_gauge_ocv_point *p = cell->ocv;
  uint8_t j = 0;

  while ((j < cell->ocv_count - 2) && (soc > p[j + 1].soc)) {
    j++;
  }

  *slope = (p[j + 1].v - p[j].v) / (p[j + 1].soc - p[j].soc);
  return p[j].v +
         *slope * (pbl_fuel_gauge__clampf(soc, p[0].soc, p[cell->ocv_count - 1].soc) - p[j].soc);
}

float pbl_fuel_gauge__soc_from_ocv(const struct pbl_fuel_gauge_cell *cell, float v) {
  const struct pbl_fuel_gauge_ocv_point *p = cell->ocv;
  uint8_t j = 0;

  if (v <= p[0].v) {
    return p[0].soc;
  }
  if (v >= p[cell->ocv_count - 1].v) {
    return p[cell->ocv_count - 1].soc;
  }

  while (v > p[j + 1].v) {
    j++;
  }

  return p[j].soc + (v - p[j].v) * (p[j + 1].soc - p[j].soc) / (p[j + 1].v - p[j].v);
}

static float prv_capacity_as(const struct pbl_fuel_gauge_cell *cell) {
  return cell->capacity_ah * 3600.0f;
}

static uint32_t prv_model_id(const struct pbl_fuel_gauge_config *c) {
  const struct pbl_fuel_gauge_cell *cell = c->cell;
  const uint32_t layout = sizeof(struct pbl_fuel_gauge_state);
  const float params[] = {cell->capacity_ah, cell->r0};
  uint32_t h;

  h = hash_update(HASH_INIT, &layout, sizeof(layout));
  h = hash_update(h, params, sizeof(params));
  h = hash_update(h, cell->ocv, cell->ocv_count * sizeof(cell->ocv[0]));

  return pbl_fuel_gauge__est_hash(c, h);
}

static bool prv_config_valid(const struct pbl_fuel_gauge_config *c) {
  const struct pbl_fuel_gauge_cell *cell = c->cell;

  if ((cell == NULL) || (c->estimator == NULL) || (cell->ocv == NULL) || (cell->ocv_count < 2U) ||
      !(cell->capacity_ah > 0.0f) || !(cell->r0 >= 0.0f) || !(c->term_current > 0.0f)) {
    return false;
  }

  for (uint8_t j = 1; j < cell->ocv_count; j++) {
    if (!(cell->ocv[j].soc > cell->ocv[j - 1].soc) || !(cell->ocv[j].v > cell->ocv[j - 1].v)) {
      return false;
    }
  }

  return (cell->ocv[0].soc == 0.0f) && (cell->ocv[cell->ocv_count - 1].soc == 1.0f) &&
         pbl_fuel_gauge__est_valid(c);
}

static bool prv_saved_usable(const struct pbl_fuel_gauge *fg,
                             const struct pbl_fuel_gauge_meas *meas,
                             const struct pbl_fuel_gauge_state *s) {
  const struct pbl_fuel_gauge_cell *cell = fg->config->cell;
  float soc, ocv, slope;

  if ((s->model_id != prv_model_id(fg->config)) ||
      !pbl_fuel_gauge__est_state_valid(&s->estimator) ||
      !(s->soc_reported >= 0.0f && s->soc_reported <= 1.0f) || !isfinite(s->avg_charge) ||
      !(s->avg_time >= 0.0f)) {
    return false;
  }

  soc = pbl_fuel_gauge__est_soc(&s->estimator);
  ocv = pbl_fuel_gauge__est_rest_voltage(fg->config, &s->estimator, meas);

  return (fabsf(pbl_fuel_gauge__soc_from_ocv(cell, ocv) - soc) <= RESTORE_MAX_SOC_ERROR) &&
         (ocv - pbl_fuel_gauge__ocv(cell, s->soc_reported, &slope) <= RESTORE_MAX_REPORTED_LAG_V);
}

int pbl_fuel_gauge_init(struct pbl_fuel_gauge *fg, const struct pbl_fuel_gauge_config *config,
                        const struct pbl_fuel_gauge_meas *meas,
                        const struct pbl_fuel_gauge_state *saved) {
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
  s->model_id = prv_model_id(config);
  pbl_fuel_gauge__est_init(config, &s->estimator, meas);
  s->soc_reported = pbl_fuel_gauge__est_soc(&s->estimator);

  return 0;
}

float pbl_fuel_gauge_update(struct pbl_fuel_gauge *fg, const struct pbl_fuel_gauge_meas *meas,
                            float dt, enum pbl_fuel_gauge_charge_state charge_state) {
  struct pbl_fuel_gauge_state *s = &fg->state;
  float soc;
  bool settled;

  if (charge_state != fg->charge_state) {
    fg->charge_state = charge_state;
    s->avg_charge = 0.0f;
    s->avg_time = 0.0f;
  }

  if (dt > 0.0f) {
    const float w = expf(-dt / AVG_TAU_S);

    s->avg_charge = s->avg_charge * w + meas->i * dt;
    s->avg_time = s->avg_time * w + dt;
  }

  fg->last = *meas;
  pbl_fuel_gauge__est_update(fg->config, &s->estimator, meas, dt);
  if (charge_state == PBL_FUEL_GAUGE_CHARGE_COMPLETE) {
    pbl_fuel_gauge__est_full(&s->estimator);
  }

  soc = pbl_fuel_gauge__est_soc(&s->estimator);
  settled = pbl_fuel_gauge__est_soc_var(&s->estimator) <= SETTLED_SOC_STD * SETTLED_SOC_STD;

  switch (charge_state) {
    case PBL_FUEL_GAUGE_CHARGE_COMPLETE:
      s->soc_reported = 1.0f;
      break;
    case PBL_FUEL_GAUGE_CHARGING_CC:
    case PBL_FUEL_GAUGE_CHARGING_CV:
      s->soc_reported = settled ? fmaxf(s->soc_reported, soc) : soc;
      break;
    case PBL_FUEL_GAUGE_DISCHARGING:
      s->soc_reported = settled ? fminf(s->soc_reported, soc) : soc;
      break;
  }

  return s->soc_reported * 100.0f;
}

float pbl_fuel_gauge_soc_get(const struct pbl_fuel_gauge *fg) {
  return pbl_fuel_gauge__est_soc(&fg->state.estimator);
}

int pbl_fuel_gauge_tte_get(const struct pbl_fuel_gauge *fg, uint32_t *seconds) {
  const struct pbl_fuel_gauge_state *s = &fg->state;
  float i_avg;

  if ((fg->charge_state != PBL_FUEL_GAUGE_DISCHARGING) || (s->avg_time < AVG_MIN_TIME_S)) {
    return -ENODATA;
  }

  i_avg = s->avg_charge / s->avg_time;
  if (!(i_avg > 0.0f)) {
    return -ENODATA;
  }

  *seconds = (uint32_t)(s->soc_reported * prv_capacity_as(fg->config->cell) / i_avg);

  return 0;
}

// A constant-voltage phase drains the current exponentially from i0 to the
// termination current: charging q takes q / (i0 - i_term) * ln(i0 / i_term).
static float prv_cv_time(float q, float i0, float i_term) {
  return (i0 > i_term) ? (q / (i0 - i_term) * logf(i0 / i_term)) : 0.0f;
}

// In constant current, the charger switches to constant voltage when the OCV
// plus the drop of the current across the cell reaches the termination
// voltage.
int pbl_fuel_gauge_ttf_get(const struct pbl_fuel_gauge *fg, uint32_t *seconds) {
  const struct pbl_fuel_gauge_config *c = fg->config;
  const struct pbl_fuel_gauge_state *s = &fg->state;
  const float q = prv_capacity_as(c->cell);
  const float soc = pbl_fuel_gauge__est_soc(&s->estimator);
  float i, soc_cv, t;

  switch (fg->charge_state) {
    case PBL_FUEL_GAUGE_CHARGING_CV:
      i = -fg->last.i;
      t = prv_cv_time((1.0f - soc) * q, i, c->term_current);
      break;
    case PBL_FUEL_GAUGE_CHARGING_CC:
      if (s->avg_time < AVG_MIN_TIME_S) {
        return -ENODATA;
      }
      i = -s->avg_charge / s->avg_time;
      if (!(i > 0.0f)) {
        return -ENODATA;
      }
      soc_cv = pbl_fuel_gauge__soc_from_ocv(
          c->cell, c->term_voltage - i * pbl_fuel_gauge__est_dc_resistance(c, fg->last.t));
      t = fmaxf(soc_cv - soc, 0.0f) * q / i +
          prv_cv_time((1.0f - fmaxf(soc, soc_cv)) * q, i, c->term_current);
      break;
    default:
      return -ENODATA;
  }

  *seconds = (uint32_t)t;

  return 0;
}

void pbl_fuel_gauge_state_get(const struct pbl_fuel_gauge *fg, struct pbl_fuel_gauge_state *state) {
  *state = fg->state;
}
