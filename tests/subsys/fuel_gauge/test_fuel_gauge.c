/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include <pbl/fuel_gauge/fuel_gauge.h>

#include <errno.h>
#include <math.h>
#include <stdio.h>

// The estimator runs against a simulated cell: the same equivalent circuit,
// integrated in double precision at 1 s steps with the true (bursty) load,
// while the estimator only sees one sample per minute.

#define Q_AH     0.19
#define R0       0.30
#define R1       0.12
#define TAU1     100.0
#define V_TERM   4.35
#define I_TERM   0.019
#define I_CHARGE 0.19

static const struct pbl_fuel_gauge_ocv_point s_ocv[] = {
  {0.00f, 3.300f}, {0.05f, 3.600f}, {0.10f, 3.680f}, {0.20f, 3.740f},
  {0.30f, 3.780f}, {0.40f, 3.810f}, {0.50f, 3.850f}, {0.60f, 3.910f},
  {0.70f, 3.980f}, {0.80f, 4.060f}, {0.90f, 4.160f}, {1.00f, 4.300f},
};

static const struct pbl_fuel_gauge_model s_model = {
  .name = "test",
  .capacity_ah = Q_AH,
  .ocv = s_ocv,
  .ocv_count = sizeof(s_ocv) / sizeof(s_ocv[0]),
  .r0 = R0,
  .r1 = R1,
  .tau1 = TAU1,
  .r_temp_coeff = 0.03f,
};

static const struct pbl_fuel_gauge_config s_config = {
  .model = &s_model,
  .voltage_noise = 0.015f,
  .current_noise = 0.005f,
  .term_voltage = V_TERM,
  .term_current = I_TERM,
};

static struct pbl_fuel_gauge s_fg;

typedef struct {
  double soc;
  double v1;
  double t;
} Cell;

static double prv_ocv(double soc) {
  const size_t n = sizeof(s_ocv) / sizeof(s_ocv[0]);
  size_t j = 0;

  while ((j < n - 2) && (soc > s_ocv[j + 1].soc)) {
    j++;
  }
  return s_ocv[j].v +
         (s_ocv[j + 1].v - s_ocv[j].v) * (soc - s_ocv[j].soc) / (s_ocv[j + 1].soc - s_ocv[j].soc);
}

static double prv_r_scale(const Cell *c) {
  return exp(s_model.r_temp_coeff * (25.0 - c->t));
}

static double prv_cell_v(const Cell *c, double i) {
  return prv_ocv(c->soc) - c->v1 - R0 * prv_r_scale(c) * i;
}

static void prv_cell_step(Cell *c, double i, double dt) {
  const double a = exp(-dt / TAU1);

  c->soc -= i * dt / (Q_AH * 3600.0);
  c->v1 = a * c->v1 + R1 * prv_r_scale(c) * (1.0 - a) * i;
}

static struct pbl_fuel_gauge_meas prv_meas(const Cell *c, double i) {
  return (struct pbl_fuel_gauge_meas){.v = prv_cell_v(c, i), .i = i, .t = c->t};
}

static uint32_t s_rand;

static double prv_rand(void) {
  s_rand = s_rand * 1664525U + 1013904223U;
  return (double)(s_rand >> 8) / (double)(1U << 24);
}

// A watch-like load: 1 mA idle, 20 mA for about 5% of the seconds.
static double prv_bursty_load(void) {
  return (prv_rand() < 0.05) ? 0.020 : 0.001;
}

static void prv_assert_near(double a, double b, double tol, int line) {
  char desc[96];

  if (fabs(a - b) > tol) {
    snprintf(desc, sizeof(desc), "%f vs %f (tolerance %f)", a, b, tol);
    clar__assert(0, __FILE__, line, "values not near", desc, 1);
  }
}

#define assert_near(a, b, tol) prv_assert_near((a), (b), (tol), __LINE__)

// Runs one minute of load on the cell, then feeds the estimator the sample
// taken at the end of it. Returns the reported SOC.
static float prv_run_minute(Cell *c, double (*load)(void), double scale, double offset,
                            enum pbl_fuel_gauge_charge_state cs) {
  double i = 0.0;

  for (int s = 0; s < 60; s++) {
    i = load();
    prv_cell_step(c, i, 1.0);
  }

  struct pbl_fuel_gauge_meas meas = prv_meas(c, i);
  meas.i = (float)(i * scale + offset);
  return pbl_fuel_gauge_update(&s_fg, &meas, 60.0f, cs);
}

void test_fuel_gauge__initialize(void) {
  s_rand = 1;
}

void test_fuel_gauge__init_from_rest_voltage(void) {
  const Cell c = {.soc = 0.5, .t = 25.0};
  const struct pbl_fuel_gauge_meas meas = prv_meas(&c, 0.0);

  cl_assert_equal_i(pbl_fuel_gauge_init(&s_fg, &s_config, &meas, NULL), 0);
  assert_near(pbl_fuel_gauge_soc_get(&s_fg), 0.5, 0.002);
  assert_near(pbl_fuel_gauge_update(&s_fg, &meas, 0.0f, PBL_FUEL_GAUGE_DISCHARGING), 50.0, 0.2);
}

void test_fuel_gauge__init_compensates_load_and_temperature(void) {
  const Cell c = {.soc = 0.5, .t = 0.0};
  const struct pbl_fuel_gauge_meas meas = prv_meas(&c, 0.1);

  cl_assert_equal_i(pbl_fuel_gauge_init(&s_fg, &s_config, &meas, NULL), 0);
  assert_near(pbl_fuel_gauge_soc_get(&s_fg), 0.5, 0.002);
}

void test_fuel_gauge__rejects_invalid_model(void) {
  static const struct pbl_fuel_gauge_ocv_point ocv[] = {{0.0f, 3.5f}, {0.5f, 3.4f}, {1.0f, 4.2f}};
  struct pbl_fuel_gauge_model model = s_model;
  struct pbl_fuel_gauge_config config = s_config;
  const struct pbl_fuel_gauge_meas meas = {.v = 3.8f, .t = 25.0f};

  model.ocv = ocv;
  model.ocv_count = 3;
  config.model = &model;

  cl_assert_equal_i(pbl_fuel_gauge_init(&s_fg, &config, &meas, NULL), -EINVAL);
}

void test_fuel_gauge__tracks_bursty_discharge(void) {
  Cell c = {.soc = 0.95, .t = 25.0};
  struct pbl_fuel_gauge_meas meas = prv_meas(&c, 0.001);

  pbl_fuel_gauge_init(&s_fg, &s_config, &meas, NULL);

  while (c.soc > 0.05) {
    prv_run_minute(&c, prv_bursty_load, 1.0, 0.0, PBL_FUEL_GAUGE_DISCHARGING);
    assert_near(pbl_fuel_gauge_soc_get(&s_fg), c.soc, 0.04);
  }
}

// A 5% gain error and a 0.2 mA offset would drift pure coulomb counting by
// more than 20% over a full discharge; the voltage keeps the estimate close.
void test_fuel_gauge__corrects_current_errors(void) {
  Cell c = {.soc = 0.95, .t = 25.0};
  struct pbl_fuel_gauge_meas meas = prv_meas(&c, 0.001);

  pbl_fuel_gauge_init(&s_fg, &s_config, &meas, NULL);

  while (c.soc > 0.05) {
    prv_run_minute(&c, prv_bursty_load, 0.95, -0.0002, PBL_FUEL_GAUGE_DISCHARGING);
    assert_near(pbl_fuel_gauge_soc_get(&s_fg), c.soc, 0.05);
  }
}

void test_fuel_gauge__converges_from_wrong_start(void) {
  Cell c = {.soc = 0.6, .t = 25.0};
  Cell wrong = {.soc = 0.8, .t = 25.0};
  struct pbl_fuel_gauge_meas meas = prv_meas(&wrong, 0.0);

  pbl_fuel_gauge_init(&s_fg, &s_config, &meas, NULL);
  assert_near(pbl_fuel_gauge_soc_get(&s_fg), 0.8, 0.002);

  for (int m = 0; m < 6 * 60; m++) {
    prv_run_minute(&c, prv_bursty_load, 1.0, 0.0, PBL_FUEL_GAUGE_DISCHARGING);
  }

  assert_near(pbl_fuel_gauge_soc_get(&s_fg), c.soc, 0.03);
}

// A current sensor reading 30% high drains the estimate faster than the cell,
// so the voltage keeps pulling it back up. None of that may show.
void test_fuel_gauge__reported_never_rises_while_discharging(void) {
  Cell c = {.soc = 0.7, .t = 25.0};
  struct pbl_fuel_gauge_meas meas = prv_meas(&c, 0.0);
  float last;

  pbl_fuel_gauge_init(&s_fg, &s_config, &meas, NULL);
  last = pbl_fuel_gauge_update(&s_fg, &meas, 0.0f, PBL_FUEL_GAUGE_DISCHARGING);

  for (int m = 0; m < 30; m++) {
    last = prv_run_minute(&c, prv_bursty_load, 1.0, 0.0, PBL_FUEL_GAUGE_DISCHARGING);
  }

  int rises = 0;
  float soc = pbl_fuel_gauge_soc_get(&s_fg);
  for (int m = 0; m < 6 * 60; m++) {
    const float pct = prv_run_minute(&c, prv_bursty_load, 1.3, 0.0, PBL_FUEL_GAUGE_DISCHARGING);
    cl_assert(pct <= last);
    last = pct;
    rises += (pbl_fuel_gauge_soc_get(&s_fg) > soc) ? 1 : 0;
    soc = pbl_fuel_gauge_soc_get(&s_fg);
  }

  cl_assert(rises > 0);
}

void test_fuel_gauge__reported_follows_convergence(void) {
  Cell c = {.soc = 0.7, .t = 25.0};
  Cell low = {.soc = 0.5, .t = 25.0};
  struct pbl_fuel_gauge_meas meas = prv_meas(&low, 0.0);
  float pct = 0.0f;

  pbl_fuel_gauge_init(&s_fg, &s_config, &meas, NULL);
  pbl_fuel_gauge_update(&s_fg, &meas, 0.0f, PBL_FUEL_GAUGE_DISCHARGING);

  for (int m = 0; m < 30; m++) {
    pct = prv_run_minute(&c, prv_bursty_load, 1.0, 0.0, PBL_FUEL_GAUGE_DISCHARGING);
  }

  assert_near(pct, c.soc * 100.0, 2.0);
}

static double s_charge_current;

static double prv_charger_load(void) {
  return -s_charge_current;
}

// Charges the simulated cell like the nPM1300: constant current until the
// terminal voltage reaches V_TERM, then constant voltage until the current
// drops to I_TERM. Returns the charging time in seconds.
static double prv_charge(Cell *c, uint32_t *ttf_at_15min) {
  double t = 0.0;
  bool cv = false;

  s_charge_current = I_CHARGE;
  while (s_charge_current > I_TERM) {
    const enum pbl_fuel_gauge_charge_state cs =
        cv ? PBL_FUEL_GAUGE_CHARGING_CV : PBL_FUEL_GAUGE_CHARGING_CC;

    prv_run_minute(c, prv_charger_load, 1.0, 0.0, cs);
    t += 60.0;

    if (t == 900.0) {
      cl_assert(pbl_fuel_gauge_ttf_get(&s_fg, ttf_at_15min));
    }

    if (prv_cell_v(c, -s_charge_current) >= V_TERM) {
      cv = true;
    }
    if (cv) {
      s_charge_current = (V_TERM - prv_ocv(c->soc) + c->v1) / (R0 * prv_r_scale(c));
    }
  }

  return t;
}

void test_fuel_gauge__reported_never_drops_while_charging(void) {
  Cell c = {.soc = 0.3, .t = 25.0};
  struct pbl_fuel_gauge_meas meas = prv_meas(&c, 0.0);
  float last;

  pbl_fuel_gauge_init(&s_fg, &s_config, &meas, NULL);
  last = pbl_fuel_gauge_update(&s_fg, &meas, 0.0f, PBL_FUEL_GAUGE_CHARGING_CC);

  s_charge_current = I_CHARGE;
  for (int m = 0; m < 60; m++) {
    const float pct = prv_run_minute(&c, prv_charger_load, 1.0, 0.0, PBL_FUEL_GAUGE_CHARGING_CC);
    cl_assert(pct >= last);
    last = pct;
  }
}

void test_fuel_gauge__charge_complete_is_full(void) {
  Cell c = {.soc = 0.9, .t = 25.0};
  struct pbl_fuel_gauge_meas meas = prv_meas(&c, 0.0);

  pbl_fuel_gauge_init(&s_fg, &s_config, &meas, NULL);

  assert_near(pbl_fuel_gauge_update(&s_fg, &meas, 60.0f, PBL_FUEL_GAUGE_CHARGE_COMPLETE), 100.0,
              0.0);
  assert_near(pbl_fuel_gauge_soc_get(&s_fg), 1.0, 0.0);
}

void test_fuel_gauge__ttf_predicts_cc_cv_charge(void) {
  Cell c = {.soc = 0.2, .t = 25.0};
  struct pbl_fuel_gauge_meas meas = prv_meas(&c, 0.0);
  uint32_t ttf;
  double t;

  pbl_fuel_gauge_init(&s_fg, &s_config, &meas, NULL);
  pbl_fuel_gauge_update(&s_fg, &meas, 0.0f, PBL_FUEL_GAUGE_CHARGING_CC);
  cl_assert(!pbl_fuel_gauge_ttf_get(&s_fg, &ttf));

  t = prv_charge(&c, &ttf);

  assert_near(ttf, t - 900.0, 0.1 * t);
}

static double prv_constant_load(void) {
  return 0.002;
}

void test_fuel_gauge__tte_from_average_current(void) {
  Cell c = {.soc = 0.8, .t = 25.0};
  struct pbl_fuel_gauge_meas meas = prv_meas(&c, 0.002);
  uint32_t tte;

  pbl_fuel_gauge_init(&s_fg, &s_config, &meas, NULL);
  pbl_fuel_gauge_update(&s_fg, &meas, 0.0f, PBL_FUEL_GAUGE_DISCHARGING);

  for (int m = 0; m < 5; m++) {
    prv_run_minute(&c, prv_constant_load, 1.0, 0.0, PBL_FUEL_GAUGE_DISCHARGING);
  }
  cl_assert(!pbl_fuel_gauge_tte_get(&s_fg, &tte));

  for (int m = 0; m < 60; m++) {
    prv_run_minute(&c, prv_constant_load, 1.0, 0.0, PBL_FUEL_GAUGE_DISCHARGING);
  }
  cl_assert(pbl_fuel_gauge_tte_get(&s_fg, &tte));

  assert_near(tte, c.soc * Q_AH * 3600.0 / 0.002, 0.02 * tte);
}

void test_fuel_gauge__restores_state(void) {
  Cell c = {.soc = 0.6, .t = 25.0};
  struct pbl_fuel_gauge_meas meas = prv_meas(&c, 0.001);
  struct pbl_fuel_gauge_state state;

  pbl_fuel_gauge_init(&s_fg, &s_config, &meas, NULL);
  for (int m = 0; m < 60; m++) {
    prv_run_minute(&c, prv_bursty_load, 1.0, 0.0, PBL_FUEL_GAUGE_DISCHARGING);
  }
  pbl_fuel_gauge_state_get(&s_fg, &state);

  meas = prv_meas(&c, 0.001);
  cl_assert_equal_i(pbl_fuel_gauge_init(&s_fg, &s_config, &meas, &state), 1);
  assert_near(pbl_fuel_gauge_soc_get(&s_fg), state.soc, 0.0);
}

void test_fuel_gauge__discards_state_of_another_battery(void) {
  Cell c = {.soc = 0.9, .t = 25.0};
  struct pbl_fuel_gauge_meas meas = prv_meas(&c, 0.0);
  struct pbl_fuel_gauge_state state;

  pbl_fuel_gauge_init(&s_fg, &s_config, &meas, NULL);
  pbl_fuel_gauge_state_get(&s_fg, &state);

  c.soc = 0.3;
  meas = prv_meas(&c, 0.0);
  cl_assert_equal_i(pbl_fuel_gauge_init(&s_fg, &s_config, &meas, &state), 0);
  assert_near(pbl_fuel_gauge_soc_get(&s_fg), 0.3, 0.002);
}

void test_fuel_gauge__discards_state_of_another_model(void) {
  Cell c = {.soc = 0.5, .t = 25.0};
  struct pbl_fuel_gauge_meas meas = prv_meas(&c, 0.0);
  struct pbl_fuel_gauge_model model = s_model;
  struct pbl_fuel_gauge_config config = s_config;
  struct pbl_fuel_gauge_state state;

  pbl_fuel_gauge_init(&s_fg, &s_config, &meas, NULL);
  pbl_fuel_gauge_state_get(&s_fg, &state);

  model.r0 = 0.5f;
  config.model = &model;
  cl_assert_equal_i(pbl_fuel_gauge_init(&s_fg, &config, &meas, &state), 0);
}
