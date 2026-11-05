/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pbl/drivers/mic.h>
#include <pbl/kernel/mutex.h>
#include <pbl/services/new_timer/new_timer.h>

/**
 * @defgroup drivers_mic_qemu QEMU microphone
 * @ingroup drivers_mic
 * @brief @ref drivers_mic implementation for QEMU, delivering host audio or silence on a timer.
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
  /** Protects the host audio ring, filled by the QEMU channel and drained by the timer. */
  struct pbl_mutex lock;
  /** Host audio ring, allocated while running. */
  int16_t *ring;
  size_t ring_read;
  size_t ring_count;
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

/**
 * @brief Handle a `QemuProtocol_MicAudio` message from the host.
 *
 * Queues the samples for the running capture, dropping the oldest once the queue is full. Ignored
 * while the microphone is stopped.
 *
 * @param data 16 kHz mono signed 16-bit little-endian PCM.
 * @param len Length of @p data in bytes.
 */
void qemu_mic_msg_callback(const uint8_t *data, uint32_t len);

/** @} */
