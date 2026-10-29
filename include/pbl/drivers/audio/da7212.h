/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/audio.h>
#include <pbl/drivers/i2c.h>

/**
 * @defgroup drivers_audio_da7212 DA7212 codec
 * @ingroup drivers_audio
 * @brief @ref drivers_audio codec: Dialog DA7212, playing to its line output.
 *
 * The codec runs its PLL from a 4 MHz master clock on the link and is the I2S master, deriving
 * an exact 16 kHz frame clock. Its DAC gain runs from -78 to +12 dB in 0.75 dB steps.
 *
 * @code{.c}
 * PBL_AUDIO_DA7212_DEFINE(s_da7212, "da7212", &s_i2c.bus, 0x1a);
 * static const struct pbl_audio_link s_speaker_link = {
 *   .codec = &s_da7212.codec,
 *   .fmt = PBL_AUDIO_FMT_I2S | PBL_AUDIO_FMT_CODEC_MASTER,
 * };
 * @endcode
 * @{
 */

/** @brief DA7212 codec. */
struct pbl_audio_da7212 {
  /** Codec. */
  struct pbl_audio_codec codec;
  /** Control port. */
  struct pbl_i2c_dev i2c;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_audio_codec_ops pbl_audio_da7212_ops;
int pbl_audio_da7212_init(const struct pbl_device *dev);
/** @endcond */

/**
 * @brief Define a DA7212 codec.
 *
 * @param sym Symbol of the codec.
 * @param _name Device name.
 * @param _bus I2C bus.
 * @param _addr 7-bit I2C address.
 */
#define PBL_AUDIO_DA7212_DEFINE(sym, _name, _bus, _addr)                                    \
  PBL_DEVICE_STATE_DEFINE(sym);                                                             \
  static const struct pbl_audio_da7212 sym = {                                              \
    .codec =                                                                                \
        {                                                                                   \
          .dev = PBL_DEVICE_INIT(sym, _name, pbl_audio_da7212_init, &(_bus)->dev, nullptr), \
          .ops = &pbl_audio_da7212_ops,                                                     \
        },                                                                                  \
    .i2c = PBL_I2C_DEV(_bus, _addr),                                                        \
  };                                                                                        \
  PBL_DEVICE_REGISTER(sym, &sym.codec.dev)

/** @} */
