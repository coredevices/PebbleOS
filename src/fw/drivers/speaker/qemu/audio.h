/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include "drivers/audio.h"

typedef struct AudioState {
  AudioTransCB trans_cb;
} AudioDeviceState;

struct AudioDevice {
  AudioDeviceState *state;
  uint32_t base_addr;
  int irqn;
};

void qemu_audio_irq_handler(AudioDevice *dev);
