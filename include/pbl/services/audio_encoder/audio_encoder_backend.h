/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/services/audio_encoder/audio_encoder.h"

#include <stddef.h>

//! One codec implementation. A backend is a singleton: open() allocates its state, close()
//! frees it, and encode() is only called in between.
typedef struct AudioEncoderBackend {
  AudioCodec codec;
  //! @param[out] info_out Filled on success
  bool (*open)(AudioEncoderInfo *info_out);
  //! @param pcm frame_samples * channels samples
  //! @return bytes written or negative on error
  int (*encode)(const int16_t *pcm, uint8_t *out, uint32_t out_len);
  void (*close)(void);
} AudioEncoderBackend;

//! Backends compiled into this firmware. Tests provide their own.
const AudioEncoderBackend *const *audio_encoder_get_backends(size_t *num_backends_out);

#ifdef CONFIG_SPEEX
extern const AudioEncoderBackend g_audio_encoder_backend_speex;
#endif
