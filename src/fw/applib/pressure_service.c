/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pressure_service.h"
#include "pressure_service_private.h"

#include "drivers/pressure.h"
#include "process_state/app_state/app_state.h"
#include "syscall/syscall_internal.h"
#include "system/logging.h"

#include <stddef.h>

#define STANDARD_SEA_LEVEL_PA 101325

// Timer intervals in ms for each ODR preset
static uint32_t prv_odr_to_interval_ms(PressureODR odr) {
  switch (odr) {
    case PRESSURE_ODR_1HZ:  return 1000;
    case PRESSURE_ODR_5HZ:  return 200;
    case PRESSURE_ODR_10HZ: return 100;
    case PRESSURE_ODR_25HZ: return 40;
    case PRESSURE_ODR_50HZ: return 20;
    default:                return 1000;
  }
}

static PressureServiceState *prv_get_state(void) {
  return app_state_get_pressure_state();
}

static void prv_compute_altitude(PressureServiceState *state, PressureData *data) {
  int32_t ref = state->ref_pressure_pa;
  if (ref <= 0) {
    ref = STANDARD_SEA_LEVEL_PA;
  }

  if (state->use_full_formula) {
    data->altitude_cm = pressure_get_altitude_full_cm(data->pressure_pa, ref);
  } else {
    data->altitude_cm = pressure_get_altitude_cm(data->pressure_pa, ref);
  }
}

// Syscall wrapper for hardware access from the timer callback, which runs in
// unprivileged app context.
DEFINE_SYSCALL(bool, sys_pressure_read_and_compute, PressureData *data) {
  PressureServiceState *state = prv_get_state();

  if (!pressure_read(&data->pressure_pa, &data->temperature_centideg)) {
    return false;
  }

  prv_compute_altitude(state, data);
  return true;
}

static void prv_timer_callback(void *context) {
  PressureServiceState *state = prv_get_state();
  if (!state->handler) {
    return;
  }

  PressureData data = { 0 };
  if (!sys_pressure_read_and_compute(&data)) {
    return;
  }

  state->handler(&data);
}

// All exported functions use DEFINE_SYSCALL to escalate privileges, since they
// are called from unprivileged app code via the SDK jump table but need to
// access hardware drivers (pressure_read, pressure_set_odr).

DEFINE_SYSCALL(void, pressure_service_subscribe, PressureDataHandler handler, PressureODR odr) {
  PressureServiceState *state = prv_get_state();

  // Clean up any existing subscription
  if (state->poll_timer) {
    app_timer_cancel(state->poll_timer);
    state->poll_timer = NULL;
  }

  state->handler = handler;
  state->odr = odr;

  if (!handler) {
    return;
  }

  // Configure hardware for the requested rate
  pressure_set_odr(odr);

  uint32_t interval_ms = prv_odr_to_interval_ms(odr);
  state->poll_timer = app_timer_register_repeatable(interval_ms,
                                                     prv_timer_callback,
                                                     NULL,
                                                     true /* repeating */);
}

DEFINE_SYSCALL(void, pressure_service_unsubscribe, void) {
  PressureServiceState *state = prv_get_state();

  if (state->poll_timer) {
    app_timer_cancel(state->poll_timer);
    state->poll_timer = NULL;
  }
  state->handler = NULL;

  // Drop hardware back to low-power mode
  pressure_set_odr(PRESSURE_ODR_1HZ);
}

DEFINE_SYSCALL(bool, pressure_service_set_data_rate, PressureODR odr) {
  PressureServiceState *state = prv_get_state();

  if (!state->handler || !state->poll_timer) {
    return false;
  }

  if (!pressure_set_odr(odr)) {
    return false;
  }

  state->odr = odr;
  uint32_t interval_ms = prv_odr_to_interval_ms(odr);
  app_timer_reschedule(state->poll_timer, interval_ms);
  return true;
}

DEFINE_SYSCALL(bool, pressure_service_set_reference, void) {
  PressureServiceState *state = prv_get_state();

  int32_t pressure_pa;
  if (!pressure_read(&pressure_pa, NULL)) {
    return false;
  }

  state->ref_pressure_pa = pressure_pa;
  return true;
}

DEFINE_SYSCALL(void, pressure_service_set_reference_pressure, int32_t ref_pressure_pa) {
  PressureServiceState *state = prv_get_state();
  state->ref_pressure_pa = ref_pressure_pa;
}

DEFINE_SYSCALL(void, pressure_service_use_full_formula, bool enable) {
  PressureServiceState *state = prv_get_state();
  state->use_full_formula = enable;
}

DEFINE_SYSCALL(bool, pressure_service_peek, PressureData *data) {
  if (!data) {
    return false;
  }

  PressureServiceState *state = prv_get_state();

  if (!pressure_read(&data->pressure_pa, &data->temperature_centideg)) {
    return false;
  }

  prv_compute_altitude(state, data);
  return true;
}

DEFINE_SYSCALL(int32_t, pressure_service_get_altitude_cm, int32_t pressure_pa,
               int32_t ref_pressure_pa) {
  PressureServiceState *state = prv_get_state();
  if (state->use_full_formula) {
    return pressure_get_altitude_full_cm(pressure_pa, ref_pressure_pa);
  } else {
    return pressure_get_altitude_cm(pressure_pa, ref_pressure_pa);
  }
}

void pressure_service_state_init(PressureServiceState *state) {
  *state = (PressureServiceState) {
    .handler = NULL,
    .poll_timer = NULL,
    .ref_pressure_pa = 0,
    .odr = PRESSURE_ODR_1HZ,
    .use_full_formula = false,
  };
}

void pressure_service_state_deinit(PressureServiceState *state) {
  if (state->poll_timer) {
    app_timer_cancel(state->poll_timer);
    state->poll_timer = NULL;
  }
  state->handler = NULL;
}
