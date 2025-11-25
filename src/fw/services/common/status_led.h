/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

//! Different states supported by the status LED.
typedef enum {
  StatusLedState_Off,
  StatusLedState_Charging,
  StatusLedState_FullyCharged,

  StatusLedStateCount
} StatusLedState;

//! Set the status LED to a new state. Note that this function is a no-op on boards that don't
//! have a status LED.
void status_led_set(StatusLedState state);
