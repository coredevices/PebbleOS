/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/audio.h>
#include <pbl/drivers/clk.h>
#include <pbl/drivers/dma.h>
#include <pbl/drivers/pinctrl.h>
#include <pbl/drivers/reset.h>
#include <pbl/kernel/irq.h>

/**
 * @defgroup drivers_audio_sf32lb52 SF32LB52 audio
 * @ingroup drivers_audio
 * @brief @ref drivers_audio DAIs of the SF32LB52: the audio codec DAC and the PDM interface.
 *
 * The audio codec (AUDCODEC) plays mono through its DAC, with its analog bias, PLL and output
 * stage, at 8 to 48 kHz; its digital gain runs from -36 to 0 dB. The PDM interface records one
 * or two digital microphones at 8 to 48 kHz, mono taking the left channel; its gain runs from
 * -15 to +45 dB. Both stream through a cyclic channel of the SF32LB52 DMA controller, so rings have
 * two periods, of a multiple of 4 bytes.
 * The CPU does not enter deep sleep while they stream.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
struct pbl_audio_sf32lb52_audcodec_state {
  uint32_t vco[2];
  uint8_t pll;
  uint32_t ring_bytes;
};
/** @endcond */

/** @brief Audio codec DAC. */
struct pbl_audio_sf32lb52_audcodec {
  /** DAI. */
  struct pbl_audio_dai dai;
  /** Register base address. */
  uintptr_t base;
  /** Clock. */
  struct pbl_clk clk;
  /** DAC DMA channel. */
  const struct pbl_dma_chan *dma;
  /** Driver runtime state. */
  struct pbl_audio_sf32lb52_audcodec_state *state;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_audio_dai_ops pbl_audio_sf32lb52_audcodec_ops;
/** @endcond */

/**
 * @brief Define the audio codec DAC, a playback DAI.
 *
 * @param sym Symbol of the DAI.
 * @param _name Device name.
 * @param _base Register base address.
 * @param _clk Clock, from PBL_CLK().
 * @param _dma DAC DMA channel.
 */
#define PBL_AUDIO_SF32LB52_AUDCODEC_DEFINE(sym, _name, _base, _clk, _dma)                       \
  PBL_AUDIO_DAI_STATE_DEFINE(sym);                                                              \
  static struct pbl_audio_sf32lb52_audcodec_state sym##_audcodec_state;                         \
  PBL_DEVICE_INSTANCE(sym, struct pbl_audio_sf32lb52_audcodec, struct pbl_audio_dai, dai) {     \
    .dai = PBL_AUDIO_DAI_INIT(sym, _name, PBL_AUDIO_PLAYBACK, &pbl_audio_sf32lb52_audcodec_ops, \
                              PBL_DEVICE_DEPS(&(_dma)->ctrl->dev)),                             \
    .base = (_base), .clk = _clk, .dma = (_dma), .state = &sym##_audcodec_state,                \
  }

/** @cond INTERNAL_HIDDEN */
struct pbl_audio_sf32lb52_pdm_state {
  uint32_t ring_bytes;
};
/** @endcond */

/** @brief PDM interface. */
struct pbl_audio_sf32lb52_pdm {
  /** DAI. */
  struct pbl_audio_dai dai;
  /** Register base address. */
  uintptr_t base;
  /** PDM interrupt, reporting FIFO overflows. */
  pbl_irq_t irq;
  /** Clock. */
  struct pbl_clk clk;
  /** Reset. */
  struct pbl_reset rst;
  /** DMA channel. */
  const struct pbl_dma_chan *dma;
  /** Clock and data pins. */
  const struct pbl_pinctrl *pinctrl;
  /** Driver runtime state. */
  struct pbl_audio_sf32lb52_pdm_state *state;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_audio_dai_ops pbl_audio_sf32lb52_pdm_ops;
/** @endcond */

/**
 * @brief Define a PDM interface, a capture DAI.
 *
 * @param sym Symbol of the DAI.
 * @param _name Device name.
 * @param _base Register base address.
 * @param _irq Interrupt line.
 * @param _clk Clock, from PBL_CLK().
 * @param _rst Reset, from PBL_RESET().
 * @param _dma DMA channel.
 * @param _pinctrl Default state of the clock and data pins.
 */
#define PBL_AUDIO_SF32LB52_PDM_DEFINE(sym, _name, _base, _irq, _clk, _rst, _dma, _pinctrl) \
  PBL_AUDIO_DAI_STATE_DEFINE(sym);                                                         \
  static struct pbl_audio_sf32lb52_pdm_state sym##_pdm_state;                              \
  static const struct pbl_pinctrl sym##_pdm_pins = PBL_PINCTRL(_pinctrl, nullptr, 0);      \
  PBL_DEVICE_INSTANCE(sym, struct pbl_audio_sf32lb52_pdm, struct pbl_audio_dai, dai){      \
    .dai = PBL_AUDIO_DAI_INIT(sym, _name, PBL_AUDIO_CAPTURE, &pbl_audio_sf32lb52_pdm_ops,  \
                              PBL_DEVICE_DEPS(&(_dma)->ctrl->dev)),                        \
    .base = (_base),                                                                       \
    .irq = (_irq),                                                                         \
    .clk = _clk,                                                                           \
    .rst = _rst,                                                                           \
    .dma = (_dma),                                                                         \
    .pinctrl = &sym##_pdm_pins,                                                            \
    .state = &sym##_pdm_state,                                                             \
  };                                                                                       \
  PBL_IRQ_CONNECT_NUM(_irq, CONFIG_AUDIO_SF32LB52_PDM_IRQ_PRIO,                            \
                      pbl_audio_sf32lb52_pdm_irq_handler, &sym##_inst, 0)

/**
 * @brief PDM interrupt handler, connected by PBL_AUDIO_SF32LB52_PDM_DEFINE().
 *
 * @param pdm PDM interface.
 */
void pbl_audio_sf32lb52_pdm_irq_handler(const struct pbl_audio_sf32lb52_pdm *pdm);

/** @} */
