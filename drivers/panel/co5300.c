/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/panel/co5300.h>
#include <pbl/kernel/delay.h>
#include <pbl/logging/logging.h>
#include <pbl/util/misc.h>
#include <pbl/util/size.h>

PBL_LOG_MODULE_DECLARE(driver_panel, CONFIG_DRIVER_PANEL_LOG_LEVEL);

#define CO5300_RESET_MS      10U
#define CO5300_RESET_WAIT_MS 50U
#define CO5300_SLEEP_OUT_MS  120U
#define CO5300_DISPLAY_ON_MS 70U
#define CO5300_SLEEP_IN_MS   120U

#define CO5300_CMD_PAGE      0xFEU
#define CO5300_CMD_PASSWD1   0xF4U
#define CO5300_CMD_PASSWD2   0xF5U
#define CO5300_CMD_SPI_MODE  0xC4U
#define CO5300_CMD_HBM_LEVEL 0x63U

#define CO5300_PAGE_USER       0x00U
#define CO5300_PAGE_PASSWD     0x20U
#define CO5300_SPI_MODE_QSPI   0x80U
#define CO5300_PIXEL_RGB565    0x55U
#define CO5300_TEAR_VBLANK     0x00U
#define CO5300_CTRL_BRIGHTNESS 0x20U
#define CO5300_HBM_MAX         0xFFU
#define CO5300_BRIGHTNESS_MAX  0xFFU

#define CO5300_ADDRESS_MODE_H_FLIP 0x02U

struct prv_cmd {
  uint8_t cmd;
  uint8_t param;
};

static const struct prv_cmd s_init_seq[] = {
  // Unlock, then lock again, the manufacturer command page
  {CO5300_CMD_PAGE, CO5300_PAGE_PASSWD},
  {CO5300_CMD_PASSWD1, 0x5AU},
  {CO5300_CMD_PASSWD2, 0x59U},
  {CO5300_CMD_PAGE, CO5300_PAGE_PASSWD},
  {CO5300_CMD_PASSWD1, 0xA5U},
  {CO5300_CMD_PASSWD2, 0xA5U},
  {CO5300_CMD_PAGE, CO5300_PAGE_USER},
  {CO5300_CMD_SPI_MODE, CO5300_SPI_MODE_QSPI},
  {PBL_MIPI_DCS_SET_PIXEL_FORMAT, CO5300_PIXEL_RGB565},
  {PBL_MIPI_DCS_SET_TEAR_ON, CO5300_TEAR_VBLANK},
  {PBL_MIPI_DCS_WRITE_CONTROL_DISPLAY, CO5300_CTRL_BRIGHTNESS},
  {CO5300_CMD_HBM_LEVEL, CO5300_HBM_MAX},
};

static const struct pbl_panel_co5300 *prv_co5300(const struct pbl_panel *panel) {
  return PBL_CONTAINER_OF(panel, const struct pbl_panel_co5300, panel);
}

static int prv_write(const struct pbl_panel_co5300 *co5300, uint8_t cmd, const uint8_t *data,
                     size_t len) {
  return pbl_mipi_dbi_command_write(co5300->dbi, cmd, data, len);
}

static int prv_set_address_mode(const struct pbl_panel_co5300 *co5300) {
  uint8_t mode = co5300->state->h_flip ? CO5300_ADDRESS_MODE_H_FLIP : 0U;

  return prv_write(co5300, PBL_MIPI_DCS_SET_ADDRESS_MODE, &mode, sizeof(mode));
}

static int prv_set_brightness(const struct pbl_panel_co5300 *co5300, uint8_t brightness) {
  uint8_t level = (uint8_t)((brightness * CO5300_BRIGHTNESS_MAX) / PBL_BACKLIGHT_BRIGHTNESS_MAX);

  return prv_write(co5300, PBL_MIPI_DCS_SET_DISPLAY_BRIGHTNESS, &level, sizeof(level));
}

int pbl_panel_co5300_init(const struct pbl_device *dev) {
  const struct pbl_panel_co5300 *co5300 =
      PBL_CONTAINER_OF(dev, const struct pbl_panel_co5300, panel.dev);
  int ret;

  ret = pbl_device_init(&co5300->reset.port->dev);
  if (ret != 0) {
    return ret;
  }

  return pbl_gpio_configure(&co5300->reset, PBL_GPIO_OUTPUT_ACTIVE);
}

static int prv_prepare(const struct pbl_panel *panel) {
  const struct pbl_panel_co5300 *co5300 = prv_co5300(panel);
  const struct pbl_panel_timing *timing = panel->timing;
  uint8_t id[3];
  int ret;

  ret = pbl_gpio_set(&co5300->reset, true);
  if (ret != 0) {
    return ret;
  }

  pbl_sleep_ms(CO5300_RESET_MS);

  ret = pbl_gpio_set(&co5300->reset, false);
  if (ret != 0) {
    return ret;
  }

  pbl_sleep_ms(CO5300_RESET_WAIT_MS);

  ret = pbl_mipi_dbi_command_read(co5300->dbi, PBL_MIPI_DCS_GET_DISPLAY_ID, id, sizeof(id));
  if (ret != 0) {
    return ret;
  }

  PBL_LOG_DBG("ID %02x %02x %02x", id[0], id[1], id[2]);

  for (size_t i = 0; i < PBL_ARRAY_SIZE(s_init_seq); i++) {
    ret = prv_write(co5300, s_init_seq[i].cmd, &s_init_seq[i].param, 1U);
    if (ret != 0) {
      return ret;
    }
  }

  ret = prv_set_address_mode(co5300);
  if (ret != 0) {
    return ret;
  }

  const uint16_t x1 = timing->hback_porch + timing->hactive - 1U;
  const uint16_t y1 = timing->vback_porch + timing->vactive - 1U;
  const uint8_t cols[] = {
    timing->hback_porch >> 8, timing->hback_porch & 0xFFU, x1 >> 8, x1 & 0xFFU
  };
  const uint8_t rows[] = {
    timing->vback_porch >> 8, timing->vback_porch & 0xFFU, y1 >> 8, y1 & 0xFFU
  };

  ret = prv_write(co5300, PBL_MIPI_DCS_SET_COLUMN_ADDRESS, cols, sizeof(cols));
  if (ret != 0) {
    return ret;
  }

  ret = prv_write(co5300, PBL_MIPI_DCS_SET_PAGE_ADDRESS, rows, sizeof(rows));
  if (ret != 0) {
    return ret;
  }

  ret = prv_write(co5300, PBL_MIPI_DCS_EXIT_SLEEP_MODE, nullptr, 0U);
  if (ret != 0) {
    return ret;
  }

  pbl_sleep_ms(CO5300_SLEEP_OUT_MS);

  co5300->state->awake = true;

  return 0;
}

static int prv_enable(const struct pbl_panel *panel) {
  const struct pbl_panel_co5300 *co5300 = prv_co5300(panel);
  struct pbl_panel_co5300_state *state = co5300->state;
  int ret;

  ret = prv_set_brightness(co5300, state->brightness);
  if (ret != 0) {
    return ret;
  }

  ret = prv_write(co5300, PBL_MIPI_DCS_SET_DISPLAY_ON, nullptr, 0U);
  if (ret != 0) {
    return ret;
  }

  pbl_sleep_ms(CO5300_DISPLAY_ON_MS);

  state->on = true;

  return 0;
}

static int prv_disable(const struct pbl_panel *panel) {
  const struct pbl_panel_co5300 *co5300 = prv_co5300(panel);

  co5300->state->on = false;

  return prv_write(co5300, PBL_MIPI_DCS_SET_DISPLAY_OFF, nullptr, 0U);
}

static int prv_unprepare(const struct pbl_panel *panel) {
  const struct pbl_panel_co5300 *co5300 = prv_co5300(panel);
  int ret;

  // Before any prepare, as when its controller power cycles it at init, the IC is just reset
  if (co5300->state->awake) {
    co5300->state->awake = false;
    ret = prv_write(co5300, PBL_MIPI_DCS_ENTER_SLEEP_MODE, nullptr, 0U);
    if (ret != 0) {
      return ret;
    }

    pbl_sleep_ms(CO5300_SLEEP_IN_MS);
  }

  return pbl_gpio_set(&co5300->reset, true);
}

static int prv_set_h_flip(const struct pbl_panel *panel, bool flip) {
  const struct pbl_panel_co5300 *co5300 = prv_co5300(panel);
  struct pbl_panel_co5300_state *state = co5300->state;

  state->h_flip = flip;

  // A panel that is down takes it as it comes up
  return state->awake ? prv_set_address_mode(co5300) : 0;
}

const struct pbl_panel_ops pbl_panel_co5300_ops = {
  .prepare = prv_prepare,
  .enable = prv_enable,
  .disable = prv_disable,
  .unprepare = prv_unprepare,
  .set_h_flip = prv_set_h_flip,
};

static const struct pbl_panel_co5300 *prv_bl_co5300(const struct pbl_backlight *bl) {
  return PBL_CONTAINER_OF(bl, const struct pbl_panel_co5300_backlight, backlight)->co5300;
}

static int prv_bl_init(const struct pbl_backlight *bl) {
  return 0;
}

static int prv_bl_set_brightness(const struct pbl_backlight *bl, uint8_t brightness) {
  const struct pbl_panel_co5300 *co5300 = prv_bl_co5300(bl);
  struct pbl_panel_co5300_state *state = co5300->state;

  state->brightness = brightness;
  if (!state->on) {
    return 0;
  }

  return prv_set_brightness(co5300, brightness);
}

const struct pbl_backlight_ops pbl_panel_co5300_backlight_ops = {
  .init = prv_bl_init,
  .set_brightness = prv_bl_set_brightness,
};

#include PBL_DT_INSTANCES(chipone_co5300)
