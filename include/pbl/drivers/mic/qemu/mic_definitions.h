/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <pbl/drivers/mic.h>
#include <pbl/services/new_timer/new_timer.h>

/**
 * @defgroup drivers_mic_qemu QEMU microphone
 * @ingroup drivers_mic
 * @brief @ref drivers_mic implementation for QEMU, delivering silence on a timer.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
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
/** @endcond */

/** @brief QEMU microphone device. */
typedef const struct MicDevice {
  /** Driver runtime state. */
  MicDeviceState *state;
  /** Number of channels. */
  uint32_t channels;
} MicDevice;

/** @} */
