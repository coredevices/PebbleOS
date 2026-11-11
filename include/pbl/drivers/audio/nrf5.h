/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/audio.h>
#include <pbl/drivers/clk.h>
#include <pbl/drivers/gpio.h>
#include <pbl/kernel/irq.h>

/**
 * @defgroup drivers_audio_nrf5 nRF5 audio
 * @ingroup drivers_audio
 * @brief @ref drivers_audio devices of the nRF52: the PDM and I2S interfaces.
 *
 * The PDM interface records one or two digital microphones at 16 kHz from a 1.28 MHz clock;
 * its gain runs from -20 to +20 dB. The I2S interface plays mono at 16 kHz, sent as stereo with
 * both channels equal, and runs as a slave when the link has @ref PBL_AUDIO_FMT_CODEC_MASTER,
 * still providing a 4 MHz master clock. Both run HFCLK from the crystal while streaming.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
struct pbl_audio_nrf5_pdm_state {
  uint8_t next;
  bool started;
};
/** @endcond */

/** @brief PDM interface. */
struct pbl_audio_nrf5_pdm {
  /** DAI. */
  struct pbl_audio_dai dai;
  /** Register base address. */
  uintptr_t base;
  /** Interrupt line. */
  pbl_irq_t irq;
  /** HFCLK, run from the crystal while streaming. */
  struct pbl_clk hfclk;
  /** Clock pin. */
  struct pbl_gpio clk;
  /** Data pin. */
  struct pbl_gpio din;
  /** Driver runtime state. */
  struct pbl_audio_nrf5_pdm_state *state;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_audio_dai_ops pbl_audio_nrf5_pdm_ops;
/** @endcond */

/**
 * @brief PDM interrupt handler, connected by the instance.
 *
 * @param pdm PDM interface.
 */
void pbl_audio_nrf5_pdm_irq_handler(const struct pbl_audio_nrf5_pdm *pdm);

/**
 * @brief Define the PDM interface, a capture DAI.
 *
 * @param sym Symbol of the DAI.
 * @param _name Device name.
 * @param _base Register base address.
 * @param _irq Interrupt line.
 * @param _hfclk HFCLK, from PBL_CLK().
 * @param _clk Clock pin, from PBL_GPIO().
 * @param _din Data pin, from PBL_GPIO().
 */
#define PBL_AUDIO_NRF5_PDM_DEFINE(sym, _name, _base, _irq, _hfclk, _clk, _din)                  \
  PBL_AUDIO_DAI_STATE_DEFINE(sym);                                                              \
  static struct pbl_audio_nrf5_pdm_state sym##_pdm_state;                                       \
  PBL_DEVICE_INSTANCE(sym, struct pbl_audio_nrf5_pdm, struct pbl_audio_dai, dai){               \
    .dai = PBL_AUDIO_DAI_INIT(sym, _name, PBL_AUDIO_CAPTURE, &pbl_audio_nrf5_pdm_ops, nullptr), \
    .base = (_base),                                                                            \
    .irq = (_irq),                                                                              \
    .hfclk = _hfclk,                                                                            \
    .clk = _clk,                                                                                \
    .din = _din,                                                                                \
    .state = &sym##_pdm_state,                                                                  \
  };                                                                                            \
  PBL_IRQ_CONNECT_NUM(_irq, CONFIG_AUDIO_NRF5_PDM_IRQ_PRIO, pbl_audio_nrf5_pdm_irq_handler,     \
                      &sym##_inst, 0)

/** @brief Stereo frames of an I2S bounce buffer. */
#define PBL_AUDIO_NRF5_I2S_BOUNCE_FRAMES 512U

/** @cond INTERNAL_HIDDEN */
struct pbl_audio_nrf5_i2s_state {
  int16_t *bounce[2];
  uint8_t bounce_idx;
  uint8_t ring_idx;
};
/** @endcond */

/** @brief I2S interface. */
struct pbl_audio_nrf5_i2s {
  /** DAI. */
  struct pbl_audio_dai dai;
  /** Register base address. */
  uintptr_t base;
  /** Interrupt line. */
  pbl_irq_t irq;
  /** HFCLK, run from the crystal while streaming. */
  struct pbl_clk hfclk;
  /** Bit clock pin. */
  struct pbl_gpio sck;
  /** Frame clock pin. */
  struct pbl_gpio lrck;
  /** Master clock pin. */
  struct pbl_gpio mck;
  /** Data out pin. */
  struct pbl_gpio sdout;
  /** Driver runtime state. */
  struct pbl_audio_nrf5_i2s_state *state;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_audio_dai_ops pbl_audio_nrf5_i2s_ops;
/** @endcond */

/**
 * @brief I2S interrupt handler, connected by the instance.
 *
 * @param i2s I2S interface.
 */
void pbl_audio_nrf5_i2s_irq_handler(const struct pbl_audio_nrf5_i2s *i2s);

/**
 * @brief Define the I2S interface, a playback DAI.
 *
 * Periods are copied into stereo bounce buffers, so a period must fit one.
 *
 * @param sym Symbol of the DAI.
 * @param _name Device name.
 * @param _base Register base address.
 * @param _irq Interrupt line.
 * @param _hfclk HFCLK, from PBL_CLK().
 * @param _sck Bit clock pin, from PBL_GPIO().
 * @param _lrck Frame clock pin, from PBL_GPIO().
 * @param _mck Master clock pin, from PBL_GPIO().
 * @param _sdout Data out pin, from PBL_GPIO().
 */
#define PBL_AUDIO_NRF5_I2S_DEFINE(sym, _name, _base, _irq, _hfclk, _sck, _lrck, _mck, _sdout)    \
  PBL_AUDIO_DAI_STATE_DEFINE(sym);                                                               \
  static struct pbl_audio_nrf5_i2s_state sym##_i2s_state;                                        \
  PBL_DEVICE_INSTANCE(sym, struct pbl_audio_nrf5_i2s, struct pbl_audio_dai, dai){                \
    .dai = PBL_AUDIO_DAI_INIT(sym, _name, PBL_AUDIO_PLAYBACK, &pbl_audio_nrf5_i2s_ops, nullptr), \
    .base = (_base),                                                                             \
    .irq = (_irq),                                                                               \
    .hfclk = _hfclk,                                                                             \
    .sck = _sck,                                                                                 \
    .lrck = _lrck,                                                                               \
    .mck = _mck,                                                                                 \
    .sdout = _sdout,                                                                             \
    .state = &sym##_i2s_state,                                                                   \
  };                                                                                             \
  PBL_IRQ_CONNECT_NUM(_irq, CONFIG_AUDIO_NRF5_I2S_IRQ_PRIO, pbl_audio_nrf5_i2s_irq_handler,      \
                      &sym##_inst, 0)

/** @} */
