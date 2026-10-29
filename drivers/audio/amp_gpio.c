/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/audio/amp_gpio.h>
#include <pbl/kernel/delay.h>
#include <pbl/util/misc.h>

static const struct pbl_audio_amp_gpio *prv_amp(const struct pbl_audio_amp *amp) {
  return container_of(amp, const struct pbl_audio_amp_gpio, amp);
}

int pbl_audio_amp_gpio_init(const struct pbl_device *dev) {
  const struct pbl_audio_amp_gpio *amp =
      container_of(dev, const struct pbl_audio_amp_gpio, amp.dev);
  int ret;

  ret = pbl_device_init(&amp->enable.port->dev);
  if (ret != 0) {
    return -ENODEV;
  }

  ret = pbl_gpio_configure(&amp->enable, PBL_GPIO_OUTPUT_INACTIVE);
  if (ret != 0) {
    return ret;
  }

  pbl_busy_wait_us(amp->settle_us);

  return 0;
}

static int prv_enable(const struct pbl_audio_amp *a) {
  const struct pbl_audio_amp_gpio *amp = prv_amp(a);
  int ret;

  if (amp->supply != nullptr) {
    ret = pbl_regulator_enable(amp->supply);
    if (ret != 0) {
      return ret;
    }
  }

  for (uint8_t i = 0U; i < amp->mode_pulses; i++) {
    ret = pbl_gpio_set(&amp->enable, true);
    if (ret != 0) {
      return ret;
    }
    pbl_busy_wait_us(amp->pulse_us);

    ret = pbl_gpio_set(&amp->enable, false);
    if (ret != 0) {
      return ret;
    }
    pbl_busy_wait_us(amp->pulse_us);
  }

  return pbl_gpio_set(&amp->enable, true);
}

static int prv_disable(const struct pbl_audio_amp *a) {
  const struct pbl_audio_amp_gpio *amp = prv_amp(a);
  int ret;

  ret = pbl_gpio_set(&amp->enable, false);
  if (ret != 0) {
    return ret;
  }

  if (amp->supply != nullptr) {
    return pbl_regulator_disable(amp->supply);
  }

  return 0;
}

const struct pbl_audio_amp_ops pbl_audio_amp_gpio_ops = {
  .enable = prv_enable,
  .disable = prv_disable,
};
