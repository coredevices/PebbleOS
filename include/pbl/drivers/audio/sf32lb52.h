/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/audio.h>
#include <pbl/drivers/dma.h>
#include <pbl/drivers/pinctrl.h>
#include <pbl/drivers/regulator.h>
#include <pbl/kernel/irq.h>

/**
 * @defgroup drivers_audio_sf32lb52 SF32LB52 audio
 * @ingroup drivers_audio
 * @brief @ref drivers_audio devices of the SF32LB52: the audio codec DAC and the PDM interface.
 *
 * The audio codec (AUDCODEC) plays mono through its DAC, with its analog bias, PLL and output
 * stage, at 8 to 48 kHz; its digital gain runs from -36 to 0 dB. The PDM interface records one
 * or two digital microphones at 8 to 48 kHz, mono taking the left channel; its gain runs from
 * -15 to +45 dB. Both stream through a cyclic DMA channel of @ref SF32LB52_DMAC1, so rings have
 * two periods, of a multiple of 4 bytes; the board connects and gives the channel interrupt.
 * The CPU does not enter deep sleep while they stream.
 *
 * @code{.c}
 * static const struct pbl_dma_chan s_dac_dma =
 *     PBL_DMA_CHAN(SF32LB52_DMAC1, 3, SF32LB52_DMA_REQ_AUDAC_CH0, SF32LB52_DMA_PL_HIGH);
 * PBL_AUDIO_SF32LB52_AUDCODEC_DEFINE(s_speaker, "speaker", &s_dac_dma, PBL_IRQN(DMAC1_CH4),
 *                                    &s_speaker_link);
 * PBL_IRQ_CONNECT(DMAC1_CH4, 5, pbl_dma_sf32lb52_irq_handler, &s_dac_dma, 0);
 * @endcode
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
  /** Audio device. */
  struct pbl_audio audio;
  /** DAC DMA channel. */
  const struct pbl_dma_chan *dma;
  /** Interrupt of the DMA channel. */
  pbl_irq_t dma_irq;
  /** Driver runtime state. */
  struct pbl_audio_sf32lb52_audcodec_state *state;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_audio_ops pbl_audio_sf32lb52_audcodec_ops;
/** @endcond */

/**
 * @brief Define the audio codec DAC, a playback device.
 *
 * @param sym Symbol of the device.
 * @param _name Device name.
 * @param _dma DAC DMA channel, on @ref SF32LB52_DMAC1 with request
 * @c SF32LB52_DMA_REQ_AUDAC_CH0.
 * @param _dma_irq Interrupt of the DMA channel.
 * @param _link What the DAC is wired to, or NULL.
 */
#define PBL_AUDIO_SF32LB52_AUDCODEC_DEFINE(sym, _name, _dma, _dma_irq, _link)                 \
  PBL_AUDIO_STATE_DEFINE(sym);                                                                \
  static struct pbl_audio_sf32lb52_audcodec_state sym##_audcodec_state;                       \
  static const struct pbl_audio_sf32lb52_audcodec sym = {                                     \
    .audio = PBL_AUDIO_INIT(sym, _name, PBL_AUDIO_PLAYBACK, &pbl_audio_sf32lb52_audcodec_ops, \
                            _link, PBL_DEVICE_DEPS(&(_dma)->ctrl->dev)),                      \
    .dma = (_dma),                                                                            \
    .dma_irq = (_dma_irq),                                                                    \
    .state = &sym##_audcodec_state,                                                           \
  };                                                                                          \
  PBL_DEVICE_REGISTER(sym, &sym.audio.dev)

/** @cond INTERNAL_HIDDEN */
struct pbl_audio_sf32lb52_pdm_state {
  uint32_t ring_bytes;
};
/** @endcond */

/** @brief PDM microphone interface. */
struct pbl_audio_sf32lb52_pdm {
  /** Audio device. */
  struct pbl_audio audio;
  /** Clock and data pins. */
  const struct pbl_pinctrl *pinctrl;
  /** PDM interrupt, reporting FIFO overflows. */
  pbl_irq_t irq;
  /** DMA channel. */
  const struct pbl_dma_chan *dma;
  /** Interrupt of the DMA channel. */
  pbl_irq_t dma_irq;
  /** Microphone supply, on while recording, or NULL. */
  const struct pbl_regulator *vdd;
  /** Driver runtime state. */
  struct pbl_audio_sf32lb52_pdm_state *state;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_audio_ops pbl_audio_sf32lb52_pdm_ops;
/** @endcond */

/**
 * @brief Define the PDM interface, a capture device.
 *
 * @param sym Symbol of the device.
 * @param _name Device name.
 * @param _pinctrl Clock and data pins, from PBL_PINCTRL().
 * @param _irq PDM interrupt; the board connects pbl_audio_sf32lb52_pdm_irq_handler() to it.
 * @param _dma DMA channel, on @ref SF32LB52_DMAC1 with request @c SF32LB52_DMA_REQ_PDM1_L.
 * @param _dma_irq Interrupt of the DMA channel.
 * @param _vdd Microphone supply, or NULL.
 */
#define PBL_AUDIO_SF32LB52_PDM_DEFINE(sym, _name, _pinctrl, _irq, _dma, _dma_irq, _vdd)          \
  PBL_AUDIO_STATE_DEFINE(sym);                                                                   \
  static struct pbl_audio_sf32lb52_pdm_state sym##_pdm_state;                                    \
  static const struct pbl_pinctrl sym##_pdm_pins = _pinctrl;                                     \
  static const struct pbl_audio_sf32lb52_pdm sym = {                                             \
    .audio = PBL_AUDIO_INIT(sym, _name, PBL_AUDIO_CAPTURE, &pbl_audio_sf32lb52_pdm_ops, nullptr, \
                            PBL_DEVICE_DEPS(&(_dma)->ctrl->dev)),                                \
    .pinctrl = &sym##_pdm_pins,                                                                  \
    .irq = (_irq),                                                                               \
    .dma = (_dma),                                                                               \
    .dma_irq = (_dma_irq),                                                                       \
    .vdd = (_vdd),                                                                               \
    .state = &sym##_pdm_state,                                                                   \
  };                                                                                             \
  PBL_DEVICE_REGISTER(sym, &sym.audio.dev)

/**
 * @brief PDM interrupt handler, to connect with PBL_IRQ_CONNECT().
 *
 * @param pdm PDM interface.
 */
void pbl_audio_sf32lb52_pdm_irq_handler(const struct pbl_audio_sf32lb52_pdm *pdm);

/** @} */
