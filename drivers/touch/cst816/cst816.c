/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/drivers/touch/cst816.h>
#include <pbl/input/input.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/time.h>
#include <pbl/logging/logging.h>
#include <pbl/services/analytics/analytics.h>
#include <pbl/services/system_task.h>
#include <pbl/util/bits.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>

#include <cst816_fw.h>

PBL_LOG_MODULE_DEFINE(driver_touch_cst816, CONFIG_DRIVER_TOUCH_LOG_LEVEL);

#define CST816_RESET_CYCLE_TIME_MS     10
#define CST816_POR_DELAY_TIME_MS       110
#define CST816_REG_WR_DELAY_TIME_MS    2
#define CST816_FW_CHECKSUM_CAL_TIME_MS 500

#define CST816_GESTURE_ID_REG       0x01
#define CST816_GESTURE_CLICK        0x05
#define CST816_GESTURE_DOUBLE_CLICK 0x0B
#define CST816_GESTURE_PALM         0xAA
#define CST816_TOUCH_DATA_REG       0x02
#define CST816_TOUCH_DATA_SIZE      5
#define CST816_FINGER_NUM_MASK      PBL_GENMASK(3, 0)
#define CST816_POS_H_MASK           PBL_GENMASK(3, 0)
#define CST816_CHIP_ID_REG          0xA7
#define CST816_FW_VERSION_REG       0xA9
#define CST816_POWER_MODE_REG       0xE5
#define CST816_POWER_MODE_SLEEP     0x03

#define CST816_BOOT_MODE_REG         0xA001
#define CST816_BOOT_MODE_CMD         0xAB
#define CST816_BOOT_FLAG_REG         0xA003
#define CST816_BOOT_FLAG_VAL         0xC1
#define CST816_BOOT_MODE_RETRIES     10
#define CST816_FW_PAGE_DONE_REG      0xA004
#define CST816_FW_PAGE_DONE_CMD      0xEE
#define CST816_FW_PAGE_STATE_REG     0xA005
#define CST816_FW_PAGE_READY         0x55
#define CST816_FW_PAGE_READY_RETRIES 50
#define CST816_BOOT_EXIT_REG         0xA006
#define CST816_BOOT_EXIT_CMD         0xEE
#define CST816_FW_CHECKSUM_REG       0xA008
#define CST816_FW_START_ADDR_REG     0xA014
#define CST816_FW_PAGE_REG           0xA018
#define CST816_FW_PAGE_SIZE          512
#define CST816_FW_WR_TIME_MS         100

#define CST816_FW_HEADER_SIZE    6
#define CST816_FW_VER_INFO_INDEX (-11)

/* The chip stays awake for 2 s after a wake, so an interrupt at least that long after the previous
 * one is a sleep to awake transition. */
#define CST816_WAKE_SPACING_MS 2000

static inline const struct pbl_cst816 *prv_from_touch(const struct pbl_touch *touch) {
  return container_of(touch, const struct pbl_cst816, touch);
}

static int prv_read(const struct pbl_cst816 *cst, uint8_t reg, uint8_t *buf, uint32_t len) {
  int ret;

  ret = pbl_i2c_write(&cst->i2c, &reg, sizeof(reg));
  if (ret != 0) {
    return ret;
  }

  return pbl_i2c_read(&cst->i2c, buf, len);
}

static int prv_write(const struct pbl_cst816 *cst, uint8_t reg, uint8_t val) {
  const uint8_t buf[] = {reg, val};

  return pbl_i2c_write(&cst->i2c, buf, sizeof(buf));
}

static int prv_boot_read(const struct pbl_cst816 *cst, uint16_t reg, uint8_t *buf, uint32_t len) {
  const uint8_t addr[] = {reg >> 8, reg & 0xFF};
  int ret;

  ret = pbl_i2c_write(&cst->i2c_boot, addr, sizeof(addr));
  if (ret != 0) {
    return ret;
  }

  return pbl_i2c_read(&cst->i2c_boot, buf, len);
}

static int prv_boot_write(const struct pbl_cst816 *cst, uint16_t reg, const uint8_t *data,
                          uint32_t len) {
  uint8_t buf[sizeof(reg) + CST816_FW_PAGE_SIZE];

  if (len > CST816_FW_PAGE_SIZE) {
    return -EINVAL;
  }

  buf[0] = reg >> 8;
  buf[1] = reg & 0xFF;
  memcpy(&buf[sizeof(reg)], data, len);

  return pbl_i2c_write(&cst->i2c_boot, buf, sizeof(reg) + len);
}

static int prv_boot_write_byte(const struct pbl_cst816 *cst, uint16_t reg, uint8_t val) {
  return prv_boot_write(cst, reg, &val, sizeof(val));
}

static int prv_hw_reset(const struct pbl_cst816 *cst, uint32_t release_delay_ms) {
  int ret;

  ret = pbl_gpio_set(&cst->reset, true);
  if (ret != 0) {
    return ret;
  }

  pbl_sleep_ms(CST816_RESET_CYCLE_TIME_MS);

  ret = pbl_gpio_set(&cst->reset, false);
  if (ret != 0) {
    return ret;
  }

  pbl_sleep_ms(release_delay_ms);

  return 0;
}

static int prv_enter_bootmode(const struct pbl_cst816 *cst) {
  uint8_t flag;
  int ret;

  ret = prv_hw_reset(cst, CST816_RESET_CYCLE_TIME_MS);
  if (ret != 0) {
    return ret;
  }

  for (uint8_t i = 0U; i < CST816_BOOT_MODE_RETRIES; i++) {
    ret = prv_boot_write_byte(cst, CST816_BOOT_MODE_REG, CST816_BOOT_MODE_CMD);
    pbl_sleep_ms(CST816_REG_WR_DELAY_TIME_MS);
    if (ret != 0) {
      continue;
    }

    ret = prv_boot_read(cst, CST816_BOOT_FLAG_REG, &flag, sizeof(flag));
    pbl_sleep_ms(CST816_REG_WR_DELAY_TIME_MS);
    if (ret == 0 && flag == CST816_BOOT_FLAG_VAL) {
      return 0;
    }
  }

  return -ENODEV;
}

static int prv_read_checksum(const struct pbl_cst816 *cst, uint16_t *checksum) {
  uint8_t data[sizeof(*checksum)];
  int ret;

  ret = prv_boot_write_byte(cst, CST816_BOOT_FLAG_REG, 0U);
  if (ret != 0) {
    return ret;
  }

  pbl_sleep_ms(CST816_FW_CHECKSUM_CAL_TIME_MS);

  ret = prv_boot_read(cst, CST816_FW_CHECKSUM_REG, data, sizeof(data));
  if (ret != 0) {
    return ret;
  }

  *checksum = ((uint16_t)data[1] << 8) | data[0];

  return 0;
}

static int prv_wait_page_ready(const struct pbl_cst816 *cst) {
  uint8_t state;
  int ret;

  pbl_sleep_ms(CST816_FW_WR_TIME_MS);

  for (uint8_t i = 0U; i < CST816_FW_PAGE_READY_RETRIES; i++) {
    pbl_sleep_ms(CST816_RESET_CYCLE_TIME_MS);
    ret = prv_boot_read(cst, CST816_FW_PAGE_STATE_REG, &state, sizeof(state));
    if (ret == 0 && state == CST816_FW_PAGE_READY) {
      return 0;
    }
  }

  return -ETIMEDOUT;
}

static int prv_fw_update(const struct pbl_cst816 *cst) {
  uint16_t start_addr = ((uint16_t)app_bin[1] << 8) | app_bin[0];
  uint16_t length = ((uint16_t)app_bin[3] << 8) | app_bin[2];
  const uint16_t checksum = ((uint16_t)app_bin[5] << 8) | app_bin[4];
  const uint8_t *page = &app_bin[CST816_FW_HEADER_SIZE];
  uint16_t checksum_read;
  uint16_t page_len;
  int ret;

  while (length > 0U) {
    const uint8_t addr[] = {start_addr & 0xFF, start_addr >> 8};

    page_len = MIN(length, CST816_FW_PAGE_SIZE);
    PBL_LOG_DBG("Writing 0x%04x, %u bytes", start_addr, page_len);

    ret = prv_boot_write(cst, CST816_FW_START_ADDR_REG, addr, sizeof(addr));
    if (ret != 0) {
      return ret;
    }
    pbl_sleep_ms(CST816_REG_WR_DELAY_TIME_MS);

    ret = prv_boot_write(cst, CST816_FW_PAGE_REG, page, page_len);
    if (ret != 0) {
      return ret;
    }
    pbl_sleep_ms(CST816_REG_WR_DELAY_TIME_MS);

    ret = prv_boot_write_byte(cst, CST816_FW_PAGE_DONE_REG, CST816_FW_PAGE_DONE_CMD);
    if (ret != 0) {
      return ret;
    }

    ret = prv_wait_page_ready(cst);
    if (ret != 0) {
      return ret;
    }

    page += page_len;
    start_addr += page_len;
    length -= page_len;
  }

  ret = prv_read_checksum(cst, &checksum_read);
  if (ret != 0) {
    return ret;
  }

  if (checksum_read != checksum) {
    PBL_LOG_ERR("Firmware checksum 0x%04x, expected 0x%04x", checksum_read, checksum);
    return -EIO;
  }

  ret = prv_boot_write_byte(cst, CST816_BOOT_EXIT_REG, CST816_BOOT_EXIT_CMD);
  if (ret != 0) {
    return ret;
  }

  PBL_LOG_INFO("Updated firmware to version 0x%02X (0x%04X)",
               app_bin[sizeof(app_bin) + CST816_FW_VER_INFO_INDEX], checksum_read);

  return prv_hw_reset(cst, CST816_POR_DELAY_TIME_MS);
}

static int prv_update_if_needed(const struct pbl_cst816 *cst) {
  const uint8_t target_version = app_bin[sizeof(app_bin) + CST816_FW_VER_INFO_INDEX];
  uint8_t chip_id;
  uint8_t version;
  int ret;

  ret = prv_read(cst, CST816_CHIP_ID_REG, &chip_id, sizeof(chip_id));
  if (ret == 0) {
    ret = prv_read(cst, CST816_FW_VERSION_REG, &version, sizeof(version));
  }

  if (ret == 0) {
    PBL_LOG_DBG("Chip ID 0x%02X, firmware 0x%02X", chip_id, version);
    if (version == target_version) {
      return 0;
    }
  }

  // A chip stranded in boot mode by an interrupted update stops answering at the normal address;
  // only a reflash brings it back, so update even when the probe failed.
  ret = prv_enter_bootmode(cst);
  if (ret != 0) {
    return ret;
  }

  return prv_fw_update(cst);
}

static void prv_count_irq(struct pbl_cst816_state *state) {
  const uint64_t now = pbl_uptime_ticks();
  const uint64_t wake_spacing = pbl_ms_to_ticks(CST816_WAKE_SPACING_MS);
  const uint64_t gap = now - state->last_irq_ticks;

  if (gap >= wake_spacing) {
    PBL_ANALYTICS_ADD(touch_driver_wake_cnt, 1);
  }

  // Each interrupt keeps the chip in active scan for the wake window.
  if (state->last_irq_ticks != 0U) {
    PBL_ANALYTICS_ADD(touch_driver_awake_time_ms, pbl_ticks_to_ms(MIN(gap, wake_spacing)));
  }

  state->last_irq_ticks = now;
}

static int prv_wake(const struct pbl_cst816 *cst) {
  struct pbl_cst816_state *state = cst->state;
  int ret;

  ret = prv_hw_reset(cst, CST816_POR_DELAY_TIME_MS);
  if (ret != 0) {
    return ret;
  }

  state->palm_down = false;
  state->palm_click_pending = false;
  return pbl_intc_enable(&state->irq);
}

static void prv_work(void *arg) {
  const struct pbl_cst816 *cst = arg;
  struct pbl_cst816_state *state = cst->state;
  uint8_t data[CST816_TOUCH_DATA_SIZE];
  uint16_t gesture_code = 0U;
  bool report_gesture = false;
  bool report_palm = false;
  uint8_t gesture;
  uint16_t x;
  uint16_t y;
  bool down;
  int ret;

  pbl_touch_lock(&cst->touch);

  state->work_pending = false;
  if (!state->enabled) {
    pbl_touch_unlock(&cst->touch);
    return;
  }

  prv_count_irq(state);

  ret = prv_read(cst, CST816_GESTURE_ID_REG, &gesture, sizeof(gesture));
  if (ret == 0) {
    ret = prv_read(cst, CST816_TOUCH_DATA_REG, data, sizeof(data));
  }

  if (ret != 0) {
    // ESD and other transients can leave the chip deaf to I2C until it is reset.
    PBL_LOG_ERR("Read failed (%d), resetting", ret);
    (void)pbl_intc_disable(&state->irq);
    ret = prv_wake(cst);
    if (ret != 0) {
      PBL_LOG_ERR("Reset failed (%d)", ret);
    }
    pbl_touch_unlock(&cst->touch);
    pbl_touch_report(&cst->touch, false, 0U, 0U);
    return;
  }

  down = PBL_FIELD_GET(CST816_FINGER_NUM_MASK, data[0]) == 1U;
  x = (PBL_FIELD_GET(CST816_POS_H_MASK, data[1]) << 8) | data[2];
  y = (PBL_FIELD_GET(CST816_POS_H_MASK, data[3]) << 8) | data[4];

  // The chip reports a click well after a palm lifts; drop it unless a new contact started in
  // between.
  if (down && gesture != CST816_GESTURE_PALM) {
    state->palm_click_pending = false;
  } else if (state->palm_click_pending &&
             (gesture == CST816_GESTURE_CLICK || gesture == CST816_GESTURE_DOUBLE_CLICK)) {
    state->palm_click_pending = false;
    gesture = 0U;
  }

  if (gesture == CST816_GESTURE_CLICK) {
    gesture_code = PBL_INPUT_GES_TAP;
    report_gesture = true;
  } else if (gesture == CST816_GESTURE_DOUBLE_CLICK) {
    gesture_code = PBL_INPUT_GES_DOUBLE_TAP;
    report_gesture = true;
  }

  // The gesture register holds its last value, so a palm reads back as one on every sample it
  // produces, its liftoff included; latch it instead.
  if (!down) {
    state->palm_down = false;
  } else if (gesture == CST816_GESTURE_PALM && !state->palm_down) {
    state->palm_down = true;
    state->palm_click_pending = true;
    report_palm = true;
  }

  // A palm also reports a contact at its centroid, which would otherwise land as a tap on whatever
  // is underneath it.
  down = down && !state->palm_down;

  pbl_touch_unlock(&cst->touch);

  if (report_gesture) {
    pbl_touch_report_gesture(&cst->touch, gesture_code, x, y);
  }

  if (report_palm) {
    pbl_touch_report_gesture(&cst->touch, PBL_INPUT_GES_PALM, x, y);
  }

  pbl_touch_report(&cst->touch, down, x, y);
}

static void prv_irq_handler(void *arg) {
  const struct pbl_cst816 *cst = arg;
  struct pbl_cst816_state *state = cst->state;

  if (state->work_pending) {
    return;
  }

  state->work_pending = true;
  if (!system_task_add_callback_from_isr_droppable_raised(prv_work, arg)) {
    state->work_pending = false;
  }
}

static int prv_sleep(const struct pbl_cst816 *cst, bool awake) {
  struct pbl_cst816_state *state = cst->state;
  int ret;

  state->enabled = false;

  ret = pbl_intc_disable(&state->irq);
  if (ret != 0) {
    return ret;
  }

  // The chip only takes the command while awake.
  if (!awake) {
    ret = prv_hw_reset(cst, CST816_POR_DELAY_TIME_MS);
    if (ret != 0) {
      return ret;
    }
  }

  return prv_write(cst, CST816_POWER_MODE_REG, CST816_POWER_MODE_SLEEP);
}

static int prv_init(const struct pbl_touch *touch) {
  const struct pbl_cst816 *cst = prv_from_touch(touch);
  struct pbl_cst816_state *state = cst->state;
  int ret;

  ret = pbl_device_init(&cst->reset.port->dev);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_gpio_configure(&cst->reset, PBL_GPIO_OUTPUT);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_device_init(&cst->int_gpio.port->dev);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_gpio_configure(&cst->int_gpio, PBL_GPIO_INPUT);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_gpio_to_irq(&cst->int_gpio, PBL_IRQ_TYPE_EDGE_FALLING, &state->irq);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_device_init(&state->irq.intc->dev);
  if (ret != 0) {
    return ret;
  }

  ret = prv_hw_reset(cst, CST816_POR_DELAY_TIME_MS);
  if (ret != 0) {
    return ret;
  }

  ret = prv_update_if_needed(cst);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_intc_request(&state->irq, prv_irq_handler, (void *)cst);
  if (ret != 0) {
    return ret;
  }

  return prv_sleep(cst, true);
}

static int prv_enable(const struct pbl_touch *touch) {
  const struct pbl_cst816 *cst = prv_from_touch(touch);
  struct pbl_cst816_state *state = cst->state;
  int ret;

  ret = prv_wake(cst);
  if (ret != 0) {
    return ret;
  }

  state->enabled = true;

  return 0;
}

static int prv_disable(const struct pbl_touch *touch) {
  return prv_sleep(prv_from_touch(touch), false);
}

const struct pbl_touch_ops pbl_cst816_ops = {
  .init = prv_init,
  .enable = prv_enable,
  .disable = prv_disable,
};
