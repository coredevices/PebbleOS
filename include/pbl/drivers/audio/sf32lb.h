/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/audio.h>
#include <pbl/drivers/clk.h>
#include <pbl/drivers/dma.h>
#include <pbl/drivers/pinctrl.h>
#include <pbl/kernel/mutex.h>

/**
 * @defgroup drivers_audio_sf32lb SF32LB audio codec
 * @ingroup drivers_audio
 * @brief @ref drivers_audio DAIs of the SF32LB audio codec: its DAC and its ADC.
 *
 * The audio codec (AUDCODEC) plays mono through its DAC, with its analog bias, PLL and output
 * stage, at 8 to 48 kHz; its digital gain runs from -36 to 0 dB. Its ADC records one or two analog
 * microphones, powered from its microphone bias, at the same rates: mono takes the first ADC,
 * stereo both, the second with a DMA channel of its own. The ADC gain runs from -60 to +30 dB.
 * Both stream through cyclic DMA channels, so rings have two periods, of a multiple of 4 bytes per
 * channel. The CPU does not enter deep sleep while they stream.
 *
 * In the devicetree, the codec takes one cell in @c sound-dai: 0 for the DAC, 1 for the ADC.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
struct pbl_audio_sf32lb_audcodec_state {
  struct pbl_mutex lock;
  bool ready;
  uint32_t vco[2];
  uint8_t pll;
  uint8_t running;
  uint32_t dac_ring_bytes;
  uint32_t adc_ring_bytes;
  uint8_t adc_channels;
  int16_t *adc_buf;
  uint32_t adc_done[2];
  uint32_t adc_copied;
};
/** @endcond */

/** @brief Audio codec. */
struct pbl_audio_sf32lb_audcodec {
  /** DAIs, by direction: the DAC, then the ADC. */
  struct pbl_audio_dai dai[2];
  /** Register base address. */
  uintptr_t base;
  /** Clock. */
  struct pbl_clk clk;
  /** DAC DMA channel. */
  const struct pbl_dma_chan *dac_dma;
  /** DMA channels of the first and second ADC, NULL for an ADC not recorded. */
  const struct pbl_dma_chan *adc_dma[2];
  /** Pins of the analog inputs and the microphone bias. */
  struct pbl_pinctrl pinctrl;
  /** PMUC registers, holding the HXT48 buffer to the codec. */
  uintptr_t pmuc;
  /** Driver runtime state. */
  struct pbl_audio_sf32lb_audcodec_state *state;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_audio_dai_ops pbl_audio_sf32lb_audcodec_dac_ops;
extern const struct pbl_audio_dai_ops pbl_audio_sf32lb_audcodec_adc_ops;
/** @endcond */

/**
 * @brief Get a DAI of an audio codec, as its @c sound-dai cell selects it.
 *
 * @param dac DAC of the codec, the symbol PBL_AUDIO_SF32LB_AUDCODEC_DEFINE() defines.
 * @param idx 0 for the DAC, 1 for the ADC.
 */
#define PBL_AUDIO_SF32LB_AUDCODEC_DAI(dac, idx) \
  (&((const struct pbl_audio_sf32lb_audcodec *)(dac))->dai[(idx)])

/**
 * @brief Define an audio codec: its DAC, a playback DAI, and its ADC, a capture DAI.
 *
 * @param sym Symbol of the DAC; the ADC is reached with PBL_AUDIO_SF32LB_AUDCODEC_DAI().
 * @param _name Device name.
 * @param _base Register base address.
 * @param _clk Clock, from PBL_CLK().
 * @param _dac_dma DAC DMA channel.
 * @param _adc1_dma DMA channel of the first ADC, or NULL.
 * @param _adc2_dma DMA channel of the second ADC, or NULL.
 * @param _pinctrl Default state of the analog pins, or NULL.
 * @param _pmuc PMUC registers.
 */
#define PBL_AUDIO_SF32LB_AUDCODEC_DEFINE(sym, _name, _base, _clk, _dac_dma, _adc1_dma, _adc2_dma, \
                                         _pinctrl, _pmuc)                                         \
  PBL_AUDIO_DAI_STATE_DEFINE(sym);                                                                \
  PBL_AUDIO_DAI_STATE_DEFINE(sym##_adc);                                                          \
  static struct pbl_audio_sf32lb_audcodec_state sym##_audcodec_state = {                          \
    .lock = PBL_MUTEX_INITIALIZER,                                                                \
  };                                                                                              \
  PBL_DEVICE_INSTANCE(sym, struct pbl_audio_sf32lb_audcodec, struct pbl_audio_dai,                \
                      dai[PBL_AUDIO_PLAYBACK]){                                                   \
    .dai =                                                                                        \
        {                                                                                         \
          [PBL_AUDIO_PLAYBACK] =                                                                  \
              PBL_AUDIO_DAI_INIT(sym, _name, nullptr, &pbl_audio_sf32lb_audcodec_dac_ops),        \
          [PBL_AUDIO_CAPTURE] = PBL_AUDIO_DAI_INIT(sym##_adc, _name "-adc", nullptr,              \
                                                   &pbl_audio_sf32lb_audcodec_adc_ops),           \
        },                                                                                        \
    .base = (_base),                                                                              \
    .clk = _clk,                                                                                  \
    .dac_dma = (_dac_dma),                                                                        \
    .adc_dma = {(_adc1_dma), (_adc2_dma)},                                                        \
    .pinctrl = PBL_PINCTRL(_pinctrl, nullptr, 0),                                                 \
    .pmuc = (_pmuc),                                                                              \
    .state = &sym##_audcodec_state,                                                               \
  };                                                                                              \
  PBL_DEVICE_REGISTER(sym##_adc, &sym##_inst.dai[PBL_AUDIO_CAPTURE].dev)

/** @} */
