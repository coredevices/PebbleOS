/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

 #include "audio_definitions.h"
 #include "drivers/gpio.h"
 #include "kernel/util/delay.h"
 #include "drivers/audio.h"
 #include "sf32lb_audio.h"

#define PA_POWER_DELAY_TIME      (200) /* us */

void audio_init(AudioDevice* audio_device) {
    gpio_output_init(&audio_device->pa_ctrl, GPIO_OType_PP, GPIO_Speed_2MHz);
    gpio_output_set(&audio_device->pa_ctrl, false);
    delay_us(PA_POWER_DELAY_TIME*10);
    audec_init(audio_device);
}

void audio_start(AudioDevice* audio_device, AudioTransCB cb) {
    gpio_output_set(&audio_device->pa_ctrl, true);
    delay_us(PA_POWER_DELAY_TIME);
    gpio_output_set(&audio_device->pa_ctrl, false);
    delay_us(PA_POWER_DELAY_TIME);
    gpio_output_set(&audio_device->pa_ctrl, true);
    audec_start(audio_device, cb);
}

uint32_t audio_write(AudioDevice* audio_device, void *writeBuf, uint32_t size) {
    return audec_write(audio_device, writeBuf, size);
}

void audio_set_volume(AudioDevice* audio_device, int volume) {
    audec_set_vol(audio_device, volume);
}

void audio_stop(AudioDevice* audio_device) {
    audec_stop(audio_device);
    gpio_output_set(&audio_device->pa_ctrl, false);
}
