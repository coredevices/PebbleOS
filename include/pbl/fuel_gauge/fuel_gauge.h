/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

//! Battery state-of-charge estimation for a single Li-ion/Li-po cell.
//!
//! The library has no OS dependencies. The estimator is selected at build
//! time; its header defines struct pbl_fuel_gauge_estimator (its parameters)
//! and struct pbl_fuel_gauge_estimator_state. See
//! docs/libraries/fuel_gauge/index.md.
//!
//! Units are SI throughout: V, A, s, degrees Celsius, Ah. Current is positive
//! when the battery discharges.

#if defined(CONFIG_FUEL_GAUGE_THEVENIN)
#include <pbl/fuel_gauge/thevenin.h>
#else
#error "No fuel gauge estimator selected"
#endif

//! A point of the open-circuit voltage curve.
struct pbl_fuel_gauge_ocv_point {
  //! State of charge, 0 (empty) to 1 (full).
  float soc;
  //! Open-circuit voltage at rest and 25 C [V].
  float v;
};

//! The cell, as its description gives it.
struct pbl_fuel_gauge_cell {
  //! Charge between the full and the empty point at 25 C [Ah].
  float capacity_ah;
  //! Strictly increasing in both SOC and voltage, from SOC 0 to SOC 1.
  const struct pbl_fuel_gauge_ocv_point *ocv;
  uint8_t ocv_count;
  //! Series resistance at 25 C [ohm].
  float r0;
};

struct pbl_fuel_gauge_config {
  const struct pbl_fuel_gauge_cell *cell;
  const struct pbl_fuel_gauge_estimator *estimator;
  //! Charger termination voltage [V], used for time-to-full.
  float term_voltage;
  //! Charger termination current [A], used for time-to-full.
  float term_current;
};

//! A battery measurement. Voltage and current should be sampled together.
struct pbl_fuel_gauge_meas {
  //! Terminal voltage [V].
  float v;
  //! Current, positive when discharging [A].
  float i;
  //! Cell temperature [C].
  float t;
};

enum pbl_fuel_gauge_charge_state {
  PBL_FUEL_GAUGE_DISCHARGING,
  //! Constant current, including trickle charge.
  PBL_FUEL_GAUGE_CHARGING_CC,
  PBL_FUEL_GAUGE_CHARGING_CV,
  //! The charger terminated: the battery is full.
  PBL_FUEL_GAUGE_CHARGE_COMPLETE,
};

//! State that survives a reboot, see pbl_fuel_gauge_state_get().
struct pbl_fuel_gauge_state {
  //! Checksum of the cell, the estimator and the state layout the state was
  //! produced with.
  uint32_t model_id;
  struct pbl_fuel_gauge_estimator_state estimator;
  //! Reported SOC, see pbl_fuel_gauge_update().
  float soc_reported;
  //! Exponentially weighted charge and time for the average current.
  float avg_charge;
  float avg_time;
};

//! Fuel gauge instance, treat as opaque.
struct pbl_fuel_gauge {
  const struct pbl_fuel_gauge_config *config;
  struct pbl_fuel_gauge_state state;
  enum pbl_fuel_gauge_charge_state charge_state;
  struct pbl_fuel_gauge_meas last;
};

//! Initializes the fuel gauge. Without @p saved, or when @p saved belongs to
//! another cell or estimator, or disagrees with the voltage by too much
//! (another battery, or a long time without power), the SOC starts from the
//! open-circuit voltage implied by @p meas.
//! @return 1 when @p saved was used, 0 when starting from the voltage,
//! -EINVAL for an invalid configuration.
int pbl_fuel_gauge_init(struct pbl_fuel_gauge *fg, const struct pbl_fuel_gauge_config *config,
                        const struct pbl_fuel_gauge_meas *meas,
                        const struct pbl_fuel_gauge_state *saved);

//! Processes a measurement taken @p dt seconds after the previous one.
//! @return the reported SOC [%]. Once the estimate has settled, it does not
//! rise while discharging nor drop while charging, so corrections from the
//! voltage are only shown when they move in the direction the battery is
//! going.
float pbl_fuel_gauge_update(struct pbl_fuel_gauge *fg, const struct pbl_fuel_gauge_meas *meas,
                            float dt, enum pbl_fuel_gauge_charge_state charge_state);

//! @return the estimated SOC, 0 to 1, without the reporting constraints.
float pbl_fuel_gauge_soc_get(const struct pbl_fuel_gauge *fg);

//! @param fg fuel gauge.
//! @param[out] seconds time until empty at the average discharge current.
//! @return 0 on success, -ENODATA while charging or while the average is not
//! settled yet.
int pbl_fuel_gauge_tte_get(const struct pbl_fuel_gauge *fg, uint32_t *seconds);

//! @param fg fuel gauge.
//! @param[out] seconds time until the charger terminates.
//! @return 0 on success, -ENODATA when not charging or not settled yet.
int pbl_fuel_gauge_ttf_get(const struct pbl_fuel_gauge *fg, uint32_t *seconds);

void pbl_fuel_gauge_state_get(const struct pbl_fuel_gauge *fg, struct pbl_fuel_gauge_state *state);
