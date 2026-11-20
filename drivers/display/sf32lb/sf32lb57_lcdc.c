/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/display/sf32lb57_lcdc.h>
#include <pbl/kernel/cache.h>
#include <pbl/kernel/compiler.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/idle.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/logging/logging.h>
#include <pbl/soc/sf32lb/idle.h>
#include <pbl/util/bits.h>
#include <pbl/util/byteorder.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>

#include <hardware/sf32lb57_lcdc.h>

PBL_LOG_MODULE_DECLARE(driver_display, CONFIG_DRIVER_DISPLAY_LOG_LEVEL);

#define LCDC_BUSY_TIMEOUT_US  100000U
#define LCDC_FRAME_TIMEOUT_MS 200U

// Fastest clock panel registers are read back at
#define LCDC_READ_CLOCK_HZ 2000000U

#define LCDC_INTF_CLK_DIV_MAX 15U
#define LCDC_SDR_DIV_MAX      255U

// Quad SPI opcodes, followed by a 24-bit address holding the command in its middle byte
#define QSPI_OP_WRITE_CMD    0x02U
#define QSPI_OP_WRITE_PIXELS 0x32U
#define QSPI_OP_READ         0x03U

static const struct pbl_display_sf32lb57_lcdc *prv_lcdc(const struct pbl_display *disp) {
  return PBL_CONTAINER_OF(disp, const struct pbl_display_sf32lb57_lcdc, disp);
}

static uint32_t prv_read(const struct pbl_display_sf32lb57_lcdc *lcdc, uint32_t offset) {
  return pbl_readl(lcdc->base + offset);
}

static void prv_write(const struct pbl_display_sf32lb57_lcdc *lcdc, uint32_t offset,
                      uint32_t value) {
  pbl_writel(value, lcdc->base + offset);
}

static void prv_modify(const struct pbl_display_sf32lb57_lcdc *lcdc, uint32_t offset,
                       uint32_t clear, uint32_t set) {
  pbl_modifyl(clear, set, lcdc->base + offset);
}

static int prv_wait_idle(const struct pbl_display_sf32lb57_lcdc *lcdc) {
  return PBL_POLL_BUSY_US(
      (prv_read(lcdc, SF32LB57_LCDC_STATUS_OFFSET) & SF32LB57_LCDC_STATUS_LCD_BUSY) == 0U &&
          (prv_read(lcdc, SF32LB57_LCDC_LCD_SINGLE_OFFSET) & SF32LB57_LCDC_LCD_SINGLE_LCD_BUSY) ==
              0U,
      1U, LCDC_BUSY_TIMEOUT_US);
}

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
  *div = (rate + clock_hz - 1U) / clock_hz;
  if (*div == 0U || *div > LCDC_INTF_CLK_DIV_MAX * LCDC_SDR_DIV_MAX) {
    return -EINVAL;
  }

  return 0;
}

// The interface and SPI dividers with the smallest product not under the total, the interface
// one as large as it can be
static void prv_set_div(const struct pbl_display_sf32lb57_lcdc *lcdc, uint32_t div) {
  uint32_t intf = PBL_MIN(div, LCDC_INTF_CLK_DIV_MAX);
  uint32_t sdr = (div + intf - 1U) / intf;

  for (uint32_t i = intf - 1U; i >= 2U && intf * sdr > div; i--) {
    uint32_t s = (div + i - 1U) / i;

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

  ret = prv_wait_idle(lcdc);
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

  return prv_wait_idle(lcdc);
}

// Commands wait for the frame being sent, and keep the next one from starting
static int prv_bus_lock(const struct pbl_display_sf32lb57_lcdc *lcdc) {
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

static int prv_dbi_write(const struct pbl_mipi_dbi *dbi, uint8_t cmd, const uint8_t *data,
                         size_t len) {
  const struct pbl_display_sf32lb57_lcdc *lcdc =
      PBL_CONTAINER_OF(dbi, const struct pbl_display_sf32lb57_lcdc_dbi, dbi)->lcdc;
  int ret;

  ret = prv_bus_lock(lcdc);
  if (ret != 0) {
    return ret;
  }

  ret = prv_command_write(lcdc, QSPI_OP_WRITE_CMD, cmd, data, len, true);

  pbl_mutex_unlock(&lcdc->state->lock);

  return ret;
}

static int prv_dbi_read(const struct pbl_mipi_dbi *dbi, uint8_t cmd, uint8_t *data, size_t len) {
  const struct pbl_display_sf32lb57_lcdc *lcdc =
      PBL_CONTAINER_OF(dbi, const struct pbl_display_sf32lb57_lcdc_dbi, dbi)->lcdc;
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

  ret = prv_bus_lock(lcdc);
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

  ret = prv_wait_idle(lcdc);
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

const struct pbl_mipi_dbi_ops pbl_display_sf32lb57_lcdc_dbi_ops = {
  .command_write = prv_dbi_write,
  .command_read = prv_dbi_read,
};

static int prv_hw_init(const struct pbl_display_sf32lb57_lcdc *lcdc) {
  int ret;

  ret = prv_clock_div(lcdc, lcdc->panel->bus_max_hz, &lcdc->state->write_div);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_clk_enable(&lcdc->clk);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_reset_reset(&lcdc->rst);
  if (ret != 0) {
    return ret;
  }

  prv_modify(lcdc, SF32LB57_LCDC_SETTING_OFFSET, 0U, SF32LB57_LCDC_SETTING_AUTO_GATE_EN);

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

  ret = pbl_panel_disable(lcdc->panel);
  if (ret != 0) {
    return ret;
  }

  return pbl_panel_unprepare(lcdc->panel);
}

static uint16_t prv_row_multiple(const struct pbl_display_sf32lb57_lcdc *lcdc) {
  return PBL_MAX(lcdc->panel->row_multiple, 1U);
}

static bool prv_rgb565(const struct pbl_display_sf32lb57_lcdc *lcdc) {
  return lcdc->disp.props.format == PBL_DISPLAY_FORMAT_RGB565;
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
  if ((irq & SF32LB57_LCDC_IRQ_EOF_STAT) == 0U) {
    return;
  }

  prv_modify(lcdc, SF32LB57_LCDC_SETTING_OFFSET, SF32LB57_LCDC_SETTING_EOF_MASK, 0U);
  state->status = (irq & SF32LB57_LCDC_IRQ_ICB_OF_RAW_STAT) != 0U ? -EIO : 0;

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

  if (lcdc->panel->bus_format != PBL_PANEL_BUS_FORMAT_QSPI_RGB565 ||
      lcdc->panel->bus_max_hz == 0U || timing->hactive != disp->props.width ||
      timing->vactive != disp->props.height ||
      (disp->props.height % prv_row_multiple(lcdc)) != 0U) {
    return -EINVAL;
  }

  lcdc->state->lcdc = lcdc;
  pbl_work_init(&lcdc->state->work, prv_end_work);

  ret = pbl_panel_unprepare(lcdc->panel);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_pinctrl_select_state(&lcdc->pinctrl, PBL_PINCTRL_STATE_DEFAULT);
  if (ret != 0) {
    return ret;
  }

  ret = prv_hw_init(lcdc);
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

  ret = pbl_panel_set_h_flip(lcdc->panel, rotated);
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

  ret = prv_bus_lock(lcdc);
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
  state->frame = true;

  ret = prv_update_start(lcdc);
  if (ret != 0) {
    PBL_LOG_ERR("update start failed (%d)", ret);
    state->frame = false;
    prv_rows_convert(lcdc, false);
    pbl_idle_state_release(SOC_SF32LB_IDLE_DEEPWFI);
  }

  pbl_mutex_unlock(&state->lock);

  return ret;
}

const struct pbl_display_ops pbl_display_sf32lb57_lcdc_ops = {
  .init = prv_init,
  .enable = prv_enable,
  .disable = prv_disable,
  .set_rotated = prv_set_rotated,
  .update = prv_update,
};

#include PBL_DT_INSTANCES(sifli_sf32lb57_lcdc)
