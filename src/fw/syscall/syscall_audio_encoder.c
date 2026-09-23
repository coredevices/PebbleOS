/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "syscall/syscall.h"
#include "syscall/syscall_internal.h"

#include "kernel/pebble_tasks.h"
#include "pbl/services/audio_encoder/audio_encoder.h"

// Builds without the encoder service (e.g. PRF) report no codec and refuse every request.

DEFINE_SYSCALL(bool, sys_audio_encoder_codec_available, uint8_t codec) {
#ifdef CONFIG_SERVICE_AUDIO_ENCODER
  return audio_encoder_service_is_codec_available((AudioCodec)codec);
#else
  return false;
#endif
}

DEFINE_SYSCALL(bool, sys_audio_encoder_open, uint8_t codec, AudioEncoderInfo *info_out) {
  if (PRIVILEGE_WAS_ELEVATED) {
    syscall_assert_userspace_buffer(info_out, sizeof(*info_out));
  }
  // Only the app task may hold the app encoder; the system owner is reserved for dictation.
  const PebbleTask task = pebble_task_get_current();
  if (task != PebbleTask_App) {
    return false;
  }
#ifdef CONFIG_SERVICE_AUDIO_ENCODER
  AudioEncoderInfo info;
  if (!audio_encoder_service_open((AudioCodec)codec, task, &info)) {
    return false;
  }
  *info_out = info;
  return true;
#else
  return false;
#endif
}

DEFINE_SYSCALL(int, sys_audio_encoder_encode, const int16_t *pcm, uint32_t num_samples,
               uint8_t *out, uint32_t out_len) {
  if (PRIVILEGE_WAS_ELEVATED) {
    if ((num_samples > AUDIO_ENCODER_MAX_FRAME_SAMPLES) ||
        (out_len > AUDIO_ENCODER_MAX_PACKET_BYTES)) {
      syscall_failed();
    }
    syscall_assert_userspace_buffer(pcm, num_samples * sizeof(int16_t));
    syscall_assert_userspace_buffer(out, out_len);
  }
#ifdef CONFIG_SERVICE_AUDIO_ENCODER
  return audio_encoder_service_encode(pebble_task_get_current(), pcm, num_samples, out, out_len);
#else
  return -1;
#endif
}

DEFINE_SYSCALL(void, sys_audio_encoder_close, void) {
#ifdef CONFIG_SERVICE_AUDIO_ENCODER
  audio_encoder_service_close(pebble_task_get_current());
#endif
}
