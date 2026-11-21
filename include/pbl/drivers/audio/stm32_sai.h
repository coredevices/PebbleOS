/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/audio.h>
#include <pbl/drivers/clk.h>
#include <pbl/drivers/dma.h>
#include <pbl/drivers/pinctrl.h>

/**
 * @defgroup drivers_audio_stm32_sai STM32 SAI
 * @ingroup drivers_audio
 * @brief Playback DAI on a sub-block of an STM32 serial audio interface.
 *
 * The sub-block is the master of an I2S link: two 32-bit slots of 16-bit samples, a bit clock
 * of 64 times the rate, and a master clock of 256 times the rate for the codec, divided from its
 * kernel clock. A mono stream is sent on both slots.
 * @{
 */

/** @brief STM32 SAI sub-block. */
struct pbl_audio_stm32_sai {
  /** DAI. */
  struct pbl_audio_dai dai;
  /** Registers of the sub-block. */
  uintptr_t base;
  /** Kernel clock. */
  struct pbl_clk clk;
  /** DMA channel. */
  const struct pbl_dma_chan *dma;
  /** Pins. */
  const struct pbl_pinctrl *pinctrl;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_audio_dai_ops pbl_audio_stm32_sai_ops;
/** @endcond */

/**
 * @brief Define a SAI sub-block as a playback DAI.
 *
 * @param sym Symbol of the DAI.
 * @param _name Device name.
 * @param _base Registers of the sub-block.
 * @param _clk Kernel clock, from PBL_CLK().
 * @param _dma DMA channel.
 * @param _pinctrl Default state of the pins.
 */
#define PBL_AUDIO_STM32_SAI_DEFINE(sym, _name, _base, _clk, _dma, _pinctrl)                       \
  PBL_AUDIO_DAI_STATE_DEFINE(sym);                                                                \
  static const struct pbl_pinctrl sym##_sai_pins = PBL_PINCTRL(_pinctrl, nullptr, 0);             \
  PBL_DEVICE_INSTANCE(sym, struct pbl_audio_stm32_sai, struct pbl_audio_dai, dai) {               \
    .dai = PBL_AUDIO_DAI_INIT(sym, _name, nullptr, &pbl_audio_stm32_sai_ops, PBL_AUDIO_PLAYBACK), \
    .base = (_base), .clk = _clk, .dma = (_dma), .pinctrl = &sym##_sai_pins,                      \
  }

/** @} */
