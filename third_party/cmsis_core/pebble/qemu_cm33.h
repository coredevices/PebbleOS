/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

/* CMSIS device header for QEMU Pebble Cortex-M33 targets
 * (qemu_emery, qemu_gabbro). */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Cortex-M33 core configuration */
#define __CM33_REV                0x0000U
#define __NVIC_PRIO_BITS          3U
#define __Vendor_SysTickConfig    0U
#define __MPU_PRESENT             1U
#define __VTOR_PRESENT            1U
#define __FPU_PRESENT             1U
#define __DSP_PRESENT             0U
#define __SAUREGION_PRESENT       0U

/* QEMU Pebble IRQ numbers */
typedef enum IRQn {
  /* Cortex-M processor exceptions */
  NonMaskableInt_IRQn   = -14,
  HardFault_IRQn        = -13,
  MemoryManagement_IRQn = -12,
  BusFault_IRQn         = -11,
  UsageFault_IRQn       = -10,
  SecureFault_IRQn      =  -9,
  SVCall_IRQn           =  -5,
  DebugMonitor_IRQn     =  -4,
  PendSV_IRQn           =  -2,
  SysTick_IRQn          =  -1,

  /* QEMU device interrupts */
  UART0_IRQn            =   0,
  UART1_IRQn            =   1,
  UART2_IRQn            =   2,
  TIMER0_IRQn           =   3,
  TIMER1_IRQn           =   4,
  RTC_IRQn              =   5,
  GPIO_IRQn             =   6,
  DISPLAY_IRQn          =   7,
  EXTFLASH_IRQn         =   8,
  TOUCH_IRQn            =   9,
  AUDIO_IRQn            =  10,
  WATCHDOG_IRQn         =  11,
} IRQn_Type;

#include "core_cm33.h"

/* Compatibility: ARMv8-M MPU uses RBAR/RLAR instead of RBAR/RASR.
 * The codebase uses ARMv7-M MPU register names via vendor HAL compat.
 * Provide RASR as an alias for RLAR so mpu.c compiles. MPU configuration
 * is skipped for CM33 QEMU targets (see memory_layout.c). */
#ifndef MPU_RASR_TEX_Pos
#define MPU_RASR_TEX_Pos  MPU_RLAR_AttrIndx_Pos
#define MPU_RASR_S_Msk    0
#define MPU_RASR_C_Msk    0
#define MPU_RASR_B_Msk    0
#define RASR RLAR
#endif

/* ARM_CM33 FreeRTOS port expects SCB_CCR_STKALIGN_Msk (always set on M33) */
#ifndef SCB_CCR_STKALIGN_Msk
#define SCB_CCR_STKALIGN_Msk (1UL << 9)
#endif

/* Generic SRAM base address */
#ifndef SRAM_BASE
#define SRAM_BASE  0x20000000UL
#endif

extern uint32_t SystemCoreClock;

#ifdef __cplusplus
}
#endif
