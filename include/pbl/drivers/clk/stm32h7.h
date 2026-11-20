/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/clk.h>

/**
 * @defgroup drivers_clk_stm32h7 STM32H7
 * @ingroup drivers_clk
 * @brief @ref drivers_clk controller of the STM32H7 RCC.
 *
 * The oscillators, the system clock and the bus clocks are clocks of the controller, parents of
 * the peripheral clocks they run. The peripheral clocks are gated, and those with a kernel clock
 * multiplexer take its current input as their parent. The clock tree is the one the RCC leaves
 * after reset, on HSI: the PLLs are not supported yet.
 *
 * Before any clock, the controller sets the supply configuration of the core domain in PWR,
 * which must match how the board is wired.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_clk_controller pbl_clk_stm32h7;
/** @endcond */

/** @brief Clock controller. */
#define STM32H7_RCC (&pbl_clk_stm32h7)

/**
 * @name Clocks
 *
 * The clocks the driver supports, numbered as in the binding, dt-bindings/clock/stm32h7-clks.h.
 * @{
 */
/** HSE oscillator. */
#define STM32H7_CLK_HSE_CK 10
/** LSE oscillator. */
#define STM32H7_CLK_LSE_CK 11
/** HSI oscillator, 64 MHz divided by HSIDIV. */
#define STM32H7_CLK_HSI_CK 18
/** CSI oscillator, 4 MHz. */
#define STM32H7_CLK_CSI_CK 20
/** System clock, sys_ck. */
#define STM32H7_CLK_SYS_CK 30
/** Domain 1 core clock, sys_d1cpre_ck: the CPU. */
#define STM32H7_CLK_SYS_D1CPRE 0
/** AHB clock, rcc_hclk. */
#define STM32H7_CLK_HCLK 1
/** APB1 clock, rcc_pclk1. */
#define STM32H7_CLK_PCLK1 2
/** APB2 clock, rcc_pclk2. */
#define STM32H7_CLK_PCLK2 3
/** GPIO port A to K clocks. */
#define STM32H7_CLK_GPIOA_CK 86
#define STM32H7_CLK_GPIOB_CK 85
#define STM32H7_CLK_GPIOC_CK 84
#define STM32H7_CLK_GPIOD_CK 83
#define STM32H7_CLK_GPIOE_CK 82
#define STM32H7_CLK_GPIOF_CK 81
#define STM32H7_CLK_GPIOG_CK 80
#define STM32H7_CLK_GPIOH_CK 79
#define STM32H7_CLK_GPIOI_CK 78
#define STM32H7_CLK_GPIOJ_CK 77
#define STM32H7_CLK_GPIOK_CK 76
/** TIM5 kernel clock. */
#define STM32H7_CLK_TIM5_CK 95
/** USART1 kernel clock. */
#define STM32H7_CLK_USART1_CK 156
/** @} */

/** @cond INTERNAL_HIDDEN */
#define PBL_CLK_STM32H7_NUM_CLKS 166
/** @endcond */

/**
 * @name Supply configurations
 *
 * The PWR CR3 supply fields of each configuration of the st,stm32h747-pwr binding.
 * @{
 */
#define PBL_CLK_STM32H7_SUPPLY_LDO                 0x02U
#define PBL_CLK_STM32H7_SUPPLY_DIRECT_SMPS         0x04U
#define PBL_CLK_STM32H7_SUPPLY_SMPS_1V8_LDO        0x16U
#define PBL_CLK_STM32H7_SUPPLY_SMPS_2V5_LDO        0x26U
#define PBL_CLK_STM32H7_SUPPLY_SMPS_1V8_EXT_LDO    0x1eU
#define PBL_CLK_STM32H7_SUPPLY_SMPS_2V5_EXT_LDO    0x2eU
#define PBL_CLK_STM32H7_SUPPLY_SMPS_1V8_EXT_BYPASS 0x1dU
#define PBL_CLK_STM32H7_SUPPLY_SMPS_2V5_EXT_BYPASS 0x2dU
#define PBL_CLK_STM32H7_SUPPLY_BYPASS              0x01U
/** @} */

/** @brief STM32H7 RCC. */
struct pbl_clk_stm32h7 {
  /** Clock controller. */
  struct pbl_clk_controller ctrl;
  /** Registers. */
  uintptr_t base;
  /** PWR registers. */
  uintptr_t pwr;
  /** Supply configuration, a @c PBL_CLK_STM32H7_SUPPLY_ value. */
  uint32_t supply;
  /** HSE input. */
  struct pbl_clk hse;
  /** LSE input. */
  struct pbl_clk lse;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_clk_controller_ops pbl_clk_stm32h7_ops;
int pbl_clk_stm32h7_init(const struct pbl_device *dev);
/** @endcond */

/**
 * @brief Define the RCC.
 *
 * @param sym Symbol of the controller.
 * @param _name Name.
 * @param _base Registers.
 * @param _pwr PWR registers.
 * @param _supply Supply configuration, a @c PBL_CLK_STM32H7_SUPPLY_ value.
 * @param _hse HSE input, from PBL_CLK().
 * @param _lse LSE input, from PBL_CLK().
 */
#define PBL_CLK_STM32H7_DEFINE(sym, _name, _base, _pwr, _supply, _hse, _lse)                      \
  PBL_CLK_CONTROLLER_STATE_DEFINE(sym, PBL_CLK_STM32H7_NUM_CLKS);                                 \
  PBL_DEVICE_INSTANCE(sym, struct pbl_clk_stm32h7, struct pbl_clk_controller, ctrl) {             \
    .ctrl =                                                                                       \
        PBL_CLK_CONTROLLER_INIT(sym, _name, pbl_clk_stm32h7_init, nullptr, &pbl_clk_stm32h7_ops), \
    .base = (_base), .pwr = (_pwr), .supply = (_supply), .hse = _hse, .lse = _lse,                \
  }

/** @} */
