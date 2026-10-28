/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/speaker.h>

/**
 * @defgroup drivers_speaker_qemu QEMU audio
 * @ingroup drivers_speaker
 * @brief @ref drivers_speaker implementation for the QEMU audio device.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
typedef struct AudioState {
  AudioTransCB trans_cb;
  bool callback_pending;
} AudioDeviceState;
/** @endcond */

/** @brief QEMU audio device. */
struct AudioDevice {
  /** Driver runtime state. */
  AudioDeviceState *state;
  /** Base address of the device registers. */
  uint32_t base_addr;
  /** Device interrupt number. */
  int irqn;
};

/**
 * @brief Device interrupt handler.
 *
 * @param dev Audio device.
 */
void qemu_audio_irq_handler(AudioDevice *dev);

/** @} */
