/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "drivers/mic.h"
#include "pbl/services/new_timer/new_timer.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
  TimerID timer;
  MicDataHandlerCB data_handler;
  void *handler_context;
  int16_t *audio_buffer;
  size_t audio_buffer_len;
  uint32_t period_ms;
  bool is_initialized;
  bool is_running;
} MicDeviceState;

typedef const struct MicDevice {
  MicDeviceState *state;
  uint32_t channels;
} MicDevice;
