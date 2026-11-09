/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/audio.h>
#include <pbl/drivers/regulator.h>

/**
 * @defgroup drivers_audio_dmic Digital microphone
 * @ingroup drivers_audio
 * @brief Digital microphones on a PDM interface, as a codec without a gain: powered from their
 *        reference supply while recording, the gain left to the interface.
 * @{
 */

/** @brief Digital microphones. */
struct pbl_audio_dmic {
  /** Codec. */
  struct pbl_audio_codec codec;
  /** Reference supply, or NULL. */
  const struct pbl_regulator *vref;
  /** Time the microphones take to wake up once powered, in ms. */
  uint32_t wakeup_delay_ms;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_audio_codec_ops pbl_audio_dmic_ops;
/** @endcond */

/**
 * @brief Define digital microphones.
 *
 * @param sym Symbol of the codec.
 * @param _name Device name.
 * @param _vref Reference supply, or NULL.
 * @param _wakeup_delay_ms Time they take to wake up once powered, in ms.
 */
#define PBL_AUDIO_DMIC_DEFINE(sym, _name, _vref, _wakeup_delay_ms)                 \
  PBL_DEVICE_STATE_DEFINE(sym);                                                    \
  PBL_DEVICE_INSTANCE(sym, struct pbl_audio_dmic, struct pbl_audio_codec, codec) { \
    .codec =                                                                       \
        {                                                                          \
          .dev = PBL_DEVICE_INIT(sym, _name, nullptr, nullptr, nullptr),           \
          .ops = &pbl_audio_dmic_ops,                                              \
        },                                                                         \
    .vref = (_vref), .wakeup_delay_ms = (_wakeup_delay_ms),                        \
  }

/** @} */
