/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/audio.h>
#include <pbl/drivers/clk.h>
#include <pbl/drivers/dma.h>

/**
 * @defgroup drivers_audio_sf32lb SF32LB audio codec
 * @ingroup drivers_audio
 * @brief @ref drivers_audio DAI of the SF32LB audio codec.
 *
 * The audio codec (AUDCODEC) plays mono through its DAC, with its analog bias, PLL and output
 * stage, at 8 to 48 kHz; its digital gain runs from -36 to 0 dB. It streams through a cyclic DMA
 * channel, so rings have two periods, of a multiple of 4 bytes.
 * The CPU does not enter deep sleep while it streams.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
struct pbl_audio_sf32lb_audcodec_state {
  uint32_t vco[2];
  uint8_t pll;
  uint32_t ring_bytes;
};
/** @endcond */

/** @brief Audio codec DAC. */
struct pbl_audio_sf32lb_audcodec {
  /** DAI. */
  struct pbl_audio_dai dai;
  /** Register base address. */
  uintptr_t base;
  /** Clock. */
  struct pbl_clk clk;
  /** DAC DMA channel. */
  const struct pbl_dma_chan *dma;
  /** PMUC registers, holding the HXT48 buffer to the codec. */
  uintptr_t pmuc;
  /** Driver runtime state. */
  struct pbl_audio_sf32lb_audcodec_state *state;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_audio_dai_ops pbl_audio_sf32lb_audcodec_ops;
/** @endcond */

/**
 * @brief Define the audio codec DAC, a playback DAI.
 *
 * @param sym Symbol of the DAI.
 * @param _name Device name.
 * @param _base Register base address.
 * @param _clk Clock, from PBL_CLK().
 * @param _dma DAC DMA channel.
 * @param _pmuc PMUC registers.
 */
#define PBL_AUDIO_SF32LB_AUDCODEC_DEFINE(sym, _name, _base, _clk, _dma, _pmuc)                    \
  PBL_AUDIO_DAI_STATE_DEFINE(sym);                                                                \
  static struct pbl_audio_sf32lb_audcodec_state sym##_audcodec_state;                             \
  PBL_DEVICE_INSTANCE(sym, struct pbl_audio_sf32lb_audcodec, struct pbl_audio_dai, dai) {         \
    .dai = PBL_AUDIO_DAI_INIT(sym, _name, nullptr, &pbl_audio_sf32lb_audcodec_ops,                \
                              PBL_AUDIO_PLAYBACK),                                                \
    .base = (_base), .clk = _clk, .dma = (_dma), .pmuc = (_pmuc), .state = &sym##_audcodec_state, \
  }

/** @} */
