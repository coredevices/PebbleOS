/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/audio.h>
#include <pbl/drivers/i2c.h>

/**
 * @defgroup drivers_audio_wm8994 Cirrus Logic WM8994
 * @ingroup drivers_audio
 * @brief @ref drivers_audio codec driver for the WM8994, playing to its headphone output.
 *
 * The codec is the slave of an I2S link on its first audio interface, its clocks from the master
 * clock at 256 times the rate on MCLK1.
 * @{
 */

/** @brief WM8994 codec. */
struct pbl_audio_wm8994 {
  /** Codec. */
  struct pbl_audio_codec codec;
  /** Control port. */
  struct pbl_i2c_dev i2c;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_audio_codec_ops pbl_audio_wm8994_ops;
int pbl_audio_wm8994_init(const struct pbl_device *dev);
/** @endcond */

/**
 * @brief Define a WM8994 codec.
 *
 * @param sym Symbol of the codec.
 * @param _name Device name.
 * @param _bus I2C bus.
 * @param _addr 7-bit I2C address.
 */
#define PBL_AUDIO_WM8994_DEFINE(sym, _name, _bus, _addr)                             \
  PBL_DEVICE_STATE_DEFINE(sym);                                                      \
  PBL_DEVICE_INSTANCE(sym, struct pbl_audio_wm8994, struct pbl_audio_codec, codec) { \
    .codec = PBL_AUDIO_CODEC_INIT(sym, _name, pbl_audio_wm8994_init, &(_bus)->dev,   \
                                  &pbl_audio_wm8994_ops),                            \
    .i2c = PBL_I2C_DEV(_bus, _addr),                                                 \
  }

/** @} */
