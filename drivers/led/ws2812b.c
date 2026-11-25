/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/led/ws2812b.h>
#include <pbl/util/byteorder.h>
#include <pbl/util/misc.h>

#define WS2812B_MIN_HZ 2105000U
#define WS2812B_MAX_HZ 2850000U

static const struct pbl_ws2812b *prv_get(const struct pbl_led *led) {
  return PBL_CONTAINER_OF(led, const struct pbl_ws2812b, led);
}

// Each bit becomes 1x0: 8 bits fill 3 bytes
static void prv_encode(uint8_t byte, uint8_t *out) {
  uint32_t bits = 0U;

  for (int i = 7; i >= 0; i--) {
    bits = (bits << 3) | (((byte >> i) & 1U) != 0U ? 0x6U : 0x4U);
  }

  pbl_put_be24(bits, out);
}

static uint8_t prv_scale(uint32_t rgb_color, unsigned int shift, uint8_t brightness) {
  return (uint8_t)((((rgb_color >> shift) & 0xFFU) * brightness) / PBL_LED_BRIGHTNESS_MAX);
}

static int prv_set(const struct pbl_led *led, uint8_t brightness, uint32_t rgb_color) {
  const struct pbl_ws2812b *ws = prv_get(led);
  uint8_t *buf = ws->state->buf;

  // The LED takes green, then red, then blue
  prv_encode(prv_scale(rgb_color, 8U, brightness), &buf[0]);
  prv_encode(prv_scale(rgb_color, 16U, brightness), &buf[3]);
  prv_encode(prv_scale(rgb_color, 0U, brightness), &buf[6]);

  return pbl_spi_write(&ws->spi, buf, sizeof(ws->state->buf));
}

int pbl_ws2812b_init(const struct pbl_device *dev) {
  const struct pbl_ws2812b *ws = PBL_CONTAINER_OF(dev, const struct pbl_ws2812b, led.dev);

  if (ws->spi.max_hz < WS2812B_MIN_HZ || ws->spi.max_hz > WS2812B_MAX_HZ) {
    return -EINVAL;
  }

  memset(ws->state->buf, 0, sizeof(ws->state->buf));

  return prv_set(&ws->led, 0U, 0U);
}

const struct pbl_led_ops pbl_ws2812b_ops = {
  .set = prv_set,
};

#include PBL_DT_INSTANCES(worldsemi_ws2812b)
