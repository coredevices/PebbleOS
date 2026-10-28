/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/display/sf32lb_lcdc.h>
#include <pbl/drivers/panel.h>
#include <pbl/drivers/rtc.h>
#include <pbl/kernel/compiler.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/fatal.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/logging/logging.h>
#include <pbl/mcu/cache.h>
#include <pbl/services/new_timer/new_timer.h>
#include <pbl/soc/sf32lb/sleep.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>

#include <hardware/sf32lb52_lcdc.h>

PBL_LOG_MODULE_DEFINE(driver_display_sf32lb_lcdc, CONFIG_DRIVER_DISPLAY_LOG_LEVEL);

// HPSYS HCLK
#define LCDC_CLK_HZ 240000000U

#define LCDC_BUSY_TIMEOUT_US 100000U

// XRST to the first VST, and the start of the transfer once enabled
#define LCDC_XRST_DELAY_US  35U
#define LCDC_START_DELAY_US 1U

// Flash addresses on the code bus, seen by the controller on the system bus
#define LCDC_MPI_CBUS_START  0x10000000U
#define LCDC_MPI_CBUS_END    0x20000000U
#define LCDC_MPI_SBUS_OFFSET 0x50000000U

// Timeout for detecting a silent loss: the LCDC kicks off a transfer but never
// delivers the EOF interrupt, so the compositor would block forever. A normal
// full-frame transfer takes well under 20ms; 500ms is a conservative threshold
// that won't fire on transient stalls but catches a real wedge well before the
// user perceives the freeze.
#define DISPLAY_SILENT_LOSS_TIMEOUT_MS 500

// Bytes of the LCDC peripheral register file snapshotted into BSS before
// crashing. The PebbleOS coredump captures all of HPSYS SRAM but does not
// capture MMIO; copying registers into SRAM gets them into the coredump that
// gets shared with Sifli. 2KB covers IRQ/SETTING/COMMAND/CANVAS/LAYER0/JDI_PAR
// (lcd_if.h struct ends around offset 0x100).
#define DISPLAY_LCDC_REG_DUMP_BYTES 2048

// The silent-loss timer callback has the generation as its argument
static const struct pbl_display_sf32lb_lcdc *s_lcdc;

static TimerID s_silent_loss_timer = TIMER_INVALID_ID;
// Generation of the update in flight, and of the last one the EOF callback
// completed. The silent-loss handler only crashes if its generation never
// completed, so a timer left running after a completion is harmless.
static uint32_t s_update_gen;
static volatile uint32_t s_completed_gen;
// Captured at silent-loss time so the coredump shipped to Sifli carries the
// LCDC register state at the moment of the wedge. uint32_t storage (not
// uint8_t) so the linker word-aligns it — prv_snapshot_lcdc_regs reads MMIO
// as 32-bit volatiles and would fault on a misaligned destination. Marked
// volatile because the only consumer is the postmortem coredump tool, not
// any C code in this image — without it, the compiler eliminates the stores.
static volatile uint32_t s_lcdc_pre_crash_regs[DISPLAY_LCDC_REG_DUMP_BYTES / sizeof(uint32_t)];

// Ring buffer of recent LCDC interrupts, recorded by the interrupt handler
#define DISPLAY_IRQ_LOG_ENTRIES 32

typedef struct {
  uint32_t timestamp;    // rtc_get_ticks() low 32 bits
  uint32_t irq_before;   // LCD_IF.IRQ as the handler reads it
  uint32_t irq_after;    // LCD_IF.IRQ after the handler cleared bits
  uint32_t jdi_par_ctrl; // LCD_IF.JDI_PAR_CTRL before the handler (INT_LINE_NUM)
  uint32_t status;       // LCD_IF.STATUS before the handler
  uint32_t flags;        // bit0: EOF callback fired; bit1: update in progress at entry
} DisplayIrqLogEntry;

typedef struct {
  uint32_t write_count; // total IRQs logged; newest = (write_count-1) % N
  DisplayIrqLogEntry entries[DISPLAY_IRQ_LOG_ENTRIES];
} DisplayIrqLog;

static volatile DisplayIrqLog s_lcdc_irq_log;

#ifndef CONFIG_RELEASE
// Test hook: arm a one-shot drop of the next LCDC transfer-complete callback,
// simulating the silent-loss failure mode. The silent-loss timer should fire
// ~DISPLAY_SILENT_LOSS_TIMEOUT_MS later and a panic.
static volatile bool s_test_drop_next_complete;
#endif

static const struct pbl_display_sf32lb_lcdc *prv_lcdc(const struct pbl_display *disp) {
  return container_of(disp, const struct pbl_display_sf32lb_lcdc, disp);
}

static uint32_t prv_read(const struct pbl_display_sf32lb_lcdc *lcdc, uint32_t offset) {
  return pbl_readl(lcdc->base + offset);
}

static void prv_write(const struct pbl_display_sf32lb_lcdc *lcdc, uint32_t offset, uint32_t value) {
  pbl_writel(value, lcdc->base + offset);
}

static void prv_modify(const struct pbl_display_sf32lb_lcdc *lcdc, uint32_t offset, uint32_t clear,
                       uint32_t set) {
  pbl_modifyl(clear, set, lcdc->base + offset);
}

static int prv_wait_idle(const struct pbl_display_sf32lb_lcdc *lcdc) {
  return PBL_POLL_BUSY_US(
      (prv_read(lcdc, SF32LB52_LCDC_STATUS_OFFSET) & LCDC_STATUS_LCD_BUSY) == 0U &&
          (prv_read(lcdc, SF32LB52_LCDC_LCD_SINGLE_OFFSET) & LCDC_LCD_SINGLE_LCD_BUSY) == 0U,
      1U, LCDC_BUSY_TIMEOUT_US);
}

static int prv_hw_init(const struct pbl_display_sf32lb_lcdc *lcdc) {
  const struct pbl_panel_timing *timing = lcdc->panel->timing;
  uint32_t max_col = (timing->hback_porch + timing->hactive + timing->hfront_porch) / 2U;
  uint32_t max_line = (timing->vback_porch + timing->vactive + timing->vfront_porch) * 2U;
  uint32_t hck = ((LCDC_CLK_HZ + timing->clock_hz - 1U) / timing->clock_hz) / 2U;
  uint32_t vck = hck * max_col;
  int ret;

  if (hck == 0U || vck > PBL_FIELD_GET(LCDC_JDI_PAR_CONF5_VCK_WIDTH_MASK, UINT32_MAX)) {
    return -EINVAL;
  }

  ret = pbl_clk_enable(&lcdc->clk);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_reset_reset(&lcdc->rst);
  if (ret != 0) {
    return ret;
  }

  lcdc->state->max_line = max_line;

  prv_modify(lcdc, SF32LB52_LCDC_SETTING_OFFSET, 0U, LCDC_SETTING_AUTO_GATE_EN);

  prv_modify(lcdc, SF32LB52_LCDC_LCD_CONF_OFFSET,
             LCDC_LCD_CONF_TARGET_LCD_MASK | LCDC_LCD_CONF_LCD_INTF_SEL_MASK |
                 LCDC_LCD_CONF_LCD_FORMAT_MASK | LCDC_LCD_CONF_AHB_FORMAT_MASK |
                 LCDC_LCD_CONF_SPI_LCD_FORMAT_MASK | LCDC_LCD_CONF_DPI_LCD_FORMAT_MASK |
                 LCDC_LCD_CONF_JDI_SER_FORMAT_MASK,
             PBL_FIELD_PREP(LCDC_LCD_CONF_LCD_INTF_SEL_MASK, LCDC_LCD_CONF_LCD_INTF_SEL_JDI_PAR) |
                 PBL_FIELD_PREP(LCDC_LCD_CONF_LCD_FORMAT_MASK, LCDC_LCD_CONF_LCD_FORMAT_RGB332) |
                 PBL_FIELD_PREP(LCDC_LCD_CONF_AHB_FORMAT_MASK, LCDC_LCD_CONF_AHB_FORMAT_RGB332));

  prv_write(lcdc, SF32LB52_LCDC_JDI_PAR_CONF1_OFFSET,
            PBL_FIELD_PREP(LCDC_JDI_PAR_CONF1_MAX_COL_MASK, max_col - 1U) |
                PBL_FIELD_PREP(LCDC_JDI_PAR_CONF1_MAX_LINE_MASK, max_line - 1U));
  prv_write(lcdc, SF32LB52_LCDC_JDI_PAR_CONF4_OFFSET,
            PBL_FIELD_PREP(LCDC_JDI_PAR_CONF4_HST_WIDTH_MASK, hck) |
                PBL_FIELD_PREP(LCDC_JDI_PAR_CONF4_HCK_WIDTH_MASK, hck));
  prv_write(lcdc, SF32LB52_LCDC_JDI_PAR_CONF5_OFFSET,
            PBL_FIELD_PREP(LCDC_JDI_PAR_CONF5_VST_WIDTH_MASK, vck) |
                PBL_FIELD_PREP(LCDC_JDI_PAR_CONF5_VCK_WIDTH_MASK, vck));
  prv_write(lcdc, SF32LB52_LCDC_JDI_PAR_CONF6_OFFSET,
            PBL_FIELD_PREP(LCDC_JDI_PAR_CONF6_HST_DLY_MASK, hck) |
                PBL_FIELD_PREP(LCDC_JDI_PAR_CONF6_VCK_DLY_MASK, vck / 2U));
  prv_write(lcdc, SF32LB52_LCDC_JDI_PAR_CONF7_OFFSET,
            PBL_FIELD_PREP(LCDC_JDI_PAR_CONF7_HCK_DLY_MASK, hck / 2U) | LCDC_JDI_PAR_CONF7_DP_MODE);
  prv_write(lcdc, SF32LB52_LCDC_JDI_PAR_CONF8_OFFSET,
            PBL_FIELD_PREP(LCDC_JDI_PAR_CONF8_ENB_ST_COL_MASK, timing->enable_start) |
                PBL_FIELD_PREP(LCDC_JDI_PAR_CONF8_ENB_END_COL_MASK, timing->enable_end));
  prv_write(lcdc, SF32LB52_LCDC_JDI_PAR_CTRL_OFFSET, 0U);

  prv_modify(lcdc, SF32LB52_LCDC_TE_CONF_OFFSET, LCDC_TE_CONF_ENABLE, 0U);
  prv_modify(lcdc, SF32LB52_LCDC_LCD_IF_CONF_OFFSET, 0U, LCDC_LCD_IF_CONF_LCD_RSTB);

  return 0;
}

static int prv_panel_on(const struct pbl_display_sf32lb_lcdc *lcdc) {
  int ret;

  ret = pbl_panel_prepare(lcdc->panel);
  if (ret != 0) {
    return ret;
  }

  return pbl_panel_enable(lcdc->panel);
}

static int prv_panel_off(const struct pbl_display_sf32lb_lcdc *lcdc) {
  int ret;

  (void)prv_wait_idle(lcdc);

  ret = pbl_panel_disable(lcdc->panel);
  if (ret != 0) {
    return ret;
  }

  return pbl_panel_unprepare(lcdc->panel);
}

// 222: XX RR GG BB, 332: RR 0G GG BB
static PBL_ALWAYS_INLINE uint32_t prv_to_332(uint32_t p) {
  return ((p & 0x30303030) << 2) | ((p & 0x0C0C0C0C) << 1) | (p & 0x03030303);
}

static PBL_ALWAYS_INLINE uint32_t prv_to_222(uint32_t p) {
  return ((p >> 2) & 0x30303030) | ((p >> 1) & 0x0C0C0C0C) | (p & 0x03030303);
}

static PBL_ALWAYS_INLINE void prv_row_convert(uint32_t *row, uint32_t words,
                                              uint32_t (*conv)(uint32_t)) {
  for (uint32_t i = 0; i < words; i++) {
    row[i] = conv(row[i]);
  }
}

// HMirror is done in software, VMirror by the LCDC
static PBL_ALWAYS_INLINE void prv_row_convert_mirror(uint32_t *row, uint32_t words,
                                                     uint32_t (*conv)(uint32_t)) {
  uint32_t i = 0;
  uint32_t j = words - 1;
  for (; i < j; i++, j--) {
    uint32_t a = row[i];
    uint32_t b = row[j];
    row[i] = PBL_BSWAP32(conv(b));
    row[j] = PBL_BSWAP32(conv(a));
  }
  if (i == j) {
    row[i] = PBL_BSWAP32(conv(row[i]));
  }
}

static void prv_rows_convert(const struct pbl_display_sf32lb_lcdc *lcdc,
                             uint32_t (*conv)(uint32_t)) {
  struct pbl_display_sf32lb_lcdc_state *state = lcdc->state;
  uint32_t words = lcdc->disp.width / 4U;

  for (uint16_t y = state->y0; y <= state->y1; y++) {
    uint32_t *row = (uint32_t *)&state->fb[y * state->pitch];

    if (state->rotated) {
      prv_row_convert_mirror(row, words, conv);
    } else {
      prv_row_convert(row, words, conv);
    }
  }
}

static uint32_t prv_bus_addr(const uint8_t *p) {
  uint32_t addr = (uint32_t)(uintptr_t)p;

  if (addr >= LCDC_MPI_CBUS_START && addr < LCDC_MPI_CBUS_END) {
    addr += LCDC_MPI_SBUS_OFFSET;
  }

  return addr;
}

static int prv_update_start(const struct pbl_display_sf32lb_lcdc *lcdc) {
  struct pbl_display_sf32lb_lcdc_state *state = lcdc->state;
  const struct pbl_panel_timing *timing = lcdc->panel->timing;
  uint16_t width = lcdc->disp.width;
  uint16_t height = lcdc->disp.height;
  uint8_t *first = &state->fb[state->y0 * state->pitch];
  uint32_t src = prv_bus_addr(first);
  uint32_t layer;
  int ret;

  uintptr_t fb_addr = (uintptr_t)first;
  size_t fb_size = (size_t)(state->y1 - state->y0 + 1) * state->pitch;
  dcache_align(&fb_addr, &fb_size);
  dcache_flush((const void *)fb_addr, fb_size);

  uint16_t roi_y0 = state->y0;
  uint16_t roi_y1 = state->y1;
  if (state->rotated) {
    roi_y0 = height - 1 - state->y1;
    roi_y1 = height - 1 - state->y0;
  }

  ret = prv_wait_idle(lcdc);
  if (ret != 0) {
    return -EBUSY;
  }

  // Each line takes two scans on the JDI parallel interface
  uint32_t y1_doubled = roi_y0 + (roi_y1 - roi_y0 + 1U) * 2U - 1U;

  prv_write(lcdc, SF32LB52_LCDC_CANVAS_BG_OFFSET, 0U);
  prv_write(lcdc, SF32LB52_LCDC_CANVAS_TL_POS_OFFSET,
            PBL_FIELD_PREP(LCDC_CANVAS_TL_POS_X0_MASK, 0U) |
                PBL_FIELD_PREP(LCDC_CANVAS_TL_POS_Y0_MASK, roi_y0));
  prv_write(lcdc, SF32LB52_LCDC_CANVAS_BR_POS_OFFSET,
            PBL_FIELD_PREP(LCDC_CANVAS_BR_POS_X1_MASK, width - 1U) |
                PBL_FIELD_PREP(LCDC_CANVAS_BR_POS_Y1_MASK, y1_doubled));

  layer = PBL_FIELD_PREP(LCDC_LAYER0_CONFIG_FORMAT_MASK, LCDC_LAYER0_CONFIG_FORMAT_RGB332) |
          PBL_FIELD_PREP(LCDC_LAYER0_CONFIG_ALPHA_MASK, 255U) | LCDC_LAYER0_CONFIG_ACTIVE |
          LCDC_LAYER0_CONFIG_PREFETCH_EN | LCDC_LAYER0_CONFIG_LINE_FETCH_MODE |
          PBL_FIELD_PREP(LCDC_LAYER0_CONFIG_WIDTH_MASK, width);
  if (state->rotated) {
    // Read from the last line up
    layer |= LCDC_LAYER0_CONFIG_V_MIRROR;
    src += (uint32_t)width * (roi_y1 - roi_y0);
  }

  prv_write(lcdc, SF32LB52_LCDC_LAYER0_CONFIG_OFFSET, layer);
  prv_write(lcdc, SF32LB52_LCDC_LAYER0_DECOMP_OFFSET, 0U);
  prv_write(lcdc, SF32LB52_LCDC_LAYER0_TL_POS_OFFSET,
            PBL_FIELD_PREP(LCDC_LAYER0_TL_POS_X0_MASK, 0U) |
                PBL_FIELD_PREP(LCDC_LAYER0_TL_POS_Y0_MASK, roi_y0));
  prv_write(lcdc, SF32LB52_LCDC_LAYER0_BR_POS_OFFSET,
            PBL_FIELD_PREP(LCDC_LAYER0_BR_POS_X1_MASK, width - 1U) |
                PBL_FIELD_PREP(LCDC_LAYER0_BR_POS_Y1_MASK, y1_doubled));
  prv_write(lcdc, SF32LB52_LCDC_LAYER0_SRC_OFFSET, src);
  prv_write(lcdc, SF32LB52_LCDC_LAYER0_FILTER_OFFSET, 0U);
  prv_write(lcdc, SF32LB52_LCDC_LAYER0_FILL_OFFSET, 0U);

  uint32_t start_line = (timing->vback_porch + roi_y0) * 2U + 1U;
  uint32_t end_line = (timing->vback_porch + roi_y1 + 1U) * 2U;
  uint32_t start_col = timing->hback_porch / 2U;
  uint32_t end_col = (timing->hback_porch + width) / 2U - 1U;

  prv_write(lcdc, SF32LB52_LCDC_JDI_PAR_CONF2_OFFSET,
            PBL_FIELD_PREP(LCDC_JDI_PAR_CONF2_ST_LINE_MASK, start_line) |
                PBL_FIELD_PREP(LCDC_JDI_PAR_CONF2_END_LINE_MASK, end_line));
  prv_write(lcdc, SF32LB52_LCDC_JDI_PAR_CONF9_OFFSET,
            PBL_FIELD_PREP(LCDC_JDI_PAR_CONF9_ENB_ST_LINE_MASK, start_line + 1U) |
                PBL_FIELD_PREP(LCDC_JDI_PAR_CONF9_ENB_END_LINE_MASK, end_line + 1U));
  prv_write(lcdc, SF32LB52_LCDC_JDI_PAR_CONF3_OFFSET,
            PBL_FIELD_PREP(LCDC_JDI_PAR_CONF3_ST_COL_MASK, start_col) |
                PBL_FIELD_PREP(LCDC_JDI_PAR_CONF3_END_COL_MASK, end_col));
  prv_write(lcdc, SF32LB52_LCDC_JDI_PAR_CONF10_OFFSET,
            PBL_FIELD_PREP(LCDC_JDI_PAR_CONF10_HC_ST_LINE_MASK, start_line) |
                PBL_FIELD_PREP(LCDC_JDI_PAR_CONF10_HC_END_LINE_MASK, end_line + 2U));

  // First interrupt halfway through the frame
  prv_modify(lcdc, SF32LB52_LCDC_JDI_PAR_CTRL_OFFSET, LCDC_JDI_PAR_CTRL_INT_LINE_NUM_MASK,
             PBL_FIELD_PREP(LCDC_JDI_PAR_CTRL_INT_LINE_NUM_MASK, state->max_line / 2U - 1U));
  prv_modify(lcdc, SF32LB52_LCDC_SETTING_OFFSET, 0U,
             LCDC_SETTING_JDI_PARL_INTR_MASK | LCDC_SETTING_EOF_MASK);

  prv_modify(lcdc, SF32LB52_LCDC_JDI_PAR_CTRL_OFFSET, 0U, LCDC_JDI_PAR_CTRL_XRST);
  pbl_busy_wait_us(LCDC_XRST_DELAY_US);
  prv_modify(lcdc, SF32LB52_LCDC_JDI_PAR_CTRL_OFFSET, 0U, LCDC_JDI_PAR_CTRL_ENABLE);
  pbl_busy_wait_us(LCDC_START_DELAY_US);

  return 0;
}

static void prv_snapshot_lcdc_regs(const struct pbl_display_sf32lb_lcdc *lcdc) {
  for (size_t i = 0; i < DISPLAY_LCDC_REG_DUMP_BYTES / sizeof(uint32_t); i++) {
    s_lcdc_pre_crash_regs[i] = prv_read(lcdc, i * sizeof(uint32_t));
  }
}

static void prv_silent_loss_handler(void *data) {
  struct pbl_display_sf32lb_lcdc_state *state = s_lcdc->state;

  if (s_completed_gen == (uint32_t)(uintptr_t)data) {
    return;
  }

  prv_snapshot_lcdc_regs(s_lcdc);
  uintptr_t snap_addr = (uintptr_t)s_lcdc_pre_crash_regs;
  size_t snap_size = sizeof(s_lcdc_pre_crash_regs);
  dcache_align(&snap_addr, &snap_size);
  dcache_flush((const void *)snap_addr, snap_size);
  pbl_panic("LCDC silent loss: no EOF in %ums (STATUS=0x%lx y=%u..%u)",
            (unsigned)DISPLAY_SILENT_LOSS_TIMEOUT_MS,
            (unsigned long)prv_read(s_lcdc, SF32LB52_LCDC_STATUS_OFFSET), (unsigned)state->y0,
            (unsigned)state->y1);
}

static void prv_update_end(const struct pbl_display_sf32lb_lcdc *lcdc, int status) {
  prv_rows_convert(lcdc, prv_to_222);
  soc_sf32lb_sleep_release(SOC_SF32LB_DEEPWFI);
  pbl_display_complete(&lcdc->disp, status);
}

// Returns true at the end of the frame
static bool prv_handle_irq(const struct pbl_display_sf32lb_lcdc *lcdc, uint32_t irq, int *status) {
  if ((irq & LCDC_IRQ_JDI_PAR_UDR_RAW_STAT) != 0U) {
    *status = -EIO;
  }

  if ((irq & LCDC_IRQ_JDI_PARL_INTR_RAW_STAT) != 0U) {
    uint32_t ctrl = prv_read(lcdc, SF32LB52_LCDC_JDI_PAR_CTRL_OFFSET);
    uint32_t max_line = PBL_FIELD_GET(LCDC_JDI_PAR_CONF1_MAX_LINE_MASK,
                                      prv_read(lcdc, SF32LB52_LCDC_JDI_PAR_CONF1_OFFSET));

    if (PBL_FIELD_GET(LCDC_JDI_PAR_CTRL_INT_LINE_NUM_MASK, ctrl) != max_line) {
      // Halfway: interrupt again at the end, and send the frame only once
      ctrl &= ~(LCDC_JDI_PAR_CTRL_INT_LINE_NUM_MASK | LCDC_JDI_PAR_CTRL_ENABLE);
      ctrl |= PBL_FIELD_PREP(LCDC_JDI_PAR_CTRL_INT_LINE_NUM_MASK, max_line);
      prv_write(lcdc, SF32LB52_LCDC_JDI_PAR_CTRL_OFFSET, ctrl);
      return false;
    }

    prv_modify(lcdc, SF32LB52_LCDC_SETTING_OFFSET, LCDC_SETTING_JDI_PARL_INTR_MASK, 0U);
    prv_modify(lcdc, SF32LB52_LCDC_JDI_PAR_CTRL_OFFSET, LCDC_JDI_PAR_CTRL_XRST, 0U);
    prv_modify(lcdc, SF32LB52_LCDC_JDI_PAR_EX_CTRL_OFFSET, 0U, LCDC_JDI_PAR_EX_CTRL_CNT_EN);
  }

  if ((irq & LCDC_IRQ_EOF_STAT) == 0U) {
    return false;
  }

  prv_modify(lcdc, SF32LB52_LCDC_SETTING_OFFSET, LCDC_SETTING_EOF_MASK, 0U);

  return true;
}

void pbl_display_sf32lb_lcdc_irq_handler(const struct pbl_display_sf32lb_lcdc *lcdc) {
  uint32_t irq = prv_read(lcdc, SF32LB52_LCDC_IRQ_OFFSET);
  int status = 0;
  bool eof;

  volatile DisplayIrqLogEntry *entry =
      &s_lcdc_irq_log.entries[s_lcdc_irq_log.write_count % DISPLAY_IRQ_LOG_ENTRIES];
  entry->timestamp = (uint32_t)rtc_get_ticks();
  entry->irq_before = irq;
  entry->jdi_par_ctrl = prv_read(lcdc, SF32LB52_LCDC_JDI_PAR_CTRL_OFFSET);
  entry->status = prv_read(lcdc, SF32LB52_LCDC_STATUS_OFFSET);
  entry->flags = pbl_display_update_in_progress(&lcdc->disp) ? 0x2u : 0x0u;

  prv_write(lcdc, SF32LB52_LCDC_IRQ_OFFSET, irq);
  eof = prv_handle_irq(lcdc, irq, &status);

  entry->irq_after = prv_read(lcdc, SF32LB52_LCDC_IRQ_OFFSET);
  if (eof) {
    entry->flags |= 0x1u;
  }
  s_lcdc_irq_log.write_count++;

  if (!eof) {
    return;
  }

#ifndef CONFIG_RELEASE
  if (s_test_drop_next_complete && pbl_display_update_in_progress(&lcdc->disp)) {
    s_test_drop_next_complete = false;
    // Simulate the lost-completion failure mode: leave the generation
    // incomplete and don't end the update. The silent-loss timer should fire
    // ~DISPLAY_SILENT_LOSS_TIMEOUT_MS later and a panic.
    return;
  }
#endif

  s_completed_gen = s_update_gen;
  prv_update_end(lcdc, status);
}

static int prv_init(const struct pbl_display *disp) {
  const struct pbl_display_sf32lb_lcdc *lcdc = prv_lcdc(disp);
  const struct pbl_panel_timing *timing = lcdc->panel->timing;
  int ret;

  if (pbl_device_init(&lcdc->clk.ctrl->dev) != 0 || pbl_device_init(&lcdc->rst.ctrl->dev) != 0 ||
      pbl_device_init(&lcdc->panel->dev) != 0) {
    return -ENODEV;
  }

  if (lcdc->panel->bus_format != PBL_PANEL_BUS_FORMAT_JDI_RGB222 ||
      timing->hactive != disp->width || timing->vactive != disp->height ||
      (disp->width % 4U) != 0U) {
    return -EINVAL;
  }

  for (uint8_t i = 0U; i < lcdc->num_discharge; i++) {
    if (pbl_device_init(&lcdc->discharge[i].port->dev) != 0) {
      return -ENODEV;
    }
  }

  s_lcdc = lcdc;
  s_silent_loss_timer = new_timer_create();

  // Power cycle the panel with its inputs low
  for (uint8_t i = 0U; i < lcdc->num_discharge; i++) {
    ret = pbl_gpio_configure(&lcdc->discharge[i], PBL_GPIO_OUTPUT_LOW);
    if (ret != 0) {
      return ret;
    }
  }

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

  pbl_irq_enable(lcdc->irqn);

  return prv_panel_on(lcdc);
}

static int prv_enable(const struct pbl_display *disp) {
  return prv_panel_on(prv_lcdc(disp));
}

static int prv_disable(const struct pbl_display *disp) {
  return prv_panel_off(prv_lcdc(disp));
}

static int prv_set_rotated(const struct pbl_display *disp, bool rotated) {
  const struct pbl_display_sf32lb_lcdc *lcdc = prv_lcdc(disp);

  lcdc->state->rotated = rotated;

  return 0;
}

static int prv_update(const struct pbl_display *disp, const struct pbl_display_buffer *buf,
                      uint16_t y0, uint16_t y1) {
  const struct pbl_display_sf32lb_lcdc *lcdc = prv_lcdc(disp);
  struct pbl_display_sf32lb_lcdc_state *state = lcdc->state;
  int ret;

  if (buf->pitch != disp->width) {
    return -EINVAL;
  }

  state->fb = buf->data;
  state->pitch = buf->pitch;
  state->y0 = y0;
  state->y1 = y1;

  // Converted in place, to save a framebuffer of RAM
  prv_rows_convert(lcdc, prv_to_332);

  soc_sf32lb_sleep_block(SOC_SF32LB_DEEPWFI);

  // Arm the timer before kickoff so the EOF IRQ, which can fire as soon as
  // the transfer starts, never races us into a state where the timer
  // isn't yet armed.
  s_update_gen++;
  new_timer_start(s_silent_loss_timer, DISPLAY_SILENT_LOSS_TIMEOUT_MS, prv_silent_loss_handler,
                  (void *)(uintptr_t)s_update_gen, 0);

  ret = prv_update_start(lcdc);
  if (ret != 0) {
    PBL_LOG_ERR("display: update start failed (%d)", ret);
    s_completed_gen = s_update_gen;
    prv_rows_convert(lcdc, prv_to_222);
    soc_sf32lb_sleep_release(SOC_SF32LB_DEEPWFI);
    return ret;
  }

  return 0;
}

const struct pbl_display_ops pbl_display_sf32lb_lcdc_ops = {
  .init = prv_init,
  .enable = prv_enable,
  .disable = prv_disable,
  .set_rotated = prv_set_rotated,
  .update = prv_update,
};

#ifndef CONFIG_RELEASE
void pbl_display_sf32lb_lcdc_test_drop_next_complete(void) {
  s_test_drop_next_complete = true;
}
#endif
