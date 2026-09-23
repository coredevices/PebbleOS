/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/audio_encoder/audio_encoder_backend.h"

#include "pbl/util/size.h"

static const AudioEncoderBackend *const s_backends[] = {
#ifdef CONFIG_SPEEX
  &g_audio_encoder_backend_speex,
#endif
};

const AudioEncoderBackend *const *audio_encoder_get_backends(size_t *num_backends_out) {
  *num_backends_out = ARRAY_LENGTH(s_backends);
  return s_backends;
}
