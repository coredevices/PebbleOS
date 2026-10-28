/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/drivers/display/sharp_ls013b7dh01.h>
#include <pbl/util/bitops.h>
#include <pbl/util/misc.h>

#define DISP_MODE_WRITE 0x01U
#define DISP_MODE_CLEAR 0x04U

#define LINE_BYTES SHARP_LS013B7DH01_LINE_BYTES

static const struct pbl_display_sharp_ls013b7dh01 *prv_sharp(const struct pbl_display *disp) {
  return container_of(disp, const struct pbl_display_sharp_ls013b7dh01, disp);
}

// EXTCOMIN toggles the panel's VCOM on rising edges, so it takes short pulses
static int prv_extcomin_init(const struct pbl_display_sharp_ls013b7dh01 *sharp) {
  struct pbl_pwm_state state;
  int ret;

  if (pbl_device_init(&sharp->extcomin.ctrl->dev) != 0) {
    return -ENODEV;
  }

  pbl_pwm_init_state(&sharp->extcomin, &state);
  ret = pbl_pwm_set_relative_duty_cycle(&state, 1U, 20U);
  if (ret != 0) {
    return ret;
  }

  state.enabled = true;

  return pbl_pwm_apply(&sharp->extcomin, &state);
}

static void prv_spi_done(int status, void *arg) {
  pbl_display_complete(arg, status);
}

static int prv_init(const struct pbl_display *disp) {
  const struct pbl_display_sharp_ls013b7dh01 *sharp = prv_sharp(disp);
  int ret;

  if (pbl_device_init(&sharp->spi.bus->dev) != 0 ||
      pbl_device_init(&sharp->enable.port->dev) != 0) {
    return -ENODEV;
  }

  ret = pbl_spi_setup(&sharp->spi);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_gpio_configure(&sharp->enable, PBL_GPIO_OUTPUT_ACTIVE);
  if (ret != 0) {
    return ret;
  }

  return prv_extcomin_init(sharp);
}

static int prv_enable(const struct pbl_display *disp) {
  return pbl_gpio_set(&prv_sharp(disp)->enable, true);
}

static int prv_disable(const struct pbl_display *disp) {
  return pbl_gpio_set(&prv_sharp(disp)->enable, false);
}

static int prv_set_rotated(const struct pbl_display *disp, bool rotated) {
  prv_sharp(disp)->state->rotated = rotated;

  return 0;
}

static int prv_update(const struct pbl_display *disp, const struct pbl_display_buffer *buf,
                      uint16_t y0, uint16_t y1) {
  const struct pbl_display_sharp_ls013b7dh01 *sharp = prv_sharp(disp);
  struct pbl_display_sharp_ls013b7dh01_state *state = sharp->state;
  uint8_t *p = state->buf;

  *p++ = DISP_MODE_WRITE;

  for (uint16_t y = y0; y <= y1; y++) {
    const uint8_t *row = &buf->data[y * buf->pitch];

    // Line address, data and trailing dummy
    if (state->rotated) {
      *p++ = SHARP_LS013B7DH01_HEIGHT - y;
      for (int i = LINE_BYTES - 1; i >= 0; i--) {
        *p++ = pbl_bitrev8(row[i]);
      }
    } else {
      *p++ = y + 1U;
      memcpy(p, row, LINE_BYTES);
      p += LINE_BYTES;
    }
    *p++ = 0x00U;
  }

  // Last trailing dummy
  *p++ = 0x00U;

  state->xfer = (struct pbl_spi_xfer){.tx_buf = state->buf, .len = p - state->buf};

  return pbl_spi_transfer_async(&sharp->spi, &state->xfer, 1U, prv_spi_done, (void *)disp);
}

static int prv_clear(const struct pbl_display *disp) {
  uint8_t buf[] = {DISP_MODE_CLEAR, 0x00U};

  return pbl_spi_write(&prv_sharp(disp)->spi, buf, sizeof(buf));
}

const struct pbl_display_ops pbl_display_sharp_ls013b7dh01_ops = {
  .init = prv_init,
  .enable = prv_enable,
  .disable = prv_disable,
  .set_rotated = prv_set_rotated,
  .update = prv_update,
  .clear = prv_clear,
};
