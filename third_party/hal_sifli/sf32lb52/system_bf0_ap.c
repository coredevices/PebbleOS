/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "bf0_hal.h"
#include "register.h"

#include <pbl/kernel/delay.h>
#include <pbl/kernel/section.h>
#include <pbl/kernel/time.h>

#define DCACHE_SIZE 16384
#define ICACHE_SIZE (DCACHE_SIZE << 1)

#if defined(__VTOR_PRESENT) && (__VTOR_PRESENT == 1U)
uint32_t __Vectors;
#endif

uint32_t SystemCoreClock = 48000000UL;

void SystemCoreClockUpdate(void) {
}

int mpu_dcache_invalidate(void *data, uint32_t size) {
  int r = 0;

  if (IS_DCACHED_RAM(data)) {
    if (size > DCACHE_SIZE) {
      SCB_InvalidateDCache();
      r = 1;
    } else
      SCB_InvalidateDCache_by_Addr(data, size);
  }

  return r;
}

int mpu_icache_invalidate(void *data, uint32_t size) {
  int r = 0;

  if (IS_DCACHED_RAM(data)) {
    if (size > ICACHE_SIZE) {
      SCB_InvalidateICache();
      r = 1;
    } else
      SCB_InvalidateICache_by_Addr(data, size);
  }

  return r;
}

pm_power_on_mode_t SystemPowerOnModeGet(void) {
  return PM_COLD_BOOT;
}

void SystemInit(void) {
#if defined(__VTOR_PRESENT) && (__VTOR_PRESENT == 1U)
  SCB->VTOR = (uint32_t)&__Vectors;
#endif

  // enable CP0/CP1/CP2 Full Access
  SCB->CPACR |= (3U << (0U * 2U)) | (3U << (1U * 2U)) | (3U << (2U * 2U));

#if defined(__FPU_USED) && (__FPU_USED == 1U)
  SCB->CPACR |= ((3U << 10U * 2U) | // enable CP10 Full Access
                 (3U << 11U * 2U)); // enable CP11 Full Access
#endif

  SCB_EnableICache();
  SCB_EnableDCache();
}

// The overrides of the HAL's weak functions live here because an archive member holding only them
// would not be linked: the weak versions satisfy the references.

// HAL timeouts count kernel uptime.
uint32_t HAL_GetTick(void) {
  return (uint32_t)pbl_uptime_ms();
}

// HAL delays, some with the flash down
PBL_SECTION_RAM void HAL_Delay_us(uint32_t us) {
  pbl_busy_wait_us(us);
}

PBL_SECTION_RAM void HAL_Delay_us_(__IO uint32_t us) {
  pbl_busy_wait_us(us);
}

PBL_SECTION_RAM void HAL_Delay(__IO uint32_t ms) {
  pbl_busy_wait_us(ms * 1000U);
}
