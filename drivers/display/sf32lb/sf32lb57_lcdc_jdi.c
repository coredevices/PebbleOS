/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "sf32lb57_lcdc_priv.h"

#include <errno.h>

#include <pbl/drivers/display/sf32lb57_lcdc.h>
#include <pbl/kernel/cache.h>
#include <pbl/kernel/delay.h>
#include <pbl/util/bits.h>
#include <pbl/util/math.h>

#include <hardware/sf32lb57_lcdc.h>

// XRST to the first VST, and the start of a transfer once enabled
#define LCDC_JDI_XRST_DELAY_US  35U
#define LCDC_JDI_START_DELAY_US 1U

static int prv_hw_init(const struct pbl_display_sf32lb57_lcdc *lcdc) {
  const struct pbl_panel_timing *timing = lcdc->panel->timing;
  uint32_t max_col = (timing->hback_porch + timing->hactive + timing->hfront_porch) / 2U;
  uint32_t max_line = (timing->vback_porch + timing->vactive + timing->vfront_porch) * 2U;
  uint64_t rate_mhz;
  uint32_t hck;
  uint32_t vck;
  int ret;

  ret = pbl_clk_get_rate(&lcdc->clk, &rate_mhz);
  if (ret != 0) {
    return ret;
  }

  hck = PBL_DIV_ROUND_UP((uint32_t)(rate_mhz / 1000U), timing->clock_hz) / 2U;
  vck = hck * max_col;

  if (hck == 0U || vck > PBL_FIELD_GET(SF32LB57_LCDC_JDI_PAR_CONF5_VCK_WIDTH_MASK, UINT32_MAX)) {
    return -EINVAL;
  }

  ret = pbl_display_sf32lb57_lcdc__hw_enable(lcdc);
  if (ret != 0) {
    return ret;
  }

  lcdc->state->max_line = max_line;

  prv_modify(lcdc, SF32LB57_LCDC_LCD_CONF_OFFSET,
             SF32LB57_LCDC_LCD_CONF_TARGET_LCD_MASK | SF32LB57_LCDC_LCD_CONF_LCD_INTF_SEL_MASK |
                 SF32LB57_LCDC_LCD_CONF_LCD_FORMAT_MASK | SF32LB57_LCDC_LCD_CONF_AHB_FORMAT_MASK |
                 SF32LB57_LCDC_LCD_CONF_SPI_LCD_FORMAT_MASK |
                 SF32LB57_LCDC_LCD_CONF_DPI_LCD_FORMAT_MASK |
                 SF32LB57_LCDC_LCD_CONF_JDI_SER_FORMAT_MASK | SF32LB57_LCDC_LCD_CONF_ENDIAN,
             PBL_FIELD_PREP(SF32LB57_LCDC_LCD_CONF_LCD_INTF_SEL_MASK,
                            SF32LB57_LCDC_LCD_CONF_LCD_INTF_SEL_JDI_PAR) |
                 PBL_FIELD_PREP(SF32LB57_LCDC_LCD_CONF_LCD_FORMAT_MASK,
                                SF32LB57_LCDC_LCD_CONF_LCD_FORMAT_RGB332) |
                 PBL_FIELD_PREP(SF32LB57_LCDC_LCD_CONF_AHB_FORMAT_MASK,
                                SF32LB57_LCDC_LCD_CONF_AHB_FORMAT_RGB332));

  // The JDI timings count the controller clock, undivided
  prv_modify(lcdc, SF32LB57_LCDC_LCD_CONF_OFFSET,
             SF32LB57_LCDC_LCD_CONF_INTF_CLK_DIV_MASK | SF32LB57_LCDC_LCD_CONF_INTF_CLK_DIV2P5_SEL,
             PBL_FIELD_PREP(SF32LB57_LCDC_LCD_CONF_INTF_CLK_DIV_MASK, 1U) |
                 SF32LB57_LCDC_LCD_CONF_INTF_CLK_DIV_UPDATE);

  prv_write(lcdc, SF32LB57_LCDC_JDI_PAR_CONF1_OFFSET,
            PBL_FIELD_PREP(SF32LB57_LCDC_JDI_PAR_CONF1_MAX_COL_MASK, max_col - 1U) |
                PBL_FIELD_PREP(SF32LB57_LCDC_JDI_PAR_CONF1_MAX_LINE_MASK, max_line - 1U));
  prv_write(lcdc, SF32LB57_LCDC_JDI_PAR_CONF4_OFFSET,
            PBL_FIELD_PREP(SF32LB57_LCDC_JDI_PAR_CONF4_HST_WIDTH_MASK, hck) |
                PBL_FIELD_PREP(SF32LB57_LCDC_JDI_PAR_CONF4_HCK_WIDTH_MASK, hck));
  prv_write(lcdc, SF32LB57_LCDC_JDI_PAR_CONF5_OFFSET,
            PBL_FIELD_PREP(SF32LB57_LCDC_JDI_PAR_CONF5_VST_WIDTH_MASK, vck) |
                PBL_FIELD_PREP(SF32LB57_LCDC_JDI_PAR_CONF5_VCK_WIDTH_MASK, vck));
  prv_write(lcdc, SF32LB57_LCDC_JDI_PAR_CONF6_OFFSET,
            PBL_FIELD_PREP(SF32LB57_LCDC_JDI_PAR_CONF6_HST_DLY_MASK, hck) |
                PBL_FIELD_PREP(SF32LB57_LCDC_JDI_PAR_CONF6_VCK_DLY_MASK, vck / 2U));
  prv_write(lcdc, SF32LB57_LCDC_JDI_PAR_CONF7_OFFSET,
            PBL_FIELD_PREP(SF32LB57_LCDC_JDI_PAR_CONF7_HCK_DLY_MASK, hck / 2U) |
                SF32LB57_LCDC_JDI_PAR_CONF7_DP_MODE);
  prv_write(lcdc, SF32LB57_LCDC_JDI_PAR_CONF8_OFFSET,
            PBL_FIELD_PREP(SF32LB57_LCDC_JDI_PAR_CONF8_ENB_ST_COL_MASK, timing->enable_start) |
                PBL_FIELD_PREP(SF32LB57_LCDC_JDI_PAR_CONF8_ENB_END_COL_MASK, timing->enable_end));
  prv_write(lcdc, SF32LB57_LCDC_JDI_PAR_CTRL_OFFSET, 0U);

  prv_modify(lcdc, SF32LB57_LCDC_TE_CONF_OFFSET, SF32LB57_LCDC_TE_CONF_ENABLE, 0U);
  prv_modify(lcdc, SF32LB57_LCDC_LCD_IF_CONF_OFFSET, 0U, SF32LB57_LCDC_LCD_IF_CONF_LCD_RSTB);

  return 0;
}

// The rows from the canvas to the layer, and the JDI scan of each, which takes two
static int prv_update_start(const struct pbl_display_sf32lb57_lcdc *lcdc) {
  struct pbl_display_sf32lb57_lcdc_state *state = lcdc->state;
  const struct pbl_panel_timing *timing = lcdc->panel->timing;
  uint16_t width = lcdc->disp.props.width;
  uint16_t height = lcdc->disp.props.height;
  uint8_t *first = &state->fb[state->y0 * state->pitch];
  uint32_t src = (uint32_t)(uintptr_t)first;
  uint16_t y0 = state->y0;
  uint16_t y1 = state->y1;
  uint32_t layer;

  pbl_dcache_flush_range(first, (size_t)(y1 - y0 + 1U) * state->pitch);

  if (state->frame_rotated) {
    y0 = height - 1U - state->y1;
    y1 = height - 1U - state->y0;
  }

  if (pbl_display_sf32lb57_lcdc__wait_idle(lcdc) != 0) {
    return -EBUSY;
  }

  uint32_t y1_doubled = y0 + (y1 - y0 + 1U) * 2U - 1U;

  // No line buffers in SRAM: the canvas would wait for them forever
  prv_write(lcdc, SF32LB57_LCDC_CANVAS_BG_OFFSET, SF32LB57_LCDC_CANVAS_BG_LB_BYPASS);
  prv_write(
      lcdc, SF32LB57_LCDC_CANVAS_TL_POS_OFFSET,
      PBL_FIELD_PREP(SF32LB57_LCDC_POS_X_MASK, 0U) | PBL_FIELD_PREP(SF32LB57_LCDC_POS_Y_MASK, y0));
  prv_write(lcdc, SF32LB57_LCDC_CANVAS_BR_POS_OFFSET,
            PBL_FIELD_PREP(SF32LB57_LCDC_POS_X_MASK, width - 1U) |
                PBL_FIELD_PREP(SF32LB57_LCDC_POS_Y_MASK, y1_doubled));

  layer = PBL_FIELD_PREP(SF32LB57_LCDC_LAYER_CONFIG_FORMAT_MASK,
                         SF32LB57_LCDC_LAYER_CONFIG_FORMAT_RGB332) |
          PBL_FIELD_PREP(SF32LB57_LCDC_LAYER_CONFIG_ALPHA_MASK, 255U) |
          PBL_FIELD_PREP(SF32LB57_LCDC_LAYER_CONFIG_WIDTH_MASK, width) |
          SF32LB57_LCDC_LAYER_CONFIG_ACTIVE | SF32LB57_LCDC_LAYER_CONFIG_LINE_FETCH_MODE;
  if (state->frame_rotated) {
    // Read from the last line up
    layer |= SF32LB57_LCDC_LAYER_CONFIG_V_MIRROR;
    src += state->pitch * (uint32_t)(y1 - y0);
  }

  prv_modify(lcdc, SF32LB57_LCDC_LAYER1_CONFIG_OFFSET, SF32LB57_LCDC_LAYER_CONFIG_ACTIVE, 0U);
  prv_write(lcdc, SF32LB57_LCDC_LAYER0_CONFIG_OFFSET, layer);
  prv_write(
      lcdc, SF32LB57_LCDC_LAYER0_TL_POS_OFFSET,
      PBL_FIELD_PREP(SF32LB57_LCDC_POS_X_MASK, 0U) | PBL_FIELD_PREP(SF32LB57_LCDC_POS_Y_MASK, y0));
  prv_write(lcdc, SF32LB57_LCDC_LAYER0_BR_POS_OFFSET,
            PBL_FIELD_PREP(SF32LB57_LCDC_POS_X_MASK, width - 1U) |
                PBL_FIELD_PREP(SF32LB57_LCDC_POS_Y_MASK, y1_doubled));
  prv_write(lcdc, SF32LB57_LCDC_LAYER0_SRC_OFFSET, src);
  prv_write(lcdc, SF32LB57_LCDC_LAYER0_FILTER_OFFSET, 0U);
  prv_write(lcdc, SF32LB57_LCDC_LAYER0_FILL_OFFSET, 0U);

  uint32_t start_line = (timing->vback_porch + y0) * 2U + 1U;
  uint32_t end_line = (timing->vback_porch + y1 + 1U) * 2U;
  uint32_t start_col = timing->hback_porch / 2U;
  uint32_t end_col = (timing->hback_porch + width) / 2U - 1U;

  prv_write(lcdc, SF32LB57_LCDC_JDI_PAR_CONF2_OFFSET,
            PBL_FIELD_PREP(SF32LB57_LCDC_JDI_PAR_CONF2_ST_LINE_MASK, start_line) |
                PBL_FIELD_PREP(SF32LB57_LCDC_JDI_PAR_CONF2_END_LINE_MASK, end_line));
  prv_write(lcdc, SF32LB57_LCDC_JDI_PAR_CONF9_OFFSET,
            PBL_FIELD_PREP(SF32LB57_LCDC_JDI_PAR_CONF9_ENB_ST_LINE_MASK, start_line + 1U) |
                PBL_FIELD_PREP(SF32LB57_LCDC_JDI_PAR_CONF9_ENB_END_LINE_MASK, end_line + 1U));
  prv_write(lcdc, SF32LB57_LCDC_JDI_PAR_CONF3_OFFSET,
            PBL_FIELD_PREP(SF32LB57_LCDC_JDI_PAR_CONF3_ST_COL_MASK, start_col) |
                PBL_FIELD_PREP(SF32LB57_LCDC_JDI_PAR_CONF3_END_COL_MASK, end_col));
  prv_write(lcdc, SF32LB57_LCDC_JDI_PAR_CONF10_OFFSET,
            PBL_FIELD_PREP(SF32LB57_LCDC_JDI_PAR_CONF10_HC_ST_LINE_MASK, start_line) |
                PBL_FIELD_PREP(SF32LB57_LCDC_JDI_PAR_CONF10_HC_END_LINE_MASK, end_line + 2U));

  // First interrupt halfway through the frame
  prv_modify(
      lcdc, SF32LB57_LCDC_JDI_PAR_CTRL_OFFSET, SF32LB57_LCDC_JDI_PAR_CTRL_INT_LINE_NUM_MASK,
      PBL_FIELD_PREP(SF32LB57_LCDC_JDI_PAR_CTRL_INT_LINE_NUM_MASK, state->max_line / 2U - 1U));
  prv_modify(lcdc, SF32LB57_LCDC_SETTING_OFFSET, 0U,
             SF32LB57_LCDC_SETTING_JDI_PARL_INTR_MASK | SF32LB57_LCDC_SETTING_EOF_MASK);

  prv_modify(lcdc, SF32LB57_LCDC_JDI_PAR_CTRL_OFFSET, 0U, SF32LB57_LCDC_JDI_PAR_CTRL_XRST);
  pbl_busy_wait_us(LCDC_JDI_XRST_DELAY_US);
  prv_modify(lcdc, SF32LB57_LCDC_JDI_PAR_CTRL_OFFSET, 0U, SF32LB57_LCDC_JDI_PAR_CTRL_ENABLE);
  pbl_busy_wait_us(LCDC_JDI_START_DELAY_US);

  return 0;
}

// Halfway through a JDI frame, interrupts again at its end and sends it only once; at the end,
// stops the scan. Returns true at the end.
static bool prv_irq(const struct pbl_display_sf32lb57_lcdc *lcdc, uint32_t irq) {
  if ((irq & SF32LB57_LCDC_IRQ_JDI_PAR_UDR_RAW_STAT) != 0U) {
    lcdc->state->status = -EIO;
  }

  if ((irq & SF32LB57_LCDC_IRQ_JDI_PARL_INTR_RAW_STAT) == 0U) {
    return true;
  }

  uint32_t ctrl = prv_read(lcdc, SF32LB57_LCDC_JDI_PAR_CTRL_OFFSET);
  uint32_t max_line = PBL_FIELD_GET(SF32LB57_LCDC_JDI_PAR_CONF1_MAX_LINE_MASK,
                                    prv_read(lcdc, SF32LB57_LCDC_JDI_PAR_CONF1_OFFSET));

  if (PBL_FIELD_GET(SF32LB57_LCDC_JDI_PAR_CTRL_INT_LINE_NUM_MASK, ctrl) != max_line) {
    ctrl &= ~(SF32LB57_LCDC_JDI_PAR_CTRL_INT_LINE_NUM_MASK | SF32LB57_LCDC_JDI_PAR_CTRL_ENABLE);
    ctrl |= PBL_FIELD_PREP(SF32LB57_LCDC_JDI_PAR_CTRL_INT_LINE_NUM_MASK, max_line);
    prv_write(lcdc, SF32LB57_LCDC_JDI_PAR_CTRL_OFFSET, ctrl);
    return false;
  }

  prv_modify(lcdc, SF32LB57_LCDC_SETTING_OFFSET, SF32LB57_LCDC_SETTING_JDI_PARL_INTR_MASK, 0U);
  prv_modify(lcdc, SF32LB57_LCDC_JDI_PAR_CTRL_OFFSET, SF32LB57_LCDC_JDI_PAR_CTRL_XRST, 0U);
  prv_modify(lcdc, SF32LB57_LCDC_JDI_PAR_EX_CTRL_OFFSET, 0U, SF32LB57_LCDC_JDI_PAR_EX_CTRL_CNT_EN);

  return true;
}

const struct pbl_display_sf32lb57_lcdc__intf pbl_display_sf32lb57_lcdc__jdi = {
  .bus_format = PBL_PANEL_BUS_FORMAT_JDI_RGB222,
  .hw_init = prv_hw_init,
  .update_start = prv_update_start,
  .irq = prv_irq,
};
