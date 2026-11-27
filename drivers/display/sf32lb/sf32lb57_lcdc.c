/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "sf32lb57_lcdc_priv.h"

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/display/sf32lb57_lcdc.h>
#include <pbl/kernel/compiler.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/idle.h>
#include <pbl/kernel/irq.h>
#include <pbl/logging/logging.h>
#include <pbl/soc/sf32lb/idle.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>

#include <hardware/sf32lb57_lcdc.h>

PBL_LOG_MODULE_DECLARE(driver_display, CONFIG_DRIVER_DISPLAY_LOG_LEVEL);

#define LCDC_BUSY_TIMEOUT_US  100000U
#define LCDC_FRAME_TIMEOUT_MS 200U

static const struct pbl_display_sf32lb57_lcdc *prv_lcdc(const struct pbl_display *disp) {
  return PBL_CONTAINER_OF(disp, const struct pbl_display_sf32lb57_lcdc, disp);
}

int pbl_display_sf32lb57_lcdc__wait_idle(const struct pbl_display_sf32lb57_lcdc *lcdc) {
  return PBL_POLL_BUSY_US(
      (prv_read(lcdc, SF32LB57_LCDC_STATUS_OFFSET) & SF32LB57_LCDC_STATUS_LCD_BUSY) == 0U &&
          (prv_read(lcdc, SF32LB57_LCDC_LCD_SINGLE_OFFSET) & SF32LB57_LCDC_LCD_SINGLE_LCD_BUSY) ==
              0U,
      1U, LCDC_BUSY_TIMEOUT_US);
}

int pbl_display_sf32lb57_lcdc__hw_enable(const struct pbl_display_sf32lb57_lcdc *lcdc) {
  int ret;

  ret = pbl_clk_enable(&lcdc->clk);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_reset_reset(&lcdc->rst);
  if (ret != 0) {
    return ret;
  }

  prv_modify(lcdc, SF32LB57_LCDC_SETTING_OFFSET, 0U, SF32LB57_LCDC_SETTING_AUTO_GATE_EN);

  return 0;
}

// Commands wait for the frame being sent, and keep the next one from starting
int pbl_display_sf32lb57_lcdc__bus_lock(const struct pbl_display_sf32lb57_lcdc *lcdc) {
  struct pbl_display_sf32lb57_lcdc_state *state = lcdc->state;
  int ret;

  (void)pbl_mutex_lock(&state->lock, PBL_FOREVER);

  ret = PBL_POLL_SLEEP_MS(!state->frame, 1U, LCDC_FRAME_TIMEOUT_MS);
  if (ret != 0) {
    pbl_mutex_unlock(&state->lock);
    return -EBUSY;
  }

  return 0;
}

static int prv_panel_on(const struct pbl_display_sf32lb57_lcdc *lcdc) {
  int ret;

  ret = pbl_panel_prepare(lcdc->panel);
  if (ret != 0) {
    return ret;
  }

  return pbl_panel_enable(lcdc->panel);
}

static int prv_panel_off(const struct pbl_display_sf32lb57_lcdc *lcdc) {
  int ret;

  (void)pbl_display_sf32lb57_lcdc__wait_idle(lcdc);

  ret = pbl_panel_disable(lcdc->panel);
  if (ret != 0) {
    return ret;
  }

  return pbl_panel_unprepare(lcdc->panel);
}

static uint16_t prv_row_multiple(const struct pbl_display_sf32lb57_lcdc *lcdc) {
  return PBL_MAX(lcdc->panel->row_multiple, 1U);
}

// 2222: AA RR GG BB, 332: RR R GG G BB, the top bit of red and green repeated for full scale
static PBL_ALWAYS_INLINE uint8_t prv_to_332(uint8_t p) {
  return ((p & 0x30U) << 2) | (p & 0x20U) | ((p & 0x0CU) << 1) | ((p & 0x08U) >> 1) | (p & 0x03U);
}

static PBL_ALWAYS_INLINE uint8_t prv_to_2222(uint8_t p) {
  return ((p >> 2) & 0x30U) | ((p >> 1) & 0x0CU) | (p & 0x03U);
}

static void prv_row_mirror_565(uint16_t *row, uint16_t width) {
  for (uint16_t i = 0U, j = width - 1U; i < j; i++, j--) {
    uint16_t a = row[i];

    row[i] = row[j];
    row[j] = a;
  }
}

// RGB565 rows are sent as they are, unless mirrored
static bool prv_rows_changed(const struct pbl_display_sf32lb57_lcdc *lcdc) {
  return !prv_rgb565(lcdc) || lcdc->state->frame_mirrored;
}

// A turned image is read from its last line up, and flipped left to right by the panel or,
// where it cannot, here, before an update and back after it.
static void prv_rows_convert(const struct pbl_display_sf32lb57_lcdc *lcdc, bool to_lcdc) {
  struct pbl_display_sf32lb57_lcdc_state *state = lcdc->state;
  uint16_t width = lcdc->disp.props.width;
  uint8_t (*conv)(uint8_t) = to_lcdc ? prv_to_332 : prv_to_2222;

  if (!prv_rows_changed(lcdc)) {
    return;
  }

  for (uint16_t y = state->y0; y <= state->y1; y++) {
    uint8_t *row = &state->fb[y * state->pitch];

    if (prv_rgb565(lcdc)) {
      prv_row_mirror_565((uint16_t *)row, width);
      continue;
    }

    if (!state->frame_mirrored) {
      for (uint16_t x = 0U; x < width; x++) {
        row[x] = conv(row[x]);
      }
      continue;
    }

    for (uint16_t i = 0U, j = width - 1U; i <= j && j < width; i++, j--) {
      uint8_t a = row[i];

      row[i] = conv(row[j]);
      row[j] = conv(a);
    }
  }
}

static void prv_update_end(const struct pbl_display_sf32lb57_lcdc *lcdc, int status) {
  prv_rows_convert(lcdc, false);
  lcdc->state->frame = false;
  pbl_idle_state_release(SOC_SF32LB_IDLE_DEEPWFI);
  pbl_display_complete(&lcdc->disp, status);
}

static void prv_end_work(struct pbl_work *work) {
  struct pbl_display_sf32lb57_lcdc_state *state =
      PBL_CONTAINER_OF(work, struct pbl_display_sf32lb57_lcdc_state, work);

  prv_update_end(state->lcdc, state->status);
}

void pbl_display_sf32lb57_lcdc_irq_handler(const struct pbl_display_sf32lb57_lcdc *lcdc) {
  struct pbl_display_sf32lb57_lcdc_state *state = lcdc->state;
  uint32_t irq = prv_read(lcdc, SF32LB57_LCDC_IRQ_OFFSET);

  prv_write(lcdc, SF32LB57_LCDC_IRQ_OFFSET, irq);
  if (!lcdc->intf->irq(lcdc, irq) || (irq & SF32LB57_LCDC_IRQ_EOF_STAT) == 0U) {
    return;
  }

  prv_modify(lcdc, SF32LB57_LCDC_SETTING_OFFSET, SF32LB57_LCDC_SETTING_EOF_MASK, 0U);

  // Rows changed for the update are put back out of the interrupt
  if (prv_rows_changed(lcdc)) {
    (void)pbl_work_submit_boosted(&pbl_sys_workq, &state->work);
    return;
  }

  prv_update_end(lcdc, state->status);
}

static int prv_init(const struct pbl_display *disp) {
  const struct pbl_display_sf32lb57_lcdc *lcdc = prv_lcdc(disp);
  const struct pbl_panel_timing *timing = lcdc->panel->timing;
  int ret;

  if (pbl_device_init(&lcdc->clk.ctrl->dev) != 0 || pbl_device_init(&lcdc->rst.ctrl->dev) != 0 ||
      pbl_device_init(&lcdc->panel->dev) != 0) {
    return -ENODEV;
  }

  if (timing->hactive != disp->props.width || timing->vactive != disp->props.height ||
      (disp->props.height % prv_row_multiple(lcdc)) != 0U) {
    return -EINVAL;
  }

  if (lcdc->panel->bus_format != lcdc->intf->bus_format) {
    return -EINVAL;
  }

  lcdc->state->lcdc = lcdc;
  pbl_work_init(&lcdc->state->work, prv_end_work);

  // Power cycle the panel with its inputs as the sleep state leaves them, e.g. driven low
  ret = pbl_pinctrl_select_state(&lcdc->pinctrl, PBL_PINCTRL_STATE_SLEEP);
  if (ret != 0 && ret != -ENOENT) {
    return ret;
  }

  ret = pbl_panel_unprepare(lcdc->panel);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_pinctrl_select_state(&lcdc->pinctrl, PBL_PINCTRL_STATE_DEFAULT);
  if (ret != 0) {
    return ret;
  }

  ret = lcdc->intf->hw_init(lcdc);
  if (ret != 0) {
    return ret;
  }

  pbl_irq_enable(lcdc->irq);

  return prv_panel_on(lcdc);
}

static int prv_enable(const struct pbl_display *disp) {
  return prv_panel_on(prv_lcdc(disp));
}

static int prv_disable(const struct pbl_display *disp) {
  return prv_panel_off(prv_lcdc(disp));
}

static int prv_set_rotated(const struct pbl_display *disp, bool rotated) {
  const struct pbl_display_sf32lb57_lcdc *lcdc = prv_lcdc(disp);
  struct pbl_display_sf32lb57_lcdc_state *state = lcdc->state;
  int ret;

  ret = pbl_panel_set_rotation(lcdc->panel,
                               PBL_PANEL_ROTATE_0 | (rotated ? PBL_PANEL_REFLECT_X : 0U));
  if (ret != 0 && ret != -ENOTSUP) {
    return ret;
  }

  state->rotated = rotated;
  state->mirrored = rotated && ret == -ENOTSUP;

  return 0;
}

static int prv_update(const struct pbl_display *disp, const struct pbl_display_buffer *buf,
                      uint16_t y0, uint16_t y1) {
  const struct pbl_display_sf32lb57_lcdc *lcdc = prv_lcdc(disp);
  struct pbl_display_sf32lb57_lcdc_state *state = lcdc->state;
  uint16_t rows = prv_row_multiple(lcdc);
  int ret;

  if (buf->pitch != disp->props.width * (prv_rgb565(lcdc) ? 2U : 1U)) {
    return -EINVAL;
  }

  ret = pbl_display_sf32lb57_lcdc__bus_lock(lcdc);
  if (ret != 0) {
    return ret;
  }

  // Whole multiples of the rows the panel takes
  state->fb = buf->data;
  state->pitch = buf->pitch;
  state->y0 = y0 - (y0 % rows);
  state->y1 = y1 + (rows - 1U - (y1 % rows));
  state->frame_rotated = state->rotated;
  state->frame_mirrored = state->mirrored;

  // Converted in place, to save a framebuffer of RAM
  prv_rows_convert(lcdc, true);

  pbl_idle_state_block(SOC_SF32LB_IDLE_DEEPWFI);
  state->status = 0;
  state->frame = true;

  ret = lcdc->intf->update_start(lcdc);
  if (ret != 0) {
    PBL_LOG_ERR("update start failed (%d)", ret);
    state->frame = false;
    prv_rows_convert(lcdc, false);
    pbl_idle_state_release(SOC_SF32LB_IDLE_DEEPWFI);
  }

  pbl_mutex_unlock(&state->lock);

  return ret;
}

static const struct pbl_display_sf32lb57_lcdc *prv_dbi_lcdc(const struct pbl_mipi_dbi *dbi) {
  return PBL_CONTAINER_OF(dbi, const struct pbl_display_sf32lb57_lcdc_dbi, dbi)->lcdc;
}

static int prv_dbi_write(const struct pbl_mipi_dbi *dbi, uint8_t cmd, const uint8_t *data,
                         size_t len) {
  const struct pbl_display_sf32lb57_lcdc *lcdc = prv_dbi_lcdc(dbi);

  if (lcdc->intf->command_write == nullptr) {
    return -ENOTSUP;
  }

  return lcdc->intf->command_write(lcdc, cmd, data, len);
}

static int prv_dbi_read(const struct pbl_mipi_dbi *dbi, uint8_t cmd, uint8_t *data, size_t len) {
  const struct pbl_display_sf32lb57_lcdc *lcdc = prv_dbi_lcdc(dbi);

  if (lcdc->intf->command_read == nullptr) {
    return -ENOTSUP;
  }

  return lcdc->intf->command_read(lcdc, cmd, data, len);
}

const struct pbl_mipi_dbi_ops pbl_display_sf32lb57_lcdc_dbi_ops = {
  .command_write = prv_dbi_write,
  .command_read = prv_dbi_read,
};

const struct pbl_display_ops pbl_display_sf32lb57_lcdc_ops = {
  .init = prv_init,
  .enable = prv_enable,
  .disable = prv_disable,
  .set_rotated = prv_set_rotated,
  .update = prv_update,
};

#include PBL_DT_INSTANCES(sifli_sf32lb57_lcdc)
