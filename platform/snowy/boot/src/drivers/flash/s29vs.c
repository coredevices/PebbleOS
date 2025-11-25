/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdbool.h>
#include <stdint.h>

#include "drivers/flash.h"
#include "drivers/flash/s29vs.h"
#include "drivers/gpio.h"
#include "drivers/periph_config.h"
#include "stm32f4xx_gpio.h"
#include "util/delay.h"


//! @param sector_address The address of the start of the sector to write the command to.
//! @param cmd The command to write.
static void flash_s29vs_issue_command(FlashAddress sector_address, S29VSCommand cmd) {
  // The offset in the sector we write the first part of commands to. Note that this is a 16-bit
  // word aligned address as opposed to a byte address.
  static const uint32_t COMMAND_ADDRESS = 0x555;

  ((__IO uint16_t*) (FMC_BANK_1_BASE_ADDRESS + sector_address))[COMMAND_ADDRESS] = cmd;
}

static uint16_t flash_s29vs_read_short(FlashAddress addr) {
  return *((__IO uint16_t*)(FMC_BANK_1_BASE_ADDRESS + addr));
}

void flash_read_bytes(uint8_t* buffer, uint32_t start_addr, uint32_t buffer_size) {
  memcpy(buffer, (void*)(FMC_BANK_1_BASE_ADDRESS + start_addr), buffer_size);
}


static void flash_s29vs_software_reset(void) {
  flash_s29vs_issue_command(0, S29VSCommand_SoftwareReset);
}

void flash_init(void) {
  gpio_use(GPIOB);
  gpio_use(GPIOD);
  gpio_use(GPIOE);

  // Configure the reset pin (D2)
  GPIO_InitTypeDef gpio_init = {
    .GPIO_Pin = GPIO_Pin_2,
    .GPIO_Mode = GPIO_Mode_OUT,
    .GPIO_Speed = GPIO_Speed_100MHz,
    .GPIO_OType = GPIO_OType_PP,
    .GPIO_PuPd  = GPIO_PuPd_NOPULL
  };
  GPIO_Init(GPIOD, &gpio_init);

  GPIO_WriteBit(GPIOD, GPIO_Pin_2, Bit_SET);

  // Configure pins relating to the FMC peripheral (30 pins!)

  // B7 - FMC AVD - FMC Address Valid aka Latch
  // D0-D1, D8-D15, E2-15 - FMC A, AD - FMC Address and Address/Data lines
  // D2 - Reset - GPIO Reset line
  // D3 - FMC CLK
  // D4 - FMC OE - FMC Output Enable
  // D5 - FMC WE - FMC Write Enable
  // D6 - FMC RDY - FMC Ready line
  // D7 - FMC CE - FMC Chip Enable

  gpio_init = (GPIO_InitTypeDef) {
    .GPIO_Mode = GPIO_Mode_AF,
    .GPIO_Speed = GPIO_Speed_100MHz,
    .GPIO_OType = GPIO_OType_PP,
    .GPIO_PuPd  = GPIO_PuPd_NOPULL
  };

  GPIO_PinAFConfig(GPIOB, GPIO_PinSource7, GPIO_AF_FMC);
  gpio_init.GPIO_Pin = GPIO_Pin_7;
  GPIO_Init(GPIOB, &gpio_init);

  for (uint8_t pin_source = 0; pin_source < 16; ++pin_source) {
    if (pin_source == 2) {
      continue;
    }
    GPIO_PinAFConfig(GPIOD, pin_source, GPIO_AF_FMC);
  }
  gpio_init.GPIO_Pin = GPIO_Pin_All & (~GPIO_Pin_2);
  GPIO_Init(GPIOD, &gpio_init);

  for (uint8_t pin_source = 2; pin_source < 16; ++pin_source) {
    GPIO_PinAFConfig(GPIOE, pin_source, GPIO_AF_FMC);
  }
  gpio_init.GPIO_Pin = GPIO_Pin_All & (~GPIO_Pin_0) & (~GPIO_Pin_1);
  GPIO_Init(GPIOE, &gpio_init);

  // We have configured the pins, lets perform a full HW reset to put the chip
  // in a good state
  GPIO_WriteBit(GPIOD, GPIO_Pin_2, Bit_RESET);
  delay_us(10); // only needs to be 50ns according to data sheet
  GPIO_WriteBit(GPIOD, GPIO_Pin_2, Bit_SET);
  delay_us(30); // need 200ns + 10us before CE can be pulled low

  RCC_AHB3PeriphClockCmd(RCC_AHB3Periph_FMC, ENABLE);

  // Setup default config for async
  // Configure the FMC peripheral itself
  FMC_NORSRAMTimingInitTypeDef nor_timing_init = {
    // time between address write and address latch (AVD high)
    // tAAVDS on datasheet, min 4 ns
    //
    // AVD low time
    // tAVDP on datasheet, min 6 ns
    .FMC_AddressSetupTime = 1,

    // time between AVD high (address is available) and OE low (memory can write)
    // tAVDO on the datasheet, min 4 ns
    .FMC_AddressHoldTime = 1,

    // time between OE low (memory can write) and valid data being available
    // tOE on datasheet, max 15 ns
    // 13 cycles is the default configuration in the component's configuration register
    // Setup to 3 for async
    .FMC_DataSetupTime = 3,

    // Time between chip selects
    // not on the datasheet, picked a random safe number
    .FMC_BusTurnAroundDuration = 1,

    .FMC_CLKDivision = 15, // Not used for async NOR
    .FMC_DataLatency = 15, // Not used for async NOR
    .FMC_AccessMode = FMC_AccessMode_A // Only used for ExtendedMode == FMC_ExtendedMode_Enable, which we don't use
  };

  FMC_NORSRAMInitTypeDef nor_init = {
    .FMC_Bank = FMC_Bank1_NORSRAM1,
    .FMC_DataAddressMux = FMC_DataAddressMux_Enable,
    .FMC_MemoryType = FMC_MemoryType_NOR,
    .FMC_MemoryDataWidth = FMC_NORSRAM_MemoryDataWidth_16b,
    .FMC_BurstAccessMode = FMC_BurstAccessMode_Disable,
    .FMC_AsynchronousWait = FMC_AsynchronousWait_Disable,
    .FMC_WaitSignalPolarity = FMC_WaitSignalPolarity_Low,
    .FMC_WrapMode = FMC_WrapMode_Disable,
    .FMC_WaitSignalActive = FMC_WaitSignalActive_BeforeWaitState,
    .FMC_WriteOperation = FMC_WriteOperation_Enable,
    .FMC_WaitSignal = FMC_WaitSignal_Enable,
    .FMC_ExtendedMode = FMC_ExtendedMode_Disable,
    .FMC_WriteBurst = FMC_WriteBurst_Disable,
    .FMC_ContinousClock = FMC_CClock_SyncOnly,
    .FMC_ReadWriteTimingStruct = &nor_timing_init
  };

  FMC_NORSRAMInit(&nor_init);

  // Re-enable NOR
  FMC_NORSRAMCmd(FMC_Bank1_NORSRAM1, ENABLE);
}

bool flash_sanity_check(void) {
  // Check that the first words of the CFI table are 'Q' 'R' 'Y'.
  // This will work on any flash memory, regardless of the manufacturer.
  flash_s29vs_issue_command(0, S29VSCommand_CFIEntry);
  bool ok = (flash_s29vs_read_short(0x20) & 0xff) == 'Q';
  ok = ok && (flash_s29vs_read_short(0x22) & 0xff) == 'R';
  ok = ok && (flash_s29vs_read_short(0x24) & 0xff) == 'Y';
  flash_s29vs_software_reset();
  return ok;
}
