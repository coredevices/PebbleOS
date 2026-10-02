/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include "pbl/services/new_timer/new_timer.h"
#include <stdbool.h>
#include <stdint.h>

//! @addtogroup Foundation
//! @{
//!   @addtogroup Battery Battery
//!   \brief Functions related to getting the battery status
//!
//! This module contains the functions necessary to find the current charge status.
//! @note Battery charge state is a complex topic; our modelling of the charge
//! that is exposed by these functions represents a very simplified model based
//! mostly on empirically derived charge and discharge voltage curves.  As
//! such, you should expect that the output will not have a high degree of
//! accuracy.
//!   @{

//! Structure for retrieval of the battery charge state
typedef struct {
  //! A percentage (0-100) of how full the battery is
  uint8_t charge_percent;
  //! True if the battery is currently being charged. False if not.
  bool is_charging;
  //! True if the charger cable is connected. False if not.
  bool is_plugged;
} BatteryChargeState;

//! @internal
//! Structure for retrieval of the exact battery charge state
typedef struct {
  //! The battery's percentage as a ratio32
  uint32_t charge_percent;
  //! The battery percentage 0-100
  uint8_t pct;
  //! WARNING: This maps to @see battery_charge_controller_thinks_we_are_charging as opposed to
  //! the user-facing definition of whether we're charging (100% battery).
  bool is_charging;
  bool is_plugged;
} PreciseBatteryChargeState;

//! Function to get the current battery charge state
//! @returns a \ref BatteryChargeState struct with the current charge state
BatteryChargeState battery_get_charge_state(void);

//!   @}
//! @}

/**
 * @defgroup services_battery Battery
 * @ingroup services
 * @brief Battery state of charge, charger state and power state handling.
 *
 * The battery state module samples the battery through the driver, filters the readings and
 * emits a @c PEBBLE_BATTERY_STATE_CHANGE_EVENT when the state changes; it is implemented either
 * on a voltage curve or on the nRF fuel gauge, selected at build time. The battery monitor
 * reacts to those changes with low power mode and standby.
 * @{
 */

/**
 * @brief Sample the battery now instead of waiting for the next periodic update.
 *
 * The update is scheduled from a timer, so the new state is not available on return.
 */
void battery_state_force_update(void);

/** @brief Initialize battery state tracking and take the first sample. */
void battery_state_init(void);

/**
 * @brief Handle a charger connection change.
 *
 * Schedules an update after a short delay, to let the readings settle and debounce reconnection.
 *
 * @param is_connected true if the charger was connected.
 */
void battery_state_handle_connection_event(bool is_connected);

/**
 * @brief Reset the voltage filter to the current reading.
 *
 * Voltage curve backend only.
 */
void battery_state_reset_filter(void);

/**
 * @brief Get the last recorded battery voltage.
 *
 * @return Voltage in mV.
 */
uint16_t battery_state_get_voltage(void);

/**
 * @brief Get the last recorded battery temperature.
 *
 * @return Temperature in thousandths of a degree Celsius.
 */
int32_t battery_state_get_temperature(void);

/**
 * @brief Get the periodic sampling timer, for unit tests.
 *
 * @return Timer id.
 */
TimerID battery_state_get_periodic_timer_id(void);

/** @} */
