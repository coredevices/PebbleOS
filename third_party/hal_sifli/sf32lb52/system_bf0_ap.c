/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "bf0_hal.h"
#include "register.h"

#define DCACHE_SIZE 16384
#define ICACHE_SIZE (DCACHE_SIZE << 1)

#if defined(__VTOR_PRESENT) && (__VTOR_PRESENT == 1U)
uint32_t __Vectors;
#endif

uint32_t SystemCoreClock = 48000000UL;

extern uint8_t __ramfunc_start[];
extern uint8_t __ramfunc_end[];

void SystemCoreClockUpdate(void) {}

enum {
  ATTR_CODE_IDX,
  ATTR_RAM_IDX,
  ATTR_DEVICE_IDX,
};

#define ATTR_CODE ARM_MPU_ATTR(ARM_MPU_ATTR_MEMORY_(0, 0, 1, 0), ARM_MPU_ATTR_MEMORY_(0, 0, 1, 0))
#define ATTR_RAM ARM_MPU_ATTR(ARM_MPU_ATTR_NON_CACHEABLE, ARM_MPU_ATTR_NON_CACHEABLE)
#define ATTR_DEVICE ARM_MPU_ATTR(ARM_MPU_ATTR_DEVICE, ARM_MPU_ATTR_DEVICE_nGnRnE)

// FIXME(SF32LB52): ARMv8 MPU support is not complete, so for now, configure
// the MPU here as needed by the system to run.
static void prv_mpu_config(void) {
  uint32_t rbar, rlar;

  ARM_MPU_Disable();

  for (uint8_t i = 0U; i < MPU_REGION_NUM; i++) {
    ARM_MPU_ClrRegion(i);
  }

  ARM_MPU_SetMemAttr(ATTR_CODE_IDX, ATTR_CODE);
  ARM_MPU_SetMemAttr(ATTR_RAM_IDX, ATTR_RAM);
  ARM_MPU_SetMemAttr(ATTR_DEVICE_IDX, ATTR_DEVICE);

  // PSRAM and FLASH2, region 1
  // Non-shareable, RO, any privilege, executable
  rbar = ARM_MPU_RBAR(0x10000000, ARM_MPU_SH_NON, 1, 1, 0);
  rlar = ARM_MPU_RLAR(0x1fffffff, ATTR_CODE_IDX);
  ARM_MPU_SetRegion(0U, rbar, rlar);

  // Peripheral space
  // Non-shareable, RW, any privilege, non-executable
  rbar = ARM_MPU_RBAR(0x40000000, ARM_MPU_SH_NON, 0, 1, 1);
  rlar = ARM_MPU_RLAR(0x5fffffff, ATTR_DEVICE_IDX);
  ARM_MPU_SetRegion(1U, rbar, rlar);

  // hpsys ram, .ramfunc range only: vendor HAL code copied here from flash
  // must be executable. The rest of HPSYS RAM is reachable for privileged
  // code via the background map (PRIVDEFENA is set in ARM_MPU_Enable below).
  // Non-shareable, RW, any privilege, executable
  rbar = ARM_MPU_RBAR((uint32_t)__ramfunc_start, ARM_MPU_SH_NON, 0, 1, 0);
  rlar = ARM_MPU_RLAR((uint32_t)__ramfunc_end - 1U, ATTR_RAM_IDX);
  ARM_MPU_SetRegion(2U, rbar, rlar);

  // lpsys ram
  // Non-shareable, RW, any privilege, executable
  rbar = ARM_MPU_RBAR(0x203fc000, ARM_MPU_SH_NON, 0, 1, 0);
  rlar = ARM_MPU_RLAR(0x204fffff, ATTR_RAM_IDX);
  ARM_MPU_SetRegion(3U, rbar, rlar);

  // HCPU<->LCPU mailbox (last 1K of HPSYS SRAM). The BT controller running
  // on LPSYS reads/writes this window directly, so it must be non-cacheable
  // on the HCPU side -- otherwise the two cores see incoherent contents.
  // Non-shareable, RW privileged only, non-executable.
  rbar = ARM_MPU_RBAR(0x2007fc00, ARM_MPU_SH_NON, 0, 0, 1);
  rlar = ARM_MPU_RLAR(0x2007ffff, ATTR_RAM_IDX);
  ARM_MPU_SetRegion(4U, rbar, rlar);

  ARM_MPU_Enable(MPU_CTRL_HFNMIENA_Msk | MPU_CTRL_PRIVDEFENA_Msk);
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

pm_power_on_mode_t SystemPowerOnModeGet(void) { return PM_COLD_BOOT; }

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

  prv_mpu_config();

  SCB_EnableICache();
  SCB_EnableDCache();
}
