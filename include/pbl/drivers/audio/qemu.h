/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pbl/drivers/audio.h>
#include <pbl/kernel/irq.h>

/**
 * @defgroup drivers_audio_qemu Emulated audio
 * @ingroup drivers_audio
 * @brief @ref drivers_audio devices of the emulators.
 *
 * The QEMU audio device plays mono through the host.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
struct pbl_audio_qemu_state {
  uint8_t idx;
  size_t offset;
};
/** @endcond */

/** @brief QEMU audio device. */
struct pbl_audio_qemu {
  /** DAI. */
  struct pbl_audio_dai dai;
  /** Base address of the registers. */
  uintptr_t base;
  /** Interrupt. */
  pbl_irq_t irq;
  /** Driver runtime state. */
  struct pbl_audio_qemu_state *state;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_audio_dai_ops pbl_audio_qemu_ops;
/** @endcond */

/**
 * @brief Define the QEMU audio device, a playback device.
 *
 * @param sym Symbol of the device.
 * @param _name Device name.
 * @param _base Base address of the registers.
 * @param _irq Interrupt line.
 */
#define PBL_AUDIO_QEMU_DEFINE(sym, _name, _base, _irq)                                       \
  PBL_AUDIO_DAI_STATE_DEFINE(sym);                                                           \
  static struct pbl_audio_qemu_state sym##_qemu_state;                                       \
  PBL_DEVICE_INSTANCE(sym, struct pbl_audio_qemu, struct pbl_audio_dai, dai){                \
    .dai = PBL_AUDIO_DAI_INIT(sym, _name, PBL_AUDIO_PLAYBACK, &pbl_audio_qemu_ops, nullptr), \
    .base = (_base),                                                                         \
    .irq = (_irq),                                                                           \
    .state = &sym##_qemu_state,                                                              \
  };                                                                                         \
  PBL_IRQ_CONNECT_NUM(_irq, CONFIG_AUDIO_QEMU_IRQ_PRIO, pbl_audio_qemu_irq_handler, &sym##_inst, 0)

/**
 * @brief QEMU audio device interrupt handler, connected by PBL_AUDIO_QEMU_DEFINE().
 *
 * @param dev Device.
 */
void pbl_audio_qemu_irq_handler(const struct pbl_audio_qemu *dev);

/** @} */
