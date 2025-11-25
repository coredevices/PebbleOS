/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdbool.h>
#include <stdint.h>

#include "board/board.h"
#include "drivers/flash.h"
#include "drivers/gpio.h"
#include "drivers/periph_config.h"
#include "util/delay.h"

#include "stm32f7haxx_qspi.h"

#define MT25Q_FASTREAD_DUMMYCYCLES 10

typedef enum MT25QCommand {
  // SPI/QSPI Commands
  MT25QCommand_FastRead = 0x0B,         // FAST_READ
  MT25QCommand_QSPIEnable = 0x35,       // QPI
  MT25QCommand_ResetEnable = 0x66,      // RSTEN
  MT25QCommand_Reset = 0x99,            // RST

  // QSPI only commands
  MT25QCommand_QSPI_ID = 0xAF,        // QPIID
} MT25QCommand;

// Helpful Enums
typedef enum {
  QSPIFlag_Retain = 0,
  QSPIFlag_ClearTC = 1,
} QSPIFlag;

static void prv_enable_qspi_clock(void) {
  periph_config_enable(QUADSPI, RCC_AHB3Periph_QSPI);
}

static void prv_disable_qspi_clock(void) {
  periph_config_disable(QUADSPI, RCC_AHB3Periph_QSPI);
}

static void prv_set_num_data_bytes(uint32_t length) {
  // From the docs: QSPI_DataLength: Number of data to be retrieved, value+1.
  // so 0 is 1 byte, so we substract 1 from the length. -1 is read the entire flash length.
  QSPI_SetDataLength(length - 1);
}

static void prv_wait_for_qspi_transfer_complete(QSPIFlag actions) {
  while (QSPI_GetFlagStatus(QSPI_FLAG_TC) == RESET) { }

  if (actions == QSPIFlag_ClearTC) {
    QSPI_ClearFlag(QSPI_FLAG_TC);
  }
}

static void prv_wait_for_qspi_not_busy(void) {
  while (QSPI_GetFlagStatus(QSPI_FLAG_BUSY) != RESET) { }
}

static void prv_quad_enable() {
  QSPI_ComConfig_InitTypeDef qspi_com_config;
  QSPI_ComConfig_StructInit(&qspi_com_config);
  qspi_com_config.QSPI_ComConfig_FMode = QSPI_ComConfig_FMode_Indirect_Write;
  qspi_com_config.QSPI_ComConfig_IMode = QSPI_ComConfig_IMode_1Line;
  qspi_com_config.QSPI_ComConfig_Ins = MT25QCommand_QSPIEnable;
  QSPI_ComConfig_Init(&qspi_com_config);

  prv_wait_for_qspi_transfer_complete(QSPIFlag_ClearTC);

  prv_wait_for_qspi_not_busy();
}

static void prv_flash_reset(void) {
  QSPI_ComConfig_InitTypeDef qspi_com_config;
  QSPI_ComConfig_StructInit(&qspi_com_config);
  qspi_com_config.QSPI_ComConfig_FMode = QSPI_ComConfig_FMode_Indirect_Write;
  qspi_com_config.QSPI_ComConfig_IMode = QSPI_ComConfig_IMode_4Line;
  qspi_com_config.QSPI_ComConfig_Ins = MT25QCommand_ResetEnable;
  QSPI_ComConfig_Init(&qspi_com_config);

  prv_wait_for_qspi_transfer_complete(QSPIFlag_ClearTC);

  QSPI_ComConfig_StructInit(&qspi_com_config);
  qspi_com_config.QSPI_ComConfig_FMode = QSPI_ComConfig_FMode_Indirect_Write;
  qspi_com_config.QSPI_ComConfig_IMode = QSPI_ComConfig_IMode_4Line;
  qspi_com_config.QSPI_ComConfig_Ins = MT25QCommand_Reset;
  QSPI_ComConfig_Init(&qspi_com_config);

  prv_wait_for_qspi_transfer_complete(QSPIFlag_ClearTC);

  delay_us(50000); // 50ms reset if busy with an erase!

  // Return the flash to Quad SPI mode, all our commands are quad-spi and it'll just cause
  // problems/bugs for someone if it comes back in single spi mode
  prv_quad_enable();
}

#include "system/passert.h"
static bool prv_flash_check_whoami(void) {
  const unsigned int num_whoami_bytes = 3;

  prv_set_num_data_bytes(num_whoami_bytes);

  QSPI_ComConfig_InitTypeDef qspi_com_config;
  QSPI_ComConfig_StructInit(&qspi_com_config);
  qspi_com_config.QSPI_ComConfig_FMode = QSPI_ComConfig_FMode_Indirect_Read;
  qspi_com_config.QSPI_ComConfig_DMode = QSPI_ComConfig_DMode_4Line;
  qspi_com_config.QSPI_ComConfig_IMode = QSPI_ComConfig_IMode_4Line;
  qspi_com_config.QSPI_ComConfig_Ins = MT25QCommand_QSPI_ID;
  QSPI_ComConfig_Init(&qspi_com_config);

  prv_wait_for_qspi_transfer_complete(QSPIFlag_ClearTC);

  uint32_t read_whoami = 0;
  for (unsigned int i = 0; i < num_whoami_bytes; ++i) {
    read_whoami |= QSPI_ReceiveData8() << (8 * i);
  }

  prv_wait_for_qspi_not_busy();

#if BOARD_ROBERT_BB || BOARD_ROBERT_BB2
  const uint32_t expected_whoami = 0x19BB20;
#elif BOARD_ROBERT_EVT
  const uint32_t expected_whoami = 0x18BB20;
#else
#error "Unsupported board"
#endif
  if (read_whoami == expected_whoami) {
    return true;
  } else {
    dbgserial_print_hex(read_whoami);
    return false;
  }
}

void flash_init(void) {
  prv_enable_qspi_clock();
  // init GPIOs
  for (unsigned i = 0; i < QSpiPinCount; ++i) {
    gpio_af_init(&BOARD_CONFIG_FLASH_PINS[i], GPIO_OType_PP, GPIO_Speed_100MHz, GPIO_PuPd_NOPULL);
  }
  if (BOARD_CONFIG_FLASH.reset_gpio.gpio) {
    gpio_output_init(&BOARD_CONFIG_FLASH.reset_gpio, GPIO_OType_PP, GPIO_Speed_2MHz);
    gpio_output_set(&BOARD_CONFIG_FLASH.reset_gpio, false);
  }

  // Init QSPI peripheral
  QSPI_InitTypeDef qspi_config;
  QSPI_StructInit(&qspi_config);
  qspi_config.QSPI_SShift = QSPI_SShift_HalfCycleShift;
  qspi_config.QSPI_Prescaler = 0;
  qspi_config.QSPI_CKMode = QSPI_CKMode_Mode0;
  qspi_config.QSPI_CSHTime = QSPI_CSHTime_1Cycle;
  qspi_config.QSPI_FSize = 23; // 2^24 = 16MB. -> 24 - 1 = 23
  qspi_config.QSPI_FSelect = QSPI_FSelect_1;
  qspi_config.QSPI_DFlash = QSPI_DFlash_Disable;
  QSPI_Init(&qspi_config);

  QSPI_Cmd(ENABLE);

  // Must call quad_enable first, all commands are QSPI
  prv_quad_enable();

  // Reset the flash to stop any program's or erase in progress from before reboot
  prv_flash_reset();

  prv_disable_qspi_clock();
}

bool flash_sanity_check(void) {
  prv_enable_qspi_clock();

  bool result = prv_flash_check_whoami();

  prv_disable_qspi_clock();

  return result;
}

void flash_read_bytes(uint8_t *buffer_ptr, uint32_t start_addr, uint32_t buffer_size) {
  prv_enable_qspi_clock();

  prv_set_num_data_bytes(buffer_size);

  QSPI_ComConfig_InitTypeDef qspi_com_config;
  QSPI_ComConfig_StructInit(&qspi_com_config);
  qspi_com_config.QSPI_ComConfig_FMode = QSPI_ComConfig_FMode_Indirect_Read;
  qspi_com_config.QSPI_ComConfig_DMode = QSPI_ComConfig_DMode_4Line;
  qspi_com_config.QSPI_ComConfig_DummyCycles = MT25Q_FASTREAD_DUMMYCYCLES;
  qspi_com_config.QSPI_ComConfig_ADMode = QSPI_ComConfig_ADMode_4Line;
  qspi_com_config.QSPI_ComConfig_IMode = QSPI_ComConfig_IMode_4Line;
  qspi_com_config.QSPI_ComConfig_ADSize = QSPI_ComConfig_ADSize_24bit;
  qspi_com_config.QSPI_ComConfig_Ins = MT25QCommand_FastRead;
  QSPI_ComConfig_Init(&qspi_com_config);

  QSPI_SetAddress(start_addr);

  uint8_t *write_ptr = buffer_ptr;
  for (unsigned i = 0; i < buffer_size; ++i) {
    write_ptr[i] = QSPI_ReceiveData8();
  }

  QSPI_ClearFlag(QSPI_FLAG_TC);
  prv_wait_for_qspi_not_busy();

  prv_disable_qspi_clock();
}
