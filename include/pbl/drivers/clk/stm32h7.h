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
 * multiplexer take its current input as their parent.
 *
 * Before any clock, the controller sets the supply configuration of the core domain in PWR,
 * which must match how the board is wired, and the highest core voltage (VOS1), then applies its
 * assigned clocks: the PLL source, the rate of PLL1 P, which configures PLL1 and starts it, the
 * parent of the system clock and the bus rates. The system clock moves to a faster parent with
 * the bus clocks divided down to their maximums, 240 MHz for HCLK and 120 MHz for the APB clocks;
 * the CPU runs up to 400 MHz. The flash keeps its reset wait states, which any clock takes.
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
/** HSI48 oscillator, 48 MHz. */
#define STM32H7_CLK_RC48_CK 22
/** LSI oscillator, 32 kHz. */
#define STM32H7_CLK_LSI_CK 23
/** PLL source, pllsrc: HSI, CSI or HSE. */
#define STM32H7_CLK_PLLSRC 29
/** PLL1 outputs. */
#define STM32H7_CLK_PLL1_P 32
#define STM32H7_CLK_PLL1_Q 33
#define STM32H7_CLK_PLL1_R 34
/** PLL2 outputs. */
#define STM32H7_CLK_PLL2_P 35
#define STM32H7_CLK_PLL2_Q 36
#define STM32H7_CLK_PLL2_R 37
/** PLL3 outputs. */
#define STM32H7_CLK_PLL3_P 38
#define STM32H7_CLK_PLL3_Q 39
#define STM32H7_CLK_PLL3_R 40
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
/** APB3 clock. */
#define STM32H7_CLK_PCLK3 4
/** APB4 clock, rcc_pclk4. */
#define STM32H7_CLK_PCLK4 5
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
/** CRC unit clock. */
#define STM32H7_CLK_CRC_CK 75
/** DMA1 clock. */
#define STM32H7_CLK_DMA1_CK 65
/** DMA2 clock. */
#define STM32H7_CLK_DMA2_CK 64
/** ADC3 bus clock, which also clocks its conversions in synchronous mode. */
#define STM32H7_CLK_ADC3_CK 128
/** Hardware semaphore clock. */
#define STM32H7_CLK_HSEM_CK 73
/** WWDG2 clock, the window watchdog of the Cortex-M4. */
#define STM32H7_CLK_WWDG2_CK 89
/** LPTIM1 kernel clock. */
#define STM32H7_CLK_LPTIM1_CK 144
/** LPTIM2 kernel clock. */
#define STM32H7_CLK_LPTIM2_CK 162
/** SYSCFG clock. */
#define STM32H7_CLK_SYSCFG_CK 111
/** RNG kernel clock. */
#define STM32H7_CLK_RNG_CK 127
/** QUADSPI kernel clock. */
#define STM32H7_CLK_QUADSPI_CK 121
/** USART1 kernel clock. */
#define STM32H7_CLK_USART1_CK 156
/** RTC kernel clock, from the LSE, the LSI or the HSE. */
#define STM32H7_CLK_RTC_CK 13
/** RTC bus clock. */
#define STM32H7_CLK_RTCAPB_CK 108
/** UART8 kernel clock. */
#define STM32H7_CLK_UART8_CK 131
/** USB OTG HS clock, its 48 MHz kernel clock selected among PLL1 Q, PLL3 Q and HSI48. */
#define STM32H7_CLK_USB1OTG_CK 124
/** ULPI clock of the USB OTG HS, for an external PHY. */
#define STM32H7_CLK_USB1ULPI_CK 59
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
  /** Assigned clocks, ending with an empty entry. */
  const struct pbl_clk_assigned *assigned;
};

/**
 * @brief Read the reset flags of the core running the code.
 *
 * The RCC keeps the causes of the resets since the flags were last cleared.
 *
 * @param ctrl The RCC.
 * @param clear Clear the flags after reading them.
 * @return The RSR register, which shows each core its own, @c RCC_RSR_ bits of
 * hardware/stm32h7_rcc.h.
 */
uint32_t pbl_clk_stm32h7_reset_flags(const struct pbl_clk_controller *ctrl, bool clear);

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
 * @param ... Assigned clocks, from PBL_CLK_ASSIGNED().
 */
#define PBL_CLK_STM32H7_DEFINE(sym, _name, _base, _pwr, _supply, _hse, _lse, ...)                 \
  PBL_CLK_CONTROLLER_STATE_DEFINE(sym, PBL_CLK_STM32H7_NUM_CLKS);                                 \
  PBL_CLK_ASSIGNED_DEFINE(sym##_assigned, __VA_ARGS__);                                           \
  PBL_DEVICE_INSTANCE(sym, struct pbl_clk_stm32h7, struct pbl_clk_controller, ctrl) {             \
    .ctrl =                                                                                       \
        PBL_CLK_CONTROLLER_INIT(sym, _name, pbl_clk_stm32h7_init, nullptr, &pbl_clk_stm32h7_ops), \
    .base = (_base), .pwr = (_pwr), .supply = (_supply), .hse = _hse, .lse = _lse,                \
    .assigned = sym##_assigned,                                                                   \
  }

/** @} */
