/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/audio.h>
#include <pbl/drivers/gpio.h>

/**
 * @defgroup drivers_audio_nrf5 nRF5 audio
 * @ingroup drivers_audio
 * @brief @ref drivers_audio devices of the nRF52: the PDM and I2S interfaces.
 *
 * The PDM interface records one or two digital microphones at 16 kHz from a 1.28 MHz clock;
 * its gain runs from -20 to +20 dB. The I2S interface plays mono at 16 kHz, sent as stereo with
 * both channels equal, and runs as a slave when the link has @ref PBL_AUDIO_FMT_CODEC_MASTER,
 * still providing a 4 MHz master clock. Both keep the high-frequency crystal on while streaming;
 * the board connects the nrfx interrupt handlers.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
struct pbl_audio_nrf5_pdm_state {
  uint8_t next;
};
/** @endcond */

/** @brief PDM interface. */
struct pbl_audio_nrf5_pdm {
  /** Audio device. */
  struct pbl_audio audio;
  /** Clock pin. */
  struct pbl_gpio clk;
  /** Data pin. */
  struct pbl_gpio din;
  /** Gain after initialization, in mdB. */
  int32_t default_gain_mdb;
  /** Driver runtime state. */
  struct pbl_audio_nrf5_pdm_state *state;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_audio_ops pbl_audio_nrf5_pdm_ops;
/** @endcond */

/**
 * @brief Define the PDM interface, a capture device.
 *
 * @param sym Symbol of the device.
 * @param _name Device name.
 * @param _clk Clock pin, from PBL_GPIO().
 * @param _din Data pin, from PBL_GPIO().
 * @param _default_gain_mdb Gain after initialization, in mdB.
 */
#define PBL_AUDIO_NRF5_PDM_DEFINE(sym, _name, _clk, _din, _default_gain_mdb)                      \
  PBL_AUDIO_STATE_DEFINE(sym);                                                                    \
  static struct pbl_audio_nrf5_pdm_state sym##_pdm_state;                                         \
  static const struct pbl_audio_nrf5_pdm sym = {                                                  \
    .audio =                                                                                      \
        PBL_AUDIO_INIT(sym, _name, PBL_AUDIO_CAPTURE, &pbl_audio_nrf5_pdm_ops, nullptr, nullptr), \
    .clk = _clk,                                                                                  \
    .din = _din,                                                                                  \
    .default_gain_mdb = (_default_gain_mdb),                                                      \
    .state = &sym##_pdm_state,                                                                    \
  };                                                                                              \
  PBL_DEVICE_REGISTER(sym, &sym.audio.dev)

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
  /** Audio device. */
  struct pbl_audio audio;
  /** Bit clock pin. */
  struct pbl_gpio sck;
  /** Frame clock pin. */
  struct pbl_gpio lrck;
  /** Master clock pin. */
  struct pbl_gpio mck;
  /** Data out pin. */
  struct pbl_gpio sdout;
  /** Interrupt priority. */
  uint8_t irq_priority;
  /** Driver runtime state. */
  struct pbl_audio_nrf5_i2s_state *state;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_audio_ops pbl_audio_nrf5_i2s_ops;
/** @endcond */

/**
 * @brief Define the I2S interface, a playback device.
 *
 * Periods are copied into stereo bounce buffers, so a period must fit one.
 *
 * @param sym Symbol of the device.
 * @param _name Device name.
 * @param _link What the interface is wired to.
 * @param _irq_priority Interrupt priority.
 * @param ... Pins: @c sck, @c lrck, @c mck and @c sdout, from PBL_GPIO().
 */
#define PBL_AUDIO_NRF5_I2S_DEFINE(sym, _name, _link, _irq_priority, ...)                         \
  PBL_AUDIO_STATE_DEFINE(sym);                                                                   \
  static struct pbl_audio_nrf5_i2s_state sym##_i2s_state;                                        \
  static const struct pbl_audio_nrf5_i2s sym = {                                                 \
    .audio =                                                                                     \
        PBL_AUDIO_INIT(sym, _name, PBL_AUDIO_PLAYBACK, &pbl_audio_nrf5_i2s_ops, _link, nullptr), \
    .irq_priority = (_irq_priority),                                                             \
    .state = &sym##_i2s_state,                                                                   \
    __VA_ARGS__                                                                                  \
  };                                                                                             \
  PBL_DEVICE_REGISTER(sym, &sym.audio.dev)

/** @} */
