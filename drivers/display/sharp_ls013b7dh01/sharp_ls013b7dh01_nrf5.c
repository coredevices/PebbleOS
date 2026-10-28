/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>
#include <string.h>

#include <pbl/drivers/display/sharp_ls013b7dh01/sharp_ls013b7dh01.h>
#include <pbl/drivers/gpio.h>
#include <pbl/drivers/gpio/nrf5.h>
#include <pbl/drivers/pwm.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/sem.h>
#include <pbl/util/bitops.h>

#include <board/board.h>
#include <kernel/events.h>
#include <nrfx_spim.h>

#define DISP_MODE_WRITE 0x01U
#define DISP_MODE_CLEAR 0x04U

static uint8_t s_buf[2 + ((DISP_LINE_BYTES + 2) * PBL_DISPLAY_HEIGHT)];
static bool s_updating;
static UpdateCompleteCallback s_uccb;
static PBL_SEM_DEFINE(s_sem, 0, 1);

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

static inline void prv_enable_spim(void) {
  nrf_spim_enable(BOARD_CONFIG_DISPLAY.spi.p_reg);
}

static inline void prv_disable_spim(void) {
  nrf_spim_disable(BOARD_CONFIG_DISPLAY.spi.p_reg);

  // Workaround for nRF52840 anomaly 195
  if (BOARD_CONFIG_DISPLAY.spi.p_reg == NRF_SPIM3) {
    *(volatile uint32_t *)0x4002F004 = 1;
  }
}

static inline void prv_enable_chip_select(void) {
  pbl_gpio_set(&BOARD_CONFIG_DISPLAY.cs, true);
}

static inline void prv_disable_chip_select(void) {
  pbl_gpio_set(&BOARD_CONFIG_DISPLAY.cs, false);
}

static void prv_terminate_transfer(void *data) {
  s_updating = false;

  prv_disable_chip_select();
  prv_disable_spim();

  s_uccb(true);
}

static void prv_spim_evt_handler(nrfx_spim_evt_t const *evt, void *ctx) {
  if (s_updating) {
    PebbleEvent e = {
      .type = PEBBLE_CALLBACK_EVENT,
      .callback = {
        .callback = prv_terminate_transfer,
      },
    };

    event_put_isr(&e);
  } else {
    pbl_sem_give(&s_sem);
  }
}

void display_init(void) {
  nrfx_spim_config_t config = NRFX_SPIM_DEFAULT_CONFIG(
      pbl_gpio_nrf5_pin(&BOARD_CONFIG_DISPLAY.clk), pbl_gpio_nrf5_pin(&BOARD_CONFIG_DISPLAY.mosi),
      NRF_SPIM_PIN_NOT_CONNECTED, NRF_SPIM_PIN_NOT_CONNECTED);
  config.frequency = NRFX_MHZ_TO_HZ(1);
  config.bit_order = NRF_SPIM_BIT_ORDER_LSB_FIRST;

  nrfx_err_t err =
      nrfx_spim_init(&BOARD_CONFIG_DISPLAY.spi, &config, prv_spim_evt_handler, nullptr);
  PBL_ASSERTN(err == NRFX_SUCCESS);

  pbl_gpio_configure(&BOARD_CONFIG_DISPLAY.cs, PBL_GPIO_OUTPUT);
  pbl_gpio_configure(&BOARD_CONFIG_DISPLAY.on_ctrl, PBL_GPIO_OUTPUT_ACTIVE);

  prv_extcomin_init();
}

void display_clear(void) {
  uint8_t buf[] = {DISP_MODE_CLEAR, 0x00};
  nrfx_spim_xfer_desc_t desc = {.p_tx_buffer = buf, .tx_length = sizeof(buf)};

  PBL_ASSERTN(!s_updating);

  prv_enable_spim();
  prv_enable_chip_select();

  nrfx_err_t err = nrfx_spim_xfer(&BOARD_CONFIG_DISPLAY.spi, &desc, 0);
  PBL_ASSERTN(err == NRFX_SUCCESS);
  pbl_sem_take(&s_sem, PBL_FOREVER);

  prv_disable_chip_select();
  prv_disable_spim();
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
  nrfx_spim_xfer_desc_t desc = {.p_tx_buffer = pbuf};

  PBL_ASSERTN(!s_updating);

  // write command (write)
  *pbuf++ = DISP_MODE_WRITE;
  desc.tx_length++;

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
    desc.tx_length += DISP_LINE_BYTES + 2;
  }

  // write last trailing dummy
  *pbuf++ = 0x00;
  desc.tx_length++;

  prv_enable_spim();
  prv_enable_chip_select();

  s_uccb = uccb;
  s_updating = true;

  nrfx_err_t err = nrfx_spim_xfer(&BOARD_CONFIG_DISPLAY.spi, &desc, 0);
  PBL_ASSERTN(err == NRFX_SUCCESS);
}

bool display_update_in_progress(void) {
  return s_updating;
}

/* stubs */

void display_update_boot_frame(uint8_t *framebuffer) {
}
