/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/audio.h>
#include <pbl/drivers/gpio.h>
#include <pbl/drivers/speaker/sf32lb52/audio_definitions.h>
#include <pbl/drivers/speaker/sf32lb52/sf32lb_audio.h>
#include <pbl/kernel/irq.h>

#include <kernel/util/delay.h>

#define PA_POWER_DELAY_TIME (200) /* us */

void audio_init(AudioDevice *audio_device) {
  pbl_gpio_configure(&audio_device->pa_ctrl, PBL_GPIO_OUTPUT_INACTIVE);
  delay_us(PA_POWER_DELAY_TIME * 10);
  audec_init(audio_device);
}

void audio_start(AudioDevice *audio_device, AudioTransCB cb) {
  if (audio_device->power_ops && audio_device->power_ops->power_up) {
    audio_device->power_ops->power_up();
  }

  pbl_gpio_set(&audio_device->pa_ctrl, true);
  delay_us(PA_POWER_DELAY_TIME);
  pbl_gpio_set(&audio_device->pa_ctrl, false);
  delay_us(PA_POWER_DELAY_TIME);
  pbl_gpio_set(&audio_device->pa_ctrl, true);
  audec_start(audio_device, cb);
}

uint32_t audio_write(AudioDevice *audio_device, void *writeBuf, uint32_t size) {
  return audec_write(audio_device, writeBuf, size);
}

void audio_set_volume(AudioDevice *audio_device, int volume) {
  audec_set_vol(audio_device, volume);
}

void audio_stop(AudioDevice *audio_device) {
  audec_stop(audio_device);
  pbl_gpio_set(&audio_device->pa_ctrl, false);

  if (audio_device->power_ops && audio_device->power_ops->power_down) {
    audio_device->power_ops->power_down();
  }
}

bool audio_set_playback_callback(AudioDevice *device, AudioPlaybackCB cb, void *context) {
  if (device->samplerate != AUDIO_PLAYBACK_SAMPLE_RATE || device->channels != 1) {
    return false;
  }
  pbl_irq_lock();
  device->state->playback_cb = cb;
  device->state->playback_context = context;
  device->state->playback_started = false;
  pbl_irq_unlock();
  return true;
}
