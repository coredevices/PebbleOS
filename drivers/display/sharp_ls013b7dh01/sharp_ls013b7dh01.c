/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>
#include <string.h>

#include <pbl/drivers/display/sharp_ls013b7dh01/sharp_ls013b7dh01.h>
#include <pbl/drivers/gpio.h>
#include <pbl/drivers/pwm.h>
#include <pbl/drivers/spi.h>
#include <pbl/kernel/assert.h>
#include <pbl/util/bitops.h>

#include <board/board.h>
#include <kernel/events.h>

#define DISP_MODE_WRITE 0x01U
#define DISP_MODE_CLEAR 0x04U

static uint8_t s_buf[2 + ((DISP_LINE_BYTES + 2) * PBL_DISPLAY_HEIGHT)];
static bool s_updating;
static struct pbl_spi_xfer s_xfer = {.tx_buf = s_buf};
static UpdateCompleteCallback s_uccb;

// watch rotation
static bool s_rotated_180 = false;

// EXTCOMIN toggles the panel's VCOM on rising edges, so it takes short pulses
static void prv_extcomin_init(void) {
  const struct pbl_pwm *extcomin = &BOARD_CONFIG_DISPLAY.extcomin;
  struct pbl_pwm_state state;

  PBL_ASSERTN(pbl_device_init(&extcomin->ctrl->dev) == 0);

  pbl_pwm_init_state(extcomin, &state);
  PBL_ASSERTN(pbl_pwm_set_relative_duty_cycle(&state, 1U, 20U) == 0);
  state.enabled = true;
  PBL_ASSERTN(pbl_pwm_apply(extcomin, &state) == 0);
}

static void prv_update_done(void *data) {
  s_updating = false;
  s_uccb(true);
}

static void prv_spi_done(int status, void *arg) {
  PebbleEvent e = {
    .type = PEBBLE_CALLBACK_EVENT,
    .callback = {
      .callback = prv_update_done,
    },
  };

  PBL_ASSERTN(status == 0);
  event_put_isr(&e);
}

void display_init(void) {
  PBL_ASSERTN(pbl_device_init(&BOARD_CONFIG_DISPLAY.spi.bus->dev) == 0);
  PBL_ASSERTN(pbl_spi_setup(&BOARD_CONFIG_DISPLAY.spi) == 0);

  pbl_gpio_configure(&BOARD_CONFIG_DISPLAY.on_ctrl, PBL_GPIO_OUTPUT_ACTIVE);

  prv_extcomin_init();
}

void display_clear(void) {
  uint8_t buf[] = {DISP_MODE_CLEAR, 0x00};

  PBL_ASSERTN(!s_updating);

  PBL_ASSERTN(pbl_spi_write(&BOARD_CONFIG_DISPLAY.spi, buf, sizeof(buf)) == 0);
}

void display_set_enabled(bool enabled) {
  pbl_gpio_set(&BOARD_CONFIG_DISPLAY.on_ctrl, enabled);
}

void display_set_rotated(bool rotated) {
  s_rotated_180 = rotated;
}

void display_update(NextRowCallback nrcb, UpdateCompleteCallback uccb) {
  DisplayRow row;
  uint8_t *pbuf = s_buf;

  PBL_ASSERTN(!s_updating);

  // write command (write)
  *pbuf++ = DISP_MODE_WRITE;

  while (nrcb(&row)) {
    // write row address, data and trailing dummy
    *pbuf++ = s_rotated_180 ? (PBL_DISPLAY_HEIGHT - 1) - row.address + 1 : row.address + 1;
    if (s_rotated_180) {
      for (int i = DISP_LINE_BYTES - 1; i >= 0; --i) {
        *pbuf++ = pbl_bitrev8(row.data[i]);
      }
    } else {
      memcpy(pbuf, row.data, DISP_LINE_BYTES);
      pbuf += DISP_LINE_BYTES;
    }
    *pbuf++ = 0x00;
  }

  // write last trailing dummy
  *pbuf++ = 0x00;

  s_xfer.len = pbuf - s_buf;
  s_uccb = uccb;
  s_updating = true;

  PBL_ASSERTN(
      pbl_spi_transfer_async(&BOARD_CONFIG_DISPLAY.spi, &s_xfer, 1U, prv_spi_done, nullptr) == 0);
}

bool display_update_in_progress(void) {
  return s_updating;
}

/* stubs */

void display_update_boot_frame(uint8_t *framebuffer) {
}
