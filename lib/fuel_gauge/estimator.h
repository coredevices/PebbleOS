/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <pbl/fuel_gauge/fuel_gauge.h>

float pbl_fuel_gauge__clampf(float x, float lo, float hi);

//! OCV at @p soc, with its slope dOCV/dSOC in @p slope.
float pbl_fuel_gauge__ocv(const struct pbl_fuel_gauge_cell *cell, float soc, float *slope);
float pbl_fuel_gauge__soc_from_ocv(const struct pbl_fuel_gauge_cell *cell, float v);

// Implemented by the selected estimator.

bool pbl_fuel_gauge__est_valid(const struct pbl_fuel_gauge_config *config);
//! Continues hash_update() @p h with the estimator parameters.
uint32_t pbl_fuel_gauge__est_hash(const struct pbl_fuel_gauge_config *config, uint32_t h);

//! Starts from the open-circuit voltage implied by @p meas.
void pbl_fuel_gauge__est_init(const struct pbl_fuel_gauge_config *config,
                              struct pbl_fuel_gauge_estimator_state *st,
                              const struct pbl_fuel_gauge_meas *meas);

bool pbl_fuel_gauge__est_state_valid(const struct pbl_fuel_gauge_estimator_state *st);

//! Open-circuit voltage implied by @p meas with the load of @p st removed.
float pbl_fuel_gauge__est_rest_voltage(const struct pbl_fuel_gauge_config *config,
                                       const struct pbl_fuel_gauge_estimator_state *st,
                                       const struct pbl_fuel_gauge_meas *meas);

//! Processes a measurement taken @p dt seconds after the previous one.
void pbl_fuel_gauge__est_update(const struct pbl_fuel_gauge_config *config,
                                struct pbl_fuel_gauge_estimator_state *st,
                                const struct pbl_fuel_gauge_meas *meas, float dt);

//! The charger terminated: the cell is full.
void pbl_fuel_gauge__est_full(struct pbl_fuel_gauge_estimator_state *st);

float pbl_fuel_gauge__est_soc(const struct pbl_fuel_gauge_estimator_state *st);
float pbl_fuel_gauge__est_soc_var(const struct pbl_fuel_gauge_estimator_state *st);

//! Resistance a constant current sees at temperature @p t [ohm].
float pbl_fuel_gauge__est_dc_resistance(const struct pbl_fuel_gauge_config *config, float t);
