/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/audio.h>
#include <pbl/drivers/i2c.h>
#include <pbl/drivers/regulator.h>

/**
 * @defgroup drivers_audio_da7212 DA7212 codec
 * @ingroup drivers_audio
 * @brief @ref drivers_audio codec: Dialog DA7212, playing to its line output and recording from
 * its first microphone input.
 *
 * The codec runs its PLL from a 4 MHz master clock on the link and is the I2S master, deriving
 * an exact 16 kHz frame clock. It serves both directions of the link, powered while either
 * streams.
 *
 * Playback goes through the right DAC to the line output; its DAC gain runs from -78 to +12 dB in
 * 0.75 dB steps. Capture takes the microphone single-ended on MIC1_P, powered by MICBIAS1,
 * through its +30 dB amplifier to both ADCs, which give the left and right channels; the ADC gain
 * runs over the same range as the DAC gain.
 *
 * @code{.c}
 * PBL_AUDIO_DA7212_DEFINE(s_da7212, "da7212", &s_i2c.bus, 0x1a, &s_ldo.reg, 2200);
 * static const struct pbl_audio_link s_link = {
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
  /** Supply of the digital and analogue circuits, to reset the codec if needed. */
  const struct pbl_regulator *vdd;
  /** MICBIAS1 voltage, in mV: 1600, 2200, 2500 or 3000; 0 when the board does not record. */
  uint16_t micbias1_mv;
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
 * @param _vdd Supply of the digital and analogue circuits, always on.
 * @param _micbias1_mv MICBIAS1 voltage, in mV: 1600, 2200, 2500 or 3000; 0 when the board does
 *                     not record.
 */
#define PBL_AUDIO_DA7212_DEFINE(sym, _name, _bus, _addr, _vdd, _micbias1_mv)         \
  PBL_DEVICE_STATE_DEFINE(sym);                                                      \
  PBL_DEVICE_INSTANCE(sym, struct pbl_audio_da7212, struct pbl_audio_codec, codec) { \
    .codec = PBL_AUDIO_CODEC_INIT(sym, _name, pbl_audio_da7212_init, &(_bus)->dev,   \
                                  &pbl_audio_da7212_ops),                            \
    .i2c = PBL_I2C_DEV(_bus, _addr), .vdd = (_vdd), .micbias1_mv = (_micbias1_mv),   \
  }

/** @} */
