/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/audio_encoder/audio_encoder.h"
#include "pbl/services/audio_encoder/audio_encoder_backend.h"

#include "pbl/kernel/mutex.h"
#include <pbl/logging/logging.h>

PBL_LOG_MODULE_DEFINE(service_audio_encoder, CONFIG_SERVICE_AUDIO_ENCODER_LOG_LEVEL);

static PBL_MUTEX_DEFINE(s_lock);

typedef struct {
  const AudioEncoderBackend *backend; //!< NULL when closed
  PebbleTask owner;
  AudioEncoderInfo info;
} AudioEncoderState;

static AudioEncoderState s_state;

void audio_encoder_service_init(void) {
  s_state = (AudioEncoderState){};
}

static const AudioEncoderBackend *prv_find_backend(AudioCodec codec) {
  size_t num_backends = 0;
  const AudioEncoderBackend *const *backends = audio_encoder_get_backends(&num_backends);
  for (size_t i = 0; i < num_backends; i++) {
    if (backends[i]->codec == codec) {
      return backends[i];
    }
  }
  return NULL;
}

bool audio_encoder_service_is_codec_available(AudioCodec codec) {
  return (prv_find_backend(codec) != NULL);
}

//! Expects s_lock held
static void prv_close_locked(void) {
  if (s_state.backend) {
    s_state.backend->close();
    PBL_LOG_DBG("Encoder closed (owner %u)", s_state.owner);
  }
  s_state = (AudioEncoderState){};
}

bool audio_encoder_service_open(AudioCodec codec, PebbleTask owner, AudioEncoderInfo *info_out) {
  const AudioEncoderBackend *backend = prv_find_backend(codec);
  if (!backend || !info_out) {
    return false;
  }

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (s_state.backend) {
    if (owner != AUDIO_ENCODER_SYSTEM_OWNER) {
      PBL_LOG_DBG("Encoder busy (owner %u), refusing %u", s_state.owner, owner);
      pbl_mutex_unlock(&s_lock);
      return false;
    }
    // The system takes precedence over an app
    prv_close_locked();
  }

  AudioEncoderInfo info = {};
  if (!backend->open(&info)) {
    PBL_LOG_ERR("Backend for codec %u failed to open", codec);
    pbl_mutex_unlock(&s_lock);
    return false;
  }
  info.codec = codec;
  s_state = (AudioEncoderState){
    .backend = backend,
    .owner = owner,
    .info = info,
  };
  *info_out = info;
  pbl_mutex_unlock(&s_lock);
  PBL_LOG_DBG("Encoder opened, codec %u, owner %u", codec, owner);
  return true;
}

int audio_encoder_service_encode(PebbleTask owner, const int16_t *pcm, uint32_t num_samples,
                                 uint8_t *out, uint32_t out_len) {
  if (!pcm || !out) {
    return -1;
  }
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  int rv = -1;
  if (s_state.backend && (s_state.owner == owner) &&
      (num_samples == (uint32_t)s_state.info.frame_samples * s_state.info.channels)) {
    rv = s_state.backend->encode(pcm, out, out_len);
  }
  pbl_mutex_unlock(&s_lock);
  return rv;
}

void audio_encoder_service_close(PebbleTask owner) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (s_state.backend && (s_state.owner == owner)) {
    prv_close_locked();
  }
  pbl_mutex_unlock(&s_lock);
}

void audio_encoder_service_close_for_task(PebbleTask task) {
  audio_encoder_service_close(task);
}

bool audio_encoder_service_is_open(void) {
  return (s_state.backend != NULL);
}
