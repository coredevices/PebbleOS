/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/display/sharp_memory.h>
#include <pbl/util/bitops.h>
#include <pbl/util/misc.h>

#define DISP_MODE_WRITE 0x01U
#define DISP_MODE_CLEAR 0x04U

static const struct pbl_display_sharp_memory *prv_sharp(const struct pbl_display *disp) {
  return PBL_CONTAINER_OF(disp, const struct pbl_display_sharp_memory, disp);
}

// The panel takes bytes least significant bit first: reversed here unless the bus does it
static inline bool prv_swap_bits(const struct pbl_display_sharp_memory *sharp) {
  return (sharp->spi.mode & PBL_SPI_LSB_FIRST) == 0U;
}

static inline uint8_t prv_wire(const struct pbl_display_sharp_memory *sharp, uint8_t byte) {
  return prv_swap_bits(sharp) ? pbl_bitrev8(byte) : byte;
}

// EXTCOMIN toggles the panel's VCOM on rising edges, so it takes short pulses
static int prv_extcomin_init(const struct pbl_display_sharp_memory *sharp) {
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
  const struct pbl_display_sharp_memory *sharp = prv_sharp(disp);
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
  const struct pbl_display_sharp_memory *sharp = prv_sharp(disp);
  struct pbl_display_sharp_memory_state *state = sharp->state;
  uint16_t height = disp->props.height;
  uint16_t line_bytes = disp->props.width / 8U;
  uint8_t *p = sharp->buf;

  *p++ = prv_wire(sharp, DISP_MODE_WRITE);

  for (uint16_t y = y0; y <= y1; y++) {
    const uint8_t *row = &buf->data[y * buf->pitch];

    // Line address, data and trailing dummy. Rotated rows also reverse the pixels of each byte.
    if (state->rotated) {
      *p++ = prv_wire(sharp, (uint8_t)(height - y));
      for (int i = line_bytes - 1; i >= 0; i--) {
        *p++ = prv_swap_bits(sharp) ? row[i] : pbl_bitrev8(row[i]);
      }
    } else {
      *p++ = prv_wire(sharp, (uint8_t)(y + 1U));
      if (prv_swap_bits(sharp)) {
        for (uint16_t i = 0U; i < line_bytes; i++) {
          *p++ = pbl_bitrev8(row[i]);
        }
      } else {
        memcpy(p, row, line_bytes);
        p += line_bytes;
      }
    }
    *p++ = 0x00U;
  }

  // Last trailing dummy
  *p++ = 0x00U;

  state->xfer = (struct pbl_spi_xfer){.tx_buf = sharp->buf, .len = p - sharp->buf};

  return pbl_spi_transfer_async(&sharp->spi, &state->xfer, 1U, prv_spi_done, (void *)disp);
}

static int prv_clear(const struct pbl_display *disp) {
  const struct pbl_display_sharp_memory *sharp = prv_sharp(disp);
  uint8_t buf[] = {prv_wire(sharp, DISP_MODE_CLEAR), 0x00U};

  return pbl_spi_write(&sharp->spi, buf, sizeof(buf));
}

const struct pbl_display_ops pbl_display_sharp_memory_ops = {
  .init = prv_init,
  .enable = prv_enable,
  .disable = prv_disable,
  .set_rotated = prv_set_rotated,
  .update = prv_update,
  .clear = prv_clear,
};

#include PBL_DT_INSTANCES(sharp_memory)
