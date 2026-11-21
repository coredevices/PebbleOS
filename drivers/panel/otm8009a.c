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

// Leaving sleep: portrait over the whole panel, 24-bit pixels, full brightness, and no adaptive
// backlight, which would change the brightness with the image
static const uint8_t s_wake[] = {
  0U,
  PBL_MIPI_DCS_EXIT_SLEEP_MODE,
  WAIT(120U),
  W1(PBL_MIPI_DCS_SET_ADDRESS_MODE, 0x00),
  4U,
  PBL_MIPI_DCS_SET_COLUMN_ADDRESS,
  0x00,
  0x00,
  0x01,
  0xdf,
  4U,
  PBL_MIPI_DCS_SET_PAGE_ADDRESS,
  0x00,
  0x00,
  0x03,
  0x1f,
  W1(PBL_MIPI_DCS_SET_PIXEL_FORMAT, COLMOD_RGB888),
  W1(CMD_WRITE_CABC, 0x00),
  W1(PBL_MIPI_DCS_SET_DISPLAY_BRIGHTNESS, 0xff),
  W1(PBL_MIPI_DCS_WRITE_CONTROL_DISPLAY, 0x24),
};

const struct pbl_panel_timing pbl_panel_otm8009a_timing = {
  .clock_hz = 27500000U,
  .hactive = 480U,
  .hsync_len = 2U,
  .hback_porch = 34U,
  .hfront_porch = 34U,
  .vactive = 800U,
  .vsync_len = 1U,
  .vback_porch = 15U,
  .vfront_porch = 16U,
};

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

    ret = pbl_mipi_dsi_dcs_write(&otm->dsi, seq[i + 1U], &seq[i + 2U], n);
    if (ret != 0) {
      return ret;
    }
    i += 2U + n;
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

  ret = pbl_mipi_dsi_attach(&otm->dsi);
  if (ret != 0) {
    return ret;
  }

  ret = prv_run(otm, s_setup, sizeof(s_setup));
  if (ret != 0) {
    return ret;
  }

  return prv_run(otm, s_wake, sizeof(s_wake));
}

static int prv_enable(const struct pbl_panel *panel) {
  const struct pbl_panel_otm8009a *otm = prv_otm(panel);
  int ret;

  ret = pbl_mipi_dsi_dcs_write(&otm->dsi, PBL_MIPI_DCS_SET_DISPLAY_ON, nullptr, 0U);
  if (ret != 0) {
    return ret;
  }

  // The video stream goes to the frame memory
  return pbl_mipi_dsi_dcs_write(&otm->dsi, PBL_MIPI_DCS_WRITE_MEMORY_START, nullptr, 0U);
}

static int prv_disable(const struct pbl_panel *panel) {
  return pbl_mipi_dsi_dcs_write(&prv_otm(panel)->dsi, PBL_MIPI_DCS_SET_DISPLAY_OFF, nullptr, 0U);
}

static int prv_unprepare(const struct pbl_panel *panel) {
  const struct pbl_panel_otm8009a *otm = prv_otm(panel);
  int ret;

  ret = pbl_mipi_dsi_dcs_write(&otm->dsi, PBL_MIPI_DCS_ENTER_SLEEP_MODE, nullptr, 0U);
  if (ret != 0) {
    return ret;
  }
  pbl_sleep_ms(120U);

  ret = pbl_mipi_dsi_detach(&otm->dsi);
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
};

int pbl_panel_otm8009a_init(const struct pbl_device *dev) {
  const struct pbl_panel_otm8009a *otm =
      PBL_CONTAINER_OF(dev, const struct pbl_panel_otm8009a, panel.dev);
  int ret;

  ret = pbl_device_init(&otm->reset.port->dev);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_device_init(&otm->dsi.host->dev);
  if (ret != 0) {
    return ret;
  }

  return pbl_gpio_configure(&otm->reset, PBL_GPIO_OUTPUT_ACTIVE);
}

#include PBL_DT_INSTANCES(orisetech_otm8009a)
