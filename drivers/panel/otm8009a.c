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

#define CMD(...)      (uint8_t)(sizeof((const uint8_t[]){__VA_ARGS__}) - 1U), __VA_ARGS__
#define AT(addr, ...) CMD(CMD_ADDRESS_SHIFT, (addr)), CMD(__VA_ARGS__)
#define WAIT(ms)      DELAY, (ms)

// The vendor setup of the panel, before it leaves sleep: power, source driver, GOA timing and
// gamma, as the panel maker gives them, each register at the low byte of its address
// clang-format off
static const uint8_t s_setup[] = {
  // Into the second parameter set
  AT(0x00, CMD_ENABLE_CMD2, 0x80, 0x09, 0x01),
  AT(0x80, CMD_ENABLE_CMD2, 0x80, 0x09),
  // Source output at GND in the porches and outside the display area
  AT(0x80, 0xc4, 0x30), WAIT(10),
  AT(0x8a, 0xc4, 0x40), WAIT(10),
  // GVDD test mode
  AT(0xb1, 0xc5, 0xa9),
  // Pump 4 and 5 VGH and VGL
  AT(0x91, 0xc5, 0x34),
  // Column inversion
  AT(0xb4, 0xc0, 0x50),
  // VCOM
  AT(0x00, 0xd9, 0x4e),
  // Oscillator at 65 Hz
  AT(0x81, 0xc1, 0x66),
  // Internal video mode
  AT(0xa1, 0xc1, 0x08),
  // Pumps 4 and 5 at x6
  AT(0x92, 0xc5, 0x01),
  // Pump 4 clock at half a line
  AT(0x95, 0xc5, 0x34),
  // GVDD and NGVDD
  AT(0x00, 0xd8, 0x79, 0x79),
  AT(0x94, 0xc5, 0x33),
  // Panel display timing
  AT(0xa3, 0xc0, 0x1b),
  // Power control
  AT(0x82, 0xc5, 0x83),
  // Source driver precharge
  AT(0x81, 0xc4, 0x83),
  AT(0xa1, 0xc1, 0x0e),
  AT(0xa6, 0xb3, 0x00, 0x01),
  // GOA timing
  AT(0x80, 0xce, 0x85, 0x01, 0x00, 0x84, 0x01, 0x00),
  AT(0xa0, 0xce, 0x18, 0x04, 0x03, 0x39, 0x00, 0x00, 0x00, 0x18,
     0x03, 0x03, 0x3a, 0x00, 0x00, 0x00),
  AT(0xb0, 0xce, 0x18, 0x02, 0x03, 0x3b, 0x00, 0x00, 0x00, 0x18,
     0x01, 0x03, 0x3c, 0x00, 0x00, 0x00),
  AT(0xc0, 0xcf, 0x01, 0x01, 0x20, 0x20, 0x00, 0x00, 0x01, 0x02, 0x00, 0x00),
  AT(0xd0, 0xcf, 0x00),
  // GOA panel mapping
  AT(0x80, 0xcb, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),
  AT(0x90, 0xcb, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),
  AT(0xa0, 0xcb, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),
  AT(0xb0, 0xcb, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),
  AT(0xc0, 0xcb, 0x00, 0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),
  AT(0xd0, 0xcb, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x04,
     0x04, 0x04, 0x04, 0x00, 0x00, 0x00, 0x00),
  AT(0xe0, 0xcb, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),
  AT(0xf0, 0xcb, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff),
  AT(0x80, 0xcc, 0x00, 0x26, 0x09, 0x0b, 0x01, 0x25, 0x00, 0x00, 0x00, 0x00),
  AT(0x90, 0xcc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x26, 0x0a, 0x0c, 0x02),
  AT(0xa0, 0xcc, 0x25, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),
  AT(0xb0, 0xcc, 0x00, 0x25, 0x0c, 0x0a, 0x02, 0x26, 0x00, 0x00, 0x00, 0x00),
  AT(0xc0, 0xcc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x25, 0x0b, 0x09, 0x01),
  AT(0xd0, 0xcc, 0x26, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),
  // Pump 1 minimum and maximum
  AT(0x81, 0xc5, 0x66),
  AT(0xb6, 0xf5, 0x06),
  // CABC PWM at 19.5 kHz
  AT(0xb1, 0xc6, 0x06),
  // Out of the second parameter set
  AT(0x00, CMD_ENABLE_CMD2, 0xff, 0xff, 0xff),
  // Gamma 2.2, positive and negative
  AT(0x00, CMD_GAMMA_POS, 0x00, 0x09, 0x0f, 0x0e, 0x07, 0x10, 0x0b, 0x0a,
     0x04, 0x07, 0x0b, 0x08, 0x0f, 0x10, 0x0a, 0x01),
  AT(0x00, CMD_GAMMA_NEG, 0x00, 0x09, 0x0f, 0x0e, 0x07, 0x10, 0x0b, 0x0a,
     0x04, 0x07, 0x0b, 0x08, 0x0f, 0x10, 0x0a, 0x01),
};
// clang-format on

// Address modes: rows and columns exchanged, columns right to left, rows bottom to top
#define MADCTL_MV 0x20U
#define MADCTL_MX 0x40U
#define MADCTL_MY 0x80U

// The panel is portrait, and only takes frames into its frame memory
static const struct pbl_panel_timing s_timing = {
  .hactive = 480U,
  .vactive = 800U,
};

// Where the scan is far enough for the first half of a frame turned a quarter turn to be written
// behind it, and the second half once the scan has wrapped
#define TEAR_SCANLINE 533U

// Leaving sleep: 24-bit pixels, no adaptive backlight, which would change the brightness with the
// image, and the tearing effect at its scanline
// clang-format off
static const uint8_t s_wake[] = {
  CMD(PBL_MIPI_DCS_EXIT_SLEEP_MODE), WAIT(120),
  CMD(PBL_MIPI_DCS_SET_PIXEL_FORMAT, COLMOD_RGB888),
  CMD(CMD_WRITE_CABC, 0x00),
  CMD(PBL_MIPI_DCS_WRITE_CONTROL_DISPLAY, 0x24),
  CMD(PBL_MIPI_DCS_SET_TEAR_SCANLINE, TEAR_SCANLINE >> 8, TEAR_SCANLINE & 0xffU),
  CMD(PBL_MIPI_DCS_SET_TEAR_ON, 0x00),
};
// clang-format on

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

// The address mode of a rotation, counterclockwise, with its reflections
static uint8_t prv_madctl(uint32_t rotation) {
  uint8_t mode = 0U;

  if ((rotation & PBL_PANEL_ROTATE_90) != 0U) {
    mode = MADCTL_MV | MADCTL_MY;
  } else if ((rotation & PBL_PANEL_ROTATE_180) != 0U) {
    mode = MADCTL_MX | MADCTL_MY;
  } else if ((rotation & PBL_PANEL_ROTATE_270) != 0U) {
    mode = MADCTL_MV | MADCTL_MX;
  }

  // A reflection mirrors the frame before it is rotated: the axes swap with a quarter turn
  if ((rotation & PBL_PANEL_REFLECT_X) != 0U) {
    mode ^= (mode & MADCTL_MV) != 0U ? MADCTL_MY : MADCTL_MX;
  }
  if ((rotation & PBL_PANEL_REFLECT_Y) != 0U) {
    mode ^= (mode & MADCTL_MV) != 0U ? MADCTL_MX : MADCTL_MY;
  }

  return mode;
}

// The whole panel, in the orientation of the frame
static int prv_address(const struct pbl_panel_otm8009a *otm) {
  const struct pbl_panel_timing *t = &otm->state->timing;
  const uint8_t mode = prv_madctl(otm->state->rotation);
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

// A frame turned a quarter turn has the active width and height of the panel exchanged
static int prv_set_rotation(const struct pbl_panel *panel, uint32_t rotation) {
  const struct pbl_panel_otm8009a *otm = prv_otm(panel);
  struct pbl_panel_otm8009a_state *st = otm->state;
  const bool turned = (rotation & (PBL_PANEL_ROTATE_90 | PBL_PANEL_ROTATE_270)) != 0U;

  st->rotation = rotation;
  st->timing = s_timing;
  if (turned) {
    st->timing.hactive = s_timing.vactive;
    st->timing.vactive = s_timing.hactive;
  }

  // A panel that is down takes it as it comes up
  return st->on ? prv_address(otm) : 0;
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
  .set_rotation = prv_set_rotation,
  .set_window = prv_set_window,
};

int pbl_panel_otm8009a_init(const struct pbl_device *dev) {
  const struct pbl_panel_otm8009a *otm =
      PBL_CONTAINER_OF(dev, const struct pbl_panel_otm8009a, panel.dev);
  int ret;

  otm->state->timing = s_timing;
  otm->state->rotation = PBL_PANEL_ROTATE_0;

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
