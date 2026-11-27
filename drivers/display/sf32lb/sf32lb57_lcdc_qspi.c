/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "sf32lb57_lcdc_priv.h"

#include <errno.h>

#include <pbl/drivers/display/sf32lb57_lcdc.h>
#include <pbl/kernel/cache.h>
#include <pbl/util/bits.h>
#include <pbl/util/byteorder.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>

#include <hardware/sf32lb57_lcdc.h>

// Fastest clock panel registers are read back at
#define LCDC_READ_CLOCK_HZ 2000000U

#define LCDC_INTF_CLK_DIV_MAX 15U
#define LCDC_SDR_DIV_MAX      255U

// Quad SPI opcodes, followed by a 24-bit address holding the command in its middle byte
#define QSPI_OP_WRITE_CMD    0x02U
#define QSPI_OP_WRITE_PIXELS 0x32U
#define QSPI_OP_READ         0x03U

// Total divider from the controller clock to the SPI clock
static int prv_clock_div(const struct pbl_display_sf32lb57_lcdc *lcdc, uint32_t clock_hz,
                         uint32_t *div) {
  uint64_t rate_mhz;
  uint32_t rate;
  int ret;

  ret = pbl_clk_get_rate(&lcdc->clk, &rate_mhz);
  if (ret != 0) {
    return ret;
  }

  rate = (uint32_t)(rate_mhz / 1000U);
  *div = PBL_DIV_ROUND_UP(rate, clock_hz);
  if (*div == 0U || *div > LCDC_INTF_CLK_DIV_MAX * LCDC_SDR_DIV_MAX) {
    return -EINVAL;
  }

  return 0;
}

// The interface and SPI dividers with the smallest product not under the total, the interface
// one as large as it can be
static void prv_set_div(const struct pbl_display_sf32lb57_lcdc *lcdc, uint32_t div) {
  uint32_t intf = PBL_MIN(div, LCDC_INTF_CLK_DIV_MAX);
  uint32_t sdr = PBL_DIV_ROUND_UP(div, intf);

  for (uint32_t i = intf - 1U; i >= 2U && intf * sdr > div; i--) {
    uint32_t s = PBL_DIV_ROUND_UP(div, i);

    if (s <= LCDC_SDR_DIV_MAX && i * s < intf * sdr) {
      intf = i;
      sdr = s;
    }
  }

  prv_modify(lcdc, SF32LB57_LCDC_LCD_CONF_OFFSET,
             SF32LB57_LCDC_LCD_CONF_INTF_CLK_DIV_MASK | SF32LB57_LCDC_LCD_CONF_INTF_CLK_DIV2P5_SEL,
             PBL_FIELD_PREP(SF32LB57_LCDC_LCD_CONF_INTF_CLK_DIV_MASK, intf) |
                 SF32LB57_LCDC_LCD_CONF_INTF_CLK_DIV_UPDATE);
  prv_modify(lcdc, SF32LB57_LCDC_SPI_IF_CONF_EXT_OFFSET,
             SF32LB57_LCDC_SPI_IF_CONF_EXT_CLK_SDR_DIV_MASK,
             PBL_FIELD_PREP(SF32LB57_LCDC_SPI_IF_CONF_EXT_CLK_SDR_DIV_MASK, sdr));
}

static void prv_set_cs_release(const struct pbl_display_sf32lb57_lcdc *lcdc, bool release) {
  prv_modify(lcdc, SF32LB57_LCDC_SPI_IF_CONF_OFFSET, SF32LB57_LCDC_SPI_IF_CONF_SPI_CS_AUTO_DIS,
             release ? SF32LB57_LCDC_SPI_IF_CONF_SPI_CS_AUTO_DIS : 0U);
}

// One transfer of 1 to 4 bytes, the first in the most significant byte sent
static int prv_single_write(const struct pbl_display_sf32lb57_lcdc *lcdc, uint32_t value,
                            uint32_t len, bool data, bool release) {
  int ret;

  ret = pbl_display_sf32lb57_lcdc__wait_idle(lcdc);
  if (ret != 0) {
    return ret;
  }

  prv_set_cs_release(lcdc, release);
  prv_modify(lcdc, SF32LB57_LCDC_SPI_IF_CONF_OFFSET,
             SF32LB57_LCDC_SPI_IF_CONF_RD_LEN_MASK | SF32LB57_LCDC_SPI_IF_CONF_WR_LEN_MASK |
                 SF32LB57_LCDC_SPI_IF_CONF_SPI_RD_MODE,
             PBL_FIELD_PREP(SF32LB57_LCDC_SPI_IF_CONF_WR_LEN_MASK, len - 1U));
  prv_write(lcdc, SF32LB57_LCDC_LCD_WR_OFFSET, value);
  prv_write(lcdc, SF32LB57_LCDC_LCD_SINGLE_OFFSET,
            SF32LB57_LCDC_LCD_SINGLE_WR_TRIG | (data ? SF32LB57_LCDC_LCD_SINGLE_TYPE : 0U));

  return 0;
}

static uint32_t prv_qspi_addr(uint8_t op, uint8_t cmd) {
  return ((uint32_t)op << 24) | ((uint32_t)cmd << 8);
}

static int prv_command_write(const struct pbl_display_sf32lb57_lcdc *lcdc, uint8_t op, uint8_t cmd,
                             const uint8_t *data, size_t len, bool release) {
  int ret;

  ret = prv_single_write(lcdc, prv_qspi_addr(op, cmd), 4U, false, release && len == 0U);
  if (ret != 0) {
    return ret;
  }

  while (len > 0U) {
    uint32_t chunk = PBL_MIN(len, 4U);
    uint32_t value = 0U;

    for (uint32_t i = 0U; i < chunk; i++) {
      value = (value << 8) | data[i];
    }

    data += chunk;
    len -= chunk;

    ret = prv_single_write(lcdc, value, chunk, true, release && len == 0U);
    if (ret != 0) {
      return ret;
    }
  }

  return pbl_display_sf32lb57_lcdc__wait_idle(lcdc);
}

static int prv_command_send(const struct pbl_display_sf32lb57_lcdc *lcdc, uint8_t cmd,
                            const uint8_t *data, size_t len) {
  int ret;

  ret = pbl_display_sf32lb57_lcdc__bus_lock(lcdc);
  if (ret != 0) {
    return ret;
  }

  ret = prv_command_write(lcdc, QSPI_OP_WRITE_CMD, cmd, data, len, true);

  pbl_mutex_unlock(&lcdc->state->lock);

  return ret;
}

static int prv_command_read(const struct pbl_display_sf32lb57_lcdc *lcdc, uint8_t cmd,
                            uint8_t *data, size_t len) {
  uint32_t div;
  uint32_t value;
  int ret;

  if (len == 0U || len > 4U) {
    return -EINVAL;
  }

  ret = prv_clock_div(lcdc, LCDC_READ_CLOCK_HZ, &div);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_display_sf32lb57_lcdc__bus_lock(lcdc);
  if (ret != 0) {
    return ret;
  }

  prv_set_div(lcdc, div);

  // The command, then the read in the same CS frame
  ret = prv_command_write(lcdc, QSPI_OP_READ, cmd, nullptr, 0U, false);
  if (ret != 0) {
    goto out;
  }

  prv_set_cs_release(lcdc, true);
  prv_modify(lcdc, SF32LB57_LCDC_SPI_IF_CONF_OFFSET,
             SF32LB57_LCDC_SPI_IF_CONF_RD_LEN_MASK | SF32LB57_LCDC_SPI_IF_CONF_WR_LEN_MASK,
             PBL_FIELD_PREP(SF32LB57_LCDC_SPI_IF_CONF_RD_LEN_MASK, len - 1U) |
                 SF32LB57_LCDC_SPI_IF_CONF_SPI_RD_MODE);
  prv_write(lcdc, SF32LB57_LCDC_LCD_SINGLE_OFFSET,
            SF32LB57_LCDC_LCD_SINGLE_RD_TYPE | SF32LB57_LCDC_LCD_SINGLE_TYPE |
                SF32LB57_LCDC_LCD_SINGLE_RD_TRIG);

  ret = pbl_display_sf32lb57_lcdc__wait_idle(lcdc);
  if (ret != 0) {
    goto out;
  }

  value = prv_read(lcdc, SF32LB57_LCDC_LCD_RD_OFFSET);
  for (size_t i = 0U; i < len; i++) {
    data[i] = (uint8_t)(value >> (8U * i));
  }

out:
  prv_modify(lcdc, SF32LB57_LCDC_SPI_IF_CONF_OFFSET, SF32LB57_LCDC_SPI_IF_CONF_SPI_RD_MODE, 0U);
  prv_set_div(lcdc, lcdc->state->write_div);
  pbl_mutex_unlock(&lcdc->state->lock);

  return ret;
}

static int prv_hw_init(const struct pbl_display_sf32lb57_lcdc *lcdc) {
  int ret;

  if (lcdc->panel->bus_max_hz == 0U) {
    return -EINVAL;
  }

  ret = prv_clock_div(lcdc, lcdc->panel->bus_max_hz, &lcdc->state->write_div);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_display_sf32lb57_lcdc__hw_enable(lcdc);
  if (ret != 0) {
    return ret;
  }

  prv_modify(lcdc, SF32LB57_LCDC_LCD_CONF_OFFSET,
             SF32LB57_LCDC_LCD_CONF_TARGET_LCD_MASK | SF32LB57_LCDC_LCD_CONF_LCD_INTF_SEL_MASK |
                 SF32LB57_LCDC_LCD_CONF_LCD_FORMAT_MASK | SF32LB57_LCDC_LCD_CONF_AHB_FORMAT_MASK |
                 SF32LB57_LCDC_LCD_CONF_SPI_LCD_FORMAT_MASK |
                 SF32LB57_LCDC_LCD_CONF_DPI_LCD_FORMAT_MASK |
                 SF32LB57_LCDC_LCD_CONF_JDI_SER_FORMAT_MASK | SF32LB57_LCDC_LCD_CONF_ENDIAN,
             PBL_FIELD_PREP(SF32LB57_LCDC_LCD_CONF_LCD_INTF_SEL_MASK,
                            SF32LB57_LCDC_LCD_CONF_LCD_INTF_SEL_SPI) |
                 PBL_FIELD_PREP(SF32LB57_LCDC_LCD_CONF_LCD_FORMAT_MASK,
                                SF32LB57_LCDC_LCD_CONF_LCD_FORMAT_RGB565) |
                 PBL_FIELD_PREP(SF32LB57_LCDC_LCD_CONF_SPI_LCD_FORMAT_MASK,
                                SF32LB57_LCDC_LCD_CONF_SPI_LCD_FORMAT_RGB565));

  // Pixels on four lines, CS and the clock released between transfers
  prv_write(lcdc, SF32LB57_LCDC_SPI_IF_CONF_OFFSET,
            PBL_FIELD_PREP(SF32LB57_LCDC_SPI_IF_CONF_LINE_MASK,
                           SF32LB57_LCDC_SPI_IF_CONF_LINE_4LINE_4DATA) |
                SF32LB57_LCDC_SPI_IF_CONF_SPI_CS_AUTO_DIS |
                SF32LB57_LCDC_SPI_IF_CONF_SPI_CLK_AUTO_DIS |
                SF32LB57_LCDC_SPI_IF_CONF_SPI_CS_NO_IDLE);
  prv_modify(lcdc, SF32LB57_LCDC_SPI_IF_CONF_EXT_OFFSET,
             SF32LB57_LCDC_SPI_IF_CONF_EXT_SPI_RD_SEL_MASK, 0U);
  prv_set_div(lcdc, lcdc->state->write_div);

  prv_modify(lcdc, SF32LB57_LCDC_TE_CONF_OFFSET, SF32LB57_LCDC_TE_CONF_ENABLE, 0U);
  prv_modify(lcdc, SF32LB57_LCDC_LCD_IF_CONF_OFFSET, 0U, SF32LB57_LCDC_LCD_IF_CONF_LCD_RSTB);

  return 0;
}

static int prv_update_start(const struct pbl_display_sf32lb57_lcdc *lcdc) {
  struct pbl_display_sf32lb57_lcdc_state *state = lcdc->state;
  const struct pbl_panel_timing *timing = lcdc->panel->timing;
  uint16_t width = lcdc->disp.props.width;
  uint16_t height = lcdc->disp.props.height;
  uint8_t *first = &state->fb[state->y0 * state->pitch];
  uint32_t src = (uint32_t)(uintptr_t)first;
  uint16_t y0 = state->y0;
  uint16_t y1 = state->y1;
  int ret;

  pbl_dcache_flush_range(first, (size_t)(y1 - y0 + 1U) * state->pitch);

  if (state->frame_rotated) {
    y0 = height - 1U - state->y1;
    y1 = height - 1U - state->y0;
  }

  const uint16_t py0 = timing->vback_porch + y0;
  const uint16_t py1 = timing->vback_porch + y1;
  uint8_t rows[4];

  pbl_put_be16(py0, &rows[0]);
  pbl_put_be16(py1, &rows[2]);

  ret = prv_command_write(lcdc, QSPI_OP_WRITE_CMD, PBL_MIPI_DCS_SET_PAGE_ADDRESS, rows,
                          sizeof(rows), true);
  if (ret != 0) {
    return ret;
  }

  // The pixels follow the opcode in the same CS frame
  ret = prv_command_write(lcdc, QSPI_OP_WRITE_PIXELS, PBL_MIPI_DCS_WRITE_MEMORY_START, nullptr, 0U,
                          false);
  if (ret != 0) {
    return ret;
  }

  prv_set_cs_release(lcdc, true);

  prv_write(
      lcdc, SF32LB57_LCDC_CANVAS_TL_POS_OFFSET,
      PBL_FIELD_PREP(SF32LB57_LCDC_POS_X_MASK, 0U) | PBL_FIELD_PREP(SF32LB57_LCDC_POS_Y_MASK, y0));
  prv_write(lcdc, SF32LB57_LCDC_CANVAS_BR_POS_OFFSET,
            PBL_FIELD_PREP(SF32LB57_LCDC_POS_X_MASK, width - 1U) |
                PBL_FIELD_PREP(SF32LB57_LCDC_POS_Y_MASK, y1));
  prv_write(lcdc, SF32LB57_LCDC_CANVAS_BG_OFFSET, SF32LB57_LCDC_CANVAS_BG_LB_BYPASS);

  uint32_t layer = PBL_FIELD_PREP(SF32LB57_LCDC_LAYER_CONFIG_FORMAT_MASK,
                                  prv_rgb565(lcdc) ? SF32LB57_LCDC_LAYER_CONFIG_FORMAT_RGB565
                                                   : SF32LB57_LCDC_LAYER_CONFIG_FORMAT_RGB332) |
                   PBL_FIELD_PREP(SF32LB57_LCDC_LAYER_CONFIG_ALPHA_MASK, 255U) |
                   PBL_FIELD_PREP(SF32LB57_LCDC_LAYER_CONFIG_WIDTH_MASK, state->pitch) |
                   SF32LB57_LCDC_LAYER_CONFIG_ACTIVE;
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
                PBL_FIELD_PREP(SF32LB57_LCDC_POS_Y_MASK, y1));
  prv_write(lcdc, SF32LB57_LCDC_LAYER0_SRC_OFFSET, src);
  prv_write(lcdc, SF32LB57_LCDC_LAYER0_FILTER_OFFSET, 0U);
  prv_write(lcdc, SF32LB57_LCDC_LAYER0_FILL_OFFSET, 0U);

  prv_modify(lcdc, SF32LB57_LCDC_SETTING_OFFSET, 0U, SF32LB57_LCDC_SETTING_EOF_MASK);
  prv_write(lcdc, SF32LB57_LCDC_COMMAND_OFFSET, SF32LB57_LCDC_COMMAND_START);

  return 0;
}

static bool prv_irq(const struct pbl_display_sf32lb57_lcdc *lcdc, uint32_t irq) {
  if ((irq & SF32LB57_LCDC_IRQ_ICB_OF_RAW_STAT) != 0U) {
    lcdc->state->status = -EIO;
  }

  return true;
}

const struct pbl_display_sf32lb57_lcdc__intf pbl_display_sf32lb57_lcdc__qspi = {
  .bus_format = PBL_PANEL_BUS_FORMAT_QSPI_RGB565,
  .hw_init = prv_hw_init,
  .update_start = prv_update_start,
  .irq = prv_irq,
  .command_write = prv_command_send,
  .command_read = prv_command_read,
};
