/*
 * Copyright 2025 Core Devices LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "board/board.h"
#include "drivers/exti.h"
#include "drivers/gpio.h"
#include "drivers/i2c.h"
#include "kernel/events.h"
#include "kernel/util/sleep.h"
#include "os/tick.h"
#include "services/common/touch/touch.h"
#include "services/common/system_task.h"
#include "system/logging.h"
#include "system/passert.h"
#include "cst816_fw.h"

#define CST816_RESET_CYCLE_TIME       10  /* ms */
#define CST816_POR_DELAY_TIME         110 /* ms */
#define CST816_REG_WR_DELAY_TIME      2   /* ms */ 
#define CST816_FW_CHECKSUM_CAL_TIME   500 /* ms */ 

#define CST816_CHIP_ID_REG            0xA7
#define CST816_FW_VERSION_REG         0xA9
#define CST816_TOUCH_DATA_REG         0x02
#define CST816_TOUCH_DATA_SIZE        5

#define CST816_BOOT_MODE_REG          0xA001
#define CST816_BOOT_MODE_CMD          0xAB
#define CST816_BOOT_FLAG_REG          0xA003
#define CST816_BOOT_FLAG_VAL          0xC1
#define CST816_FW_START_ADDR_REG      0xA014
#define CST816_FW_PAGE_REG            0xA018
#define CST816_FW_PAGE_SIZE           512
#define CST816_FW_PAGE_DONE           0xA004
#define CST816_FW_PAGE_STATE          0xA005
#define CST816_BOOT_EXIT_REG          0xA006
#define CST816_BOOT_EXIT_VAL          0xEE
#define CST816_FW_PAGE_READY          0x55
#define CST816_FW_WR_TIME             100 /* ms */
#define CST816_FW_CHECKSUM_REG        0xA008
#define CST816_FW_VER_INFO_INDEX      (-11)

static bool s_callback_scheduled = false;
static PebbleMutex *s_i2c_lock;

static void prv_exti_cb(bool *should_context_switch);
static void cst816_hw_reset(void);

static bool prv_read_data(uint16_t register_address, uint8_t *result, uint16_t size, bool is_work_mode) {
  mutex_lock(s_i2c_lock);
  I2CSlavePort* port = CST816->i2c;
  uint8_t addr_size = 1;
  if(!is_work_mode) {
    port = CST816->i2c_boot;
    addr_size = 2;
  }
  i2c_use(port);
  uint8_t regad[2] = { register_address >> 8, register_address & 0xFF };
  bool rv = i2c_write_block(port, addr_size, is_work_mode?regad+1:regad);
  if (rv) {
    rv = i2c_read_block(port, size, result);
  }
  i2c_release(port);
  mutex_unlock(s_i2c_lock);
  return rv;
}

static bool prv_write_data(uint16_t register_address, const uint8_t *datum, uint16_t size, bool is_work_mode) {
  mutex_lock(s_i2c_lock);
  I2CSlavePort* port = CST816->i2c;
  uint8_t addr_size = 1;
  if(!is_work_mode) {
    port = CST816->i2c_boot;
    addr_size = 2;
  }
  i2c_use(port);
  uint8_t data[size + sizeof(register_address)];
  data[0] = register_address >> 8;
  data[1] = register_address & 0xFF;
  memcpy(data+sizeof(register_address), datum, size);
  bool rv = i2c_write_block(port, size+addr_size, is_work_mode?data+1:data);
  i2c_release(port);
  mutex_unlock(s_i2c_lock);
  return rv;
}

static bool cst816_enter_bootmode(void) {
#if RESET_PIN_CTRLBY_NPM1300
  NPM1300_OPS.gpio_set(Npm1300_Gpio2, 0);
  psleep(CST816_RESET_CYCLE_TIME);
  NPM1300_OPS.gpio_set(Npm1300_Gpio2, 1);
  psleep(CST816_RESET_CYCLE_TIME);
#endif

  uint8_t retry_cnt = 10;
  while (retry_cnt--) {
    uint8_t cmd = CST816_BOOT_MODE_CMD;
    bool rv = prv_write_data(CST816_BOOT_MODE_REG, &cmd, 1, 0);
    psleep(CST816_REG_WR_DELAY_TIME);
    rv &= prv_read_data(CST816_BOOT_FLAG_REG, &cmd, 1, 0);
    psleep(CST816_REG_WR_DELAY_TIME);

    if (cmd == CST816_BOOT_FLAG_VAL) {
      return true;
    }
  }

  return false;
}

static uint16_t cst816_read_checksum(void)
{
  uint8_t cmd = 0;
  bool rv = prv_write_data(CST816_BOOT_FLAG_REG, &cmd, 1, 0);
  psleep(CST816_FW_CHECKSUM_CAL_TIME);

  uint8_t data[2];
  rv &= prv_read_data(CST816_FW_CHECKSUM_REG, data, 2, 0);
  PBL_ASSERT(rv, "get checksum error");
  uint16_t checksum = (((uint16_t)(data[1] & 0xFF)) << 8) | data[0];

  return checksum;
}

static bool cst816_fw_update(void) {
  if (sizeof(app_bin) > 10) {
    uint16_t start_addr = (((uint16_t)(app_bin[1] & 0xFF)) << 8) | app_bin[0];
    uint16_t length = (((uint16_t)(app_bin[3] & 0xFF)) << 8) | app_bin[2];
    uint16_t checksum = (((uint16_t)(app_bin[5] & 0xFF)) << 8) | app_bin[4];
    uint16_t fw_offset = 6;
    
    while (length) {
      PBL_LOG(LOG_LEVEL_DEBUG, "fw start_addr:%d length:%d", start_addr, length);
      uint8_t addr[2] = {start_addr&0xff, start_addr>>8};
      bool rv = prv_write_data(CST816_FW_START_ADDR_REG, addr, 2, 0);
      psleep(CST816_REG_WR_DELAY_TIME);
      if(!prv_write_data(CST816_FW_PAGE_REG, app_bin+fw_offset,
                        length>=CST816_FW_PAGE_SIZE?CST816_FW_PAGE_SIZE:length , 0)) {
        PBL_LOG(LOG_LEVEL_ERROR, "cst816 update fw error by iic");
        return false;
      }
      psleep(CST816_REG_WR_DELAY_TIME);
      uint8_t cmd = 0xEE;
      rv = prv_write_data(CST816_FW_PAGE_DONE, &cmd, 1, 0);
      psleep(CST816_FW_WR_TIME);
      for (int t=0;; t++) {
        if(t > 50) {
          PBL_LOG(LOG_LEVEL_ERROR, "cst816 update fw error by writing timeout");
          return false;
        }
        psleep(CST816_RESET_CYCLE_TIME);
        uint8_t ready;
        rv = prv_read_data(CST816_FW_PAGE_STATE, &ready, 1, 0);
        if (rv && ready == CST816_FW_PAGE_READY) {
          break;
        }
      }
      fw_offset += CST816_FW_PAGE_SIZE;
      start_addr += CST816_FW_PAGE_SIZE;
      length -= length>=CST816_FW_PAGE_SIZE?CST816_FW_PAGE_SIZE:length;
    }

    uint16_t checksum_read = cst816_read_checksum();
    if (checksum_read == checksum) {
      uint8_t boot_exit_cmd = CST816_BOOT_EXIT_VAL;
      bool rv = prv_write_data(CST816_BOOT_EXIT_REG, &boot_exit_cmd, 1, 0);
      if (!rv) {
        PBL_LOG(LOG_LEVEL_ERROR, "exit boot failed");
        return false;
      }

      cst816_hw_reset();
      return true;
    }
    PBL_LOG(LOG_LEVEL_ERROR, "cst816 update fw error by checksum:%x read:%x", checksum, checksum_read);
  }

  return false;
}

static void cst816_hw_reset(void) {
#if RESET_PIN_CTRLBY_NPM1300
  NPM1300_OPS.gpio_set(Npm1300_Gpio2, 0);
  psleep(CST816_RESET_CYCLE_TIME);
  NPM1300_OPS.gpio_set(Npm1300_Gpio2, 1);
  psleep(CST816_POR_DELAY_TIME);
#endif
}

void touch_sensor_init(void) {
  s_i2c_lock = mutex_create();
  cst816_hw_reset();

  uint8_t chip_id;
  uint8_t fw_version;
  bool rv;
  rv = prv_read_data(CST816_CHIP_ID_REG, &chip_id, 1, 1);
  rv &= prv_read_data(CST816_FW_VERSION_REG, &fw_version, 1, 1);
  if (!rv) {
    PBL_ASSERT(rv, "cst816 read chip ID error");
  }
  PBL_LOG(LOG_LEVEL_DEBUG, "cst816 fw version:0x%x", fw_version);

  uint8_t target_ver = app_bin[sizeof(app_bin) + CST816_FW_VER_INFO_INDEX];

  if (target_ver != fw_version) {
    if (cst816_enter_bootmode()) {
      cst816_fw_update();
    } else {
      PBL_LOG(LOG_LEVEL_ERROR, "cst816 enter boot mode failed");
    }
  }

  // initialize exti
  exti_configure_pin(CST816->int_exti, ExtiTrigger_Falling, prv_exti_cb);
  exti_enable(CST816->int_exti);
}

static void prv_process_pending_messages(void* context) {
  s_callback_scheduled = false;

  uint8_t data[CST816_TOUCH_DATA_SIZE] = {0};
  bool rv = prv_read_data(CST816_TOUCH_DATA_REG, data, CST816_TOUCH_DATA_SIZE, 1);
  PBL_ASSERT(rv, "get touch data error");

  const uint64_t current_time_ms = ticks_to_milliseconds(rtc_get_ticks());
  uint8_t press = data[0] & 0x0F;
  GPoint point = {
    .x = (((uint16_t)(data[1] & 0x0F)) << 8) | data[2],
    .y = (((uint16_t)(data[3] & 0X0F)) << 8) | data[4],
  };

  if (press == 0x01) {
    touch_handle_update(0, TouchState_FingerDown, &point, 0, current_time_ms);
  } else {
    touch_handle_update(0, TouchState_FingerUp, NULL, 0, current_time_ms);
  }
}

static void prv_exti_cb(bool *should_context_switch) {
  if (s_callback_scheduled) {
    return;
  }

  system_task_add_callback_from_isr(prv_process_pending_messages, NULL, should_context_switch);
  s_callback_scheduled = true;
}
