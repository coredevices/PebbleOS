/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

/* CMSIS device header for Nordic nRF52 targets. */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Cortex-M4 core configuration */
#define __CM4_REV              0x0001U
#define __NVIC_PRIO_BITS       3U
#define __Vendor_SysTickConfig 0U
#define __MPU_PRESENT          1U
#define __VTOR_PRESENT         1U
#define __FPU_PRESENT          1U
#define __DSP_PRESENT          1U

/* Device interrupt lines are in soc_irqs.h */
typedef enum IRQn {
  NonMaskableInt_IRQn = -14,
  HardFault_IRQn = -13,
  MemoryManagement_IRQn = -12,
  BusFault_IRQn = -11,
  UsageFault_IRQn = -10,
  SVCall_IRQn = -5,
  DebugMonitor_IRQn = -4,
  PendSV_IRQn = -2,
  SysTick_IRQn = -1,
} IRQn_Type;

#include "core_cm4.h"

extern uint32_t SystemCoreClock;

#ifdef __cplusplus
}
#endif
