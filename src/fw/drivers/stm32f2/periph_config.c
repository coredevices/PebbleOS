/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/periph_config.h"
#include "os/mutex.h"
#include "system/logging.h"
#include "system/passert.h"

#define STM32F2_COMPATIBLE
#define STM32F4_COMPATIBLE
#define STM32F7_COMPATIBLE
#include <mcu.h>

#include "FreeRTOS.h"

#define PERIPH_CONFIG_DEBUG 0

#if PERIPH_CONFIG_DEBUG
#define PERIPH_CONFIG_LOG(fmt, args...) \
        PBL_LOG(LOG_LEVEL_DEBUG, fmt, ## args)
#else
#define PERIPH_CONFIG_LOG(fmt, args...)
#endif

typedef void (*ClockCmd)(uint32_t periph, FunctionalState state);

static PebbleMutex * s_periph_config_mutex;

#if PERIPH_CONFIG_DEBUG
static const char *prv_string_for_cmd(ClockCmd cmd) {
  if (cmd == RCC_APB1PeriphClockCmd) {
    return "APB1";
  } else if (cmd == RCC_APB2PeriphClockCmd) {
    return "APB2";
  } else if (cmd == RCC_AHB1PeriphClockCmd) {
    return "AHB1";
  } else if (cmd == RCC_AHB2PeriphClockCmd) {
    return "AHB2";
  } else {
    return NULL;
  }
}
#endif

// F(S)MC is the only AHB3 peripheral
#ifdef FMC_R_BASE
#define AHB3_BASE FMC_R_BASE
#else
#define AHB3_BASE FSMC_R_BASE
#endif

_Static_assert(APB1PERIPH_BASE < APB2PERIPH_BASE, "Clock mapping assumptions don't hold");
_Static_assert(APB2PERIPH_BASE < AHB1PERIPH_BASE, "Clock mapping assumptions don't hold");
_Static_assert(AHB1PERIPH_BASE < AHB2PERIPH_BASE, "Clock mapping assumptions don't hold");
_Static_assert(AHB2PERIPH_BASE < AHB3_BASE, "Clock mapping assumptions don't hold");

// Note: this works only with peripheral (<...>Typedef_t *) defines, not with RCC defines
static ClockCmd prv_get_clock_cmd(uintptr_t periph_addr) {
  PBL_ASSERTN(periph_addr >= APB1PERIPH_BASE);
  if (periph_addr < APB2PERIPH_BASE) {
    return RCC_APB1PeriphClockCmd;
  } else if (periph_addr < AHB1PERIPH_BASE) {
    return RCC_APB2PeriphClockCmd;
  } else if (periph_addr < AHB2PERIPH_BASE) {
    return RCC_AHB1PeriphClockCmd;
  } else if (periph_addr < AHB3_BASE) {
    return RCC_AHB2PeriphClockCmd;
  } else {
    return RCC_AHB3PeriphClockCmd;
  }
}

void periph_config_init(void) {
  s_periph_config_mutex = mutex_create();
}

void periph_config_acquire_lock(void) {
  mutex_lock(s_periph_config_mutex);
}

void periph_config_release_lock(void) {
  mutex_unlock(s_periph_config_mutex);
}

void periph_config_enable(void *periph, uint32_t rcc_bit) {
  ClockCmd clock_cmd = prv_get_clock_cmd((uintptr_t)periph);
#if PERIPH_CONFIG_DEBUG
  if (prv_string_for_cmd(clock_cmd))
    PERIPH_CONFIG_LOG("Enabling clock %s", prv_string_for_cmd(clock_cmd));
#endif
  portENTER_CRITICAL();
  clock_cmd(rcc_bit, ENABLE);
  portEXIT_CRITICAL();
}

void periph_config_disable(void *periph, uint32_t rcc_bit) {
  ClockCmd clock_cmd = prv_get_clock_cmd((uintptr_t)periph);
#if PERIPH_CONFIG_DEBUG
  if (prv_string_for_cmd(clock_cmd))
    PERIPH_CONFIG_LOG("Disabling clock %s", prv_string_for_cmd(clock_cmd));
#endif
  portENTER_CRITICAL();
  clock_cmd(rcc_bit, DISABLE);
  portEXIT_CRITICAL();
}
