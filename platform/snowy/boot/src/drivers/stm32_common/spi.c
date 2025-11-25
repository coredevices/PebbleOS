/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/spi.h"

#include "util/misc.h"
#include "system/passert.h"

#if defined(MICRO_FAMILY_STM32F2)
#include "stm32f2xx_rcc.h"
#elif defined(MICRO_FAMILY_STM32F4)
#include "stm32f4xx_rcc.h"
#endif

// Deduced by looking at the prescalers in stm32f2xx_spi.h
#define SPI_FREQ_LOG_TO_PRESCALER(LG) (((LG) - 1) * 0x8)

uint16_t spi_find_prescaler(uint32_t bus_frequency, SpiPeriphClock periph_clock) {
  // Get the clocks
  RCC_ClocksTypeDef clocks;
  RCC_GetClocksFreq(&clocks);

  uint32_t clock = 0;
  // Find which peripheral clock we belong to
  if (periph_clock == SpiPeriphClockAPB1) {
    clock = clocks.PCLK1_Frequency;
  } else if (periph_clock == SpiPeriphClockAPB2) {
    clock = clocks.PCLK2_Frequency;
  } else {
    WTF;
  }

  int lg;
  if (bus_frequency > (clock / 2)) {
    lg = 1; // Underclock to the highest possible frequency
  } else {
    uint32_t divisor = clock / bus_frequency;
    lg = ceil_log_two(divisor);
  }

  // Prescalers only exists for values in [2 - 256] range
  PBL_ASSERTN(lg > 0);
  PBL_ASSERTN(lg < 9);

  // return prescaler
  return (SPI_FREQ_LOG_TO_PRESCALER(lg));
}
