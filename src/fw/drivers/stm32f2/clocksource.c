/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdbool.h>
#include <string.h>

#include "drivers/clocksource.h"

#include "board/board.h"
#include "drivers/gpio.h"
#include "drivers/periph_config.h"
#include "drivers/watchdog.h"
#include "system/logging.h"
#include "system/passert.h"
#include "kernel/util/delay.h"

#include "FreeRTOS.h"

#define STM32F2_COMPATIBLE
#define STM32F4_COMPATIBLE
#define STM32F7_COMPATIBLE
#include <mcu.h>

//! How long to wait for the LSE to start. Empirically about 4 seconds.
//! STM32 datasheet says typical max is 2.
static const int LSE_READY_TIMEOUT_MS = 5000;

void clocksource_lse_configure(void) {
  // Only start the LSE oscillator if it is not already running. The oscillator
  // will normally be running even during standby mode to keep the RTC ticking;
  // it is only disabled when the microcontroller completely loses power.
  if (clocksource_is_lse_started()) {
    PBL_LOG(LOG_LEVEL_INFO, "LSE oscillator already running");
  } else {
    PBL_LOG(LOG_LEVEL_INFO, "Starting LSE oscillator");
    RCC_LSEConfig(BOARD_LSE_MODE);
    for (int i = 0; i < LSE_READY_TIMEOUT_MS; ++i) {
      if (RCC_GetFlagStatus(RCC_FLAG_LSERDY) != RESET) {
        PBL_LOG(LOG_LEVEL_INFO, "LSE oscillator started after %d ms", i);
        break;
      }

      delay_us(1000);
      watchdog_feed();
    }
    if (RCC_GetFlagStatus(RCC_FLAG_LSERDY) == RESET) {
      PBL_LOG(LOG_LEVEL_ERROR, "LSE oscillator did not start");
    }
  }
}

bool clocksource_is_lse_started(void) {
  return (RCC_GetFlagStatus(RCC_FLAG_LSERDY) != RESET);
}

void clocksource_MCO1_enable(bool on) {
  static int8_t s_refcount = 0;

  portENTER_CRITICAL();

  PBL_ASSERTN(BOARD_CONFIG_MCO1.output_enabled);
  if (on) {
    gpio_af_init(
        &BOARD_CONFIG_MCO1.af_cfg, GPIO_OType_PP, GPIO_Speed_2MHz, GPIO_PuPd_NOPULL);
    // LSE is 32kHz, we want 32kHz for our external clock and is used by:
    //  - The cc2564 bluetooth module
    //  - Snowy / Spalding display VCOM
    RCC_MCO1Config(RCC_MCO1Source_LSE, RCC_MCO1Div_1);
    ++s_refcount;
  } else {
    PBL_ASSERTN(s_refcount > 0);
    --s_refcount;
    if (s_refcount == 0) {
      PBL_LOG(LOG_LEVEL_DEBUG, "Disabling MCO1");
      gpio_analog_init(&BOARD_CONFIG_MCO1.an_cfg);
    }
  }

  portEXIT_CRITICAL();
}
