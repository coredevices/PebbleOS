/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/panel/otm8009a.h>
#include <pbl/kernel/delay.h>
#include <pbl/util/misc.h>

// The second parameter set of the IC is reached by writing the low byte of a register's address
// to 0x00, then the parameter to the register's high byte
#define CMD_ADDRESS_SHIFT 0x00U
#define CMD_ENABLE_CMD2   0xffU
#define CMD_GAMMA_POS     0xe1U
#define CMD_GAMMA_NEG     0xe2U
#define CMD_WRITE_CABC    0x55U

#define COLMOD_RGB888 0x77U

// Entries of an init sequence: the parameter count, the command and its parameters, or a delay
#define DELAY 0xffU

#define SHIFT(addr)    1U, CMD_ADDRESS_SHIFT, (addr)
#define W1(cmd, value) 1U, (cmd), (value)
#define WAIT(ms)       DELAY, (ms)

// The vendor setup of the panel, before it leaves sleep: power, source driver, GOA timing and
// gamma, as the panel maker gives them
static const uint8_t s_setup[] = {
  // Into the second parameter set
  SHIFT(0x00),
  3U,
  CMD_ENABLE_CMD2,
  0x80,
  0x09,
  0x01,
  SHIFT(0x80),
  2U,
  CMD_ENABLE_CMD2,
  0x80,
  0x09,
  // Source output at GND in the porches and outside the display area
  SHIFT(0x80),
  W1(0xc4, 0x30),
  WAIT(10U),
  SHIFT(0x8a),
  W1(0xc4, 0x40),
  WAIT(10U),
  // GVDD test mode
  SHIFT(0xb1),
  W1(0xc5, 0xa9),
  // Pump 4 and 5 VGH and VGL
  SHIFT(0x91),
  W1(0xc5, 0x34),
  // Column inversion
  SHIFT(0xb4),
  W1(0xc0, 0x50),
  // VCOM
  SHIFT(0x00),
  W1(0xd9, 0x4e),
  // Oscillator at 65 Hz
  SHIFT(0x81),
  W1(0xc1, 0x66),
  // Internal video mode
  SHIFT(0xa1),
  W1(0xc1, 0x08),
  // Pumps 4 and 5 at x6
  SHIFT(0x92),
  W1(0xc5, 0x01),
  // Pump 4 clock at half a line
  SHIFT(0x95),
  W1(0xc5, 0x34),
  // GVDD and NGVDD
  SHIFT(0x00),
  2U,
  0xd8,
  0x79,
  0x79,
  SHIFT(0x94),
  W1(0xc5, 0x33),
  // Panel display timing
  SHIFT(0xa3),
  W1(0xc0, 0x1b),
  // Power control
  SHIFT(0x82),
  W1(0xc5, 0x83),
  // Source driver precharge
  SHIFT(0x81),
  W1(0xc4, 0x83),
  SHIFT(0xa1),
  W1(0xc1, 0x0e),
  SHIFT(0xa6),
  2U,
  0xb3,
  0x00,
  0x01,
  // GOA timing
  SHIFT(0x80),
  6U,
  0xce,
  0x85,
  0x01,
  0x00,
  0x84,
  0x01,
  0x00,
  SHIFT(0xa0),
  14U,
  0xce,
  0x18,
  0x04,
  0x03,
  0x39,
  0x00,
  0x00,
  0x00,
  0x18,
  0x03,
  0x03,
  0x3a,
  0x00,
  0x00,
  0x00,
  SHIFT(0xb0),
  14U,
  0xce,
  0x18,
  0x02,
  0x03,
  0x3b,
  0x00,
  0x00,
  0x00,
  0x18,
  0x01,
  0x03,
  0x3c,
  0x00,
  0x00,
  0x00,
  SHIFT(0xc0),
  10U,
  0xcf,
  0x01,
  0x01,
  0x20,
  0x20,
  0x00,
  0x00,
  0x01,
  0x02,
  0x00,
  0x00,
  SHIFT(0xd0),
  W1(0xcf, 0x00),
  // GOA panel mapping
  SHIFT(0x80),
  10U,
  0xcb,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  SHIFT(0x90),
  15U,
  0xcb,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  SHIFT(0xa0),
  15U,
  0xcb,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  SHIFT(0xb0),
  10U,
  0xcb,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  SHIFT(0xc0),
  15U,
  0xcb,
  0x00,
  0x04,
  0x04,
  0x04,
  0x04,
  0x04,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  SHIFT(0xd0),
  15U,
  0xcb,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x04,
  0x04,
  0x04,
  0x04,
  0x04,
  0x00,
  0x00,
  0x00,
  0x00,
  SHIFT(0xe0),
  10U,
  0xcb,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  SHIFT(0xf0),
  10U,
  0xcb,
  0xff,
  0xff,
  0xff,
  0xff,
  0xff,
  0xff,
  0xff,
  0xff,
  0xff,
  0xff,
  SHIFT(0x80),
  10U,
  0xcc,
  0x00,
  0x26,
  0x09,
  0x0b,
  0x01,
  0x25,
  0x00,
  0x00,
  0x00,
  0x00,
  SHIFT(0x90),
  15U,
  0xcc,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x26,
  0x0a,
  0x0c,
  0x02,
  SHIFT(0xa0),
  15U,
  0xcc,
  0x25,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  SHIFT(0xb0),
  10U,
  0xcc,
  0x00,
  0x25,
  0x0c,
  0x0a,
  0x02,
  0x26,
  0x00,
  0x00,
  0x00,
  0x00,
  SHIFT(0xc0),
  15U,
  0xcc,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x25,
  0x0b,
  0x09,
  0x01,
  SHIFT(0xd0),
  15U,
  0xcc,
  0x26,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  // Pump 1 minimum and maximum
  SHIFT(0x81),
  W1(0xc5, 0x66),
  SHIFT(0xb6),
  W1(0xf5, 0x06),
  // CABC PWM at 19.5 kHz
  SHIFT(0xb1),
  W1(0xc6, 0x06),
  // Out of the second parameter set
  SHIFT(0x00),
  3U,
  CMD_ENABLE_CMD2,
  0xff,
  0xff,
  0xff,
  // Gamma 2.2, positive and negative
  SHIFT(0x00),
  16U,
  CMD_GAMMA_POS,
  0x00,
  0x09,
  0x0f,
  0x0e,
  0x07,
  0x10,
  0x0b,
  0x0a,
  0x04,
  0x07,
  0x0b,
  0x08,
  0x0f,
  0x10,
  0x0a,
  0x01,
  SHIFT(0x00),
  16U,
  CMD_GAMMA_NEG,
  0x00,
  0x09,
  0x0f,
  0x0e,
  0x07,
  0x10,
  0x0b,
  0x0a,
  0x04,
  0x07,
  0x0b,
  0x08,
  0x0f,
  0x10,
  0x0a,
  0x01,
};

// Address mode of a frame turned a quarter turn clockwise: rows and columns exchanged, columns
// right to left
#define MADCTL_QUARTER_TURN 0x60U

// Leaving sleep: 24-bit pixels, and no adaptive backlight, which would change the brightness with
// the image
static const uint8_t s_wake[] = {
  0U,
  PBL_MIPI_DCS_EXIT_SLEEP_MODE,
  WAIT(120U),
  W1(PBL_MIPI_DCS_SET_PIXEL_FORMAT, COLMOD_RGB888),
  W1(CMD_WRITE_CABC, 0x00),
  W1(PBL_MIPI_DCS_WRITE_CONTROL_DISPLAY, 0x24),
};

// Portrait; a frame turned a quarter turn has the active width and height exchanged. The
// controller only writes frames to the frame memory, with the shortest blanking
static const struct pbl_panel_timing s_timing = {
  .clock_hz = 42000000U,
  .hactive = 480U,
  .hsync_len = 1U,
  .hback_porch = 1U,
  .hfront_porch = 1U,
  .vactive = 800U,
  .vsync_len = 1U,
  .vback_porch = 1U,
  .vfront_porch = 1U,
};

// Where the scan is far enough for the left half of a turned frame to be written ahead of it
#define TEAR_SCANLINE ((s_timing.vactive * 2U) / 3U)

static const struct pbl_panel_otm8009a *prv_otm(const struct pbl_panel *panel) {
  return PBL_CONTAINER_OF(panel, const struct pbl_panel_otm8009a, panel);
}

static int prv_run(const struct pbl_panel_otm8009a *otm, const uint8_t *seq, size_t len) {
  size_t i = 0U;

  while (i < len) {
    uint8_t n = seq[i];
    int ret;

    if (n == DELAY) {
      pbl_sleep_ms(seq[i + 1U]);
      i += 2U;
      continue;
    }

    ret = pbl_mipi_dsi_dcs_write(&otm->state->dsi, seq[i + 1U], &seq[i + 2U], n);
    if (ret != 0) {
      return ret;
    }
    i += 2U + n;
  }

  return 0;
}

// The whole panel, in the orientation of the frame
static int prv_address(const struct pbl_panel_otm8009a *otm) {
  const struct pbl_panel_timing *t = &otm->state->timing;
  const uint8_t mode = otm->state->turned ? MADCTL_QUARTER_TURN : 0x00U;
  const uint8_t cols[] = {
    0x00, 0x00, (uint8_t)((t->hactive - 1U) >> 8), (uint8_t)(t->hactive - 1U)
  };
  const uint8_t rows[] = {
    0x00, 0x00, (uint8_t)((t->vactive - 1U) >> 8), (uint8_t)(t->vactive - 1U)
  };
  int ret;

  ret = pbl_mipi_dsi_dcs_write(&otm->state->dsi, PBL_MIPI_DCS_SET_ADDRESS_MODE, &mode, 1U);
  if (ret != 0) {
    return ret;
  }

  ret =
      pbl_mipi_dsi_dcs_write(&otm->state->dsi, PBL_MIPI_DCS_SET_COLUMN_ADDRESS, cols, sizeof(cols));
  if (ret != 0) {
    return ret;
  }

  return pbl_mipi_dsi_dcs_write(&otm->state->dsi, PBL_MIPI_DCS_SET_PAGE_ADDRESS, rows,
                                sizeof(rows));
}

// The controller dims the backlight of the module with its PWM output
static int prv_brightness(const struct pbl_panel_otm8009a *otm, uint8_t brightness) {
  const uint8_t level = (uint8_t)((brightness * UINT8_MAX) / PBL_BACKLIGHT_BRIGHTNESS_MAX);

  return pbl_mipi_dsi_dcs_write(&otm->state->dsi, PBL_MIPI_DCS_SET_DISPLAY_BRIGHTNESS, &level, 1U);
}

static int prv_set_window(const struct pbl_panel *panel, uint16_t x0, uint16_t y0, uint16_t x1,
                          uint16_t y1) {
  const struct pbl_panel_otm8009a *otm = prv_otm(panel);
  const uint8_t cols[] = {(uint8_t)(x0 >> 8), (uint8_t)x0, (uint8_t)(x1 >> 8), (uint8_t)x1};
  const uint8_t rows[] = {(uint8_t)(y0 >> 8), (uint8_t)y0, (uint8_t)(y1 >> 8), (uint8_t)y1};
  int ret;

  ret =
      pbl_mipi_dsi_dcs_write(&otm->state->dsi, PBL_MIPI_DCS_SET_COLUMN_ADDRESS, cols, sizeof(cols));
  if (ret != 0) {
    return ret;
  }

  return pbl_mipi_dsi_dcs_write(&otm->state->dsi, PBL_MIPI_DCS_SET_PAGE_ADDRESS, rows,
                                sizeof(rows));
}

static int prv_set_tear(const struct pbl_panel *panel, bool on) {
  const struct pbl_panel_otm8009a *otm = prv_otm(panel);
  const uint8_t line[] = {(uint8_t)(TEAR_SCANLINE >> 8), (uint8_t)TEAR_SCANLINE};

  if (!on) {
    return pbl_mipi_dsi_dcs_write(&otm->state->dsi, PBL_MIPI_DCS_SET_TEAR_OFF, nullptr, 0U);
  }

  return pbl_mipi_dsi_dcs_write(&otm->state->dsi, PBL_MIPI_DCS_SET_TEAR_SCANLINE, line,
                                sizeof(line));
}

static int prv_set_quarter_turn(const struct pbl_panel *panel, bool turn) {
  struct pbl_panel_otm8009a_state *st = prv_otm(panel)->state;

  st->turned = turn;
  st->timing = s_timing;
  if (turn) {
    st->timing.hactive = s_timing.vactive;
    st->timing.vactive = s_timing.hactive;
  }

  return 0;
}

static int prv_prepare(const struct pbl_panel *panel) {
  const struct pbl_panel_otm8009a *otm = prv_otm(panel);
  int ret;

  ret = pbl_gpio_set(&otm->reset, true);
  if (ret != 0) {
    return ret;
  }
  pbl_sleep_ms(1U);
  ret = pbl_gpio_set(&otm->reset, false);
  if (ret != 0) {
    return ret;
  }
  pbl_sleep_ms(20U);

  otm->state->dsi.mode_flags = PBL_MIPI_DSI_MODE_LPM;
  ret = pbl_mipi_dsi_attach(&otm->state->dsi);
  if (ret != 0) {
    return ret;
  }

  ret = prv_run(otm, s_setup, sizeof(s_setup));
  if (ret != 0) {
    return ret;
  }

  ret = prv_run(otm, s_wake, sizeof(s_wake));
  if (ret != 0) {
    return ret;
  }

  ret = prv_brightness(otm, otm->state->brightness);
  if (ret != 0) {
    return ret;
  }

  ret = prv_address(otm);
  if (ret != 0) {
    return ret;
  }

  // The window and the tearing effect then go between frames, in high speed
  otm->state->dsi.mode_flags &= ~PBL_MIPI_DSI_MODE_LPM;
  otm->state->on = true;

  return 0;
}

static int prv_enable(const struct pbl_panel *panel) {
  const struct pbl_panel_otm8009a *otm = prv_otm(panel);
  int ret;

  ret = pbl_mipi_dsi_dcs_write(&otm->state->dsi, PBL_MIPI_DCS_SET_DISPLAY_ON, nullptr, 0U);
  if (ret != 0) {
    return ret;
  }

  // Frames go to the frame memory
  return pbl_mipi_dsi_dcs_write(&otm->state->dsi, PBL_MIPI_DCS_WRITE_MEMORY_START, nullptr, 0U);
}

static int prv_disable(const struct pbl_panel *panel) {
  return pbl_mipi_dsi_dcs_write(&prv_otm(panel)->state->dsi, PBL_MIPI_DCS_SET_DISPLAY_OFF, nullptr,
                                0U);
}

static int prv_unprepare(const struct pbl_panel *panel) {
  const struct pbl_panel_otm8009a *otm = prv_otm(panel);
  int ret;

  otm->state->on = false;
  ret = pbl_mipi_dsi_dcs_write(&otm->state->dsi, PBL_MIPI_DCS_ENTER_SLEEP_MODE, nullptr, 0U);
  if (ret != 0) {
    return ret;
  }
  pbl_sleep_ms(120U);

  ret = pbl_mipi_dsi_detach(&otm->state->dsi);
  if (ret != 0) {
    return ret;
  }

  return pbl_gpio_set(&otm->reset, true);
}

const struct pbl_panel_ops pbl_panel_otm8009a_ops = {
  .prepare = prv_prepare,
  .enable = prv_enable,
  .disable = prv_disable,
  .unprepare = prv_unprepare,
  .set_quarter_turn = prv_set_quarter_turn,
  .set_window = prv_set_window,
  .set_tear = prv_set_tear,
};

int pbl_panel_otm8009a_init(const struct pbl_device *dev) {
  const struct pbl_panel_otm8009a *otm =
      PBL_CONTAINER_OF(dev, const struct pbl_panel_otm8009a, panel.dev);
  int ret;

  otm->state->timing = s_timing;

  ret = pbl_device_init(&otm->reset.port->dev);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_device_init(&otm->state->dsi.host->dev);
  if (ret != 0) {
    return ret;
  }

  return pbl_gpio_configure(&otm->reset, PBL_GPIO_OUTPUT_ACTIVE);
}

static const struct pbl_panel_otm8009a *prv_bl_otm(const struct pbl_backlight *bl) {
  return PBL_CONTAINER_OF(bl, const struct pbl_panel_otm8009a_backlight, backlight)->otm;
}

static int prv_bl_init(const struct pbl_backlight *bl) {
  return 0;
}

// Kept for the next time the panel is set up when it is not
static int prv_bl_set_brightness(const struct pbl_backlight *bl, uint8_t brightness) {
  const struct pbl_panel_otm8009a *otm = prv_bl_otm(bl);

  otm->state->brightness = brightness;
  if (!otm->state->on) {
    return 0;
  }

  return prv_brightness(otm, brightness);
}

const struct pbl_backlight_ops pbl_panel_otm8009a_backlight_ops = {
  .init = prv_bl_init,
  .set_brightness = prv_bl_set_brightness,
};

#include PBL_DT_INSTANCES(orisetech_otm8009a)
