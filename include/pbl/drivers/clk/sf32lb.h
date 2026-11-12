/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/clk.h>

/**
 * @defgroup drivers_clk_sf32lb SF32LB RCC clocks
 * @ingroup drivers_clk
 * @brief @ref drivers_clk controllers of the SF32LB52: the HPSYS RCC, the PMUC oscillators and
 * the RTC clock.
 *
 * The HPSYS RCC derives the system and peripheral clocks from the HRC48 and HXT48 oscillators.
 * Gate numbers are (register index << 5) | bit, register 0 being ENR1, and a gate runs at the
 * rate of the clock it gates; the clocks before the gates follow.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_clk_controller pbl_clk_sf32lb_hpsys_rcc;
/** @endcond */

/** HPSYS RCC clock controller. */
#define SF32LB_HPSYS_RCC (&pbl_clk_sf32lb_hpsys_rcc)

/** @name HPSYS RCC clocks
 * @{
 */
#define SF32LB52_HPSYS_CLK_DMAC1    0
#define SF32LB52_HPSYS_CLK_MAILBOX1 1
#define SF32LB52_HPSYS_CLK_PINMUX1  2
#define SF32LB52_HPSYS_CLK_USART2   4
#define SF32LB52_HPSYS_CLK_EZIP1    5
#define SF32LB52_HPSYS_CLK_EPIC     6
#define SF32LB52_HPSYS_CLK_LCDC1    7
#define SF32LB52_HPSYS_CLK_I2S1     8
#define SF32LB52_HPSYS_CLK_SYSCFG1  10
#define SF32LB52_HPSYS_CLK_EFUSEC   11
#define SF32LB52_HPSYS_CLK_AES      12
#define SF32LB52_HPSYS_CLK_CRC1     13
#define SF32LB52_HPSYS_CLK_TRNG     14
#define SF32LB52_HPSYS_CLK_GPTIM1   15
#define SF32LB52_HPSYS_CLK_GPTIM2   16
#define SF32LB52_HPSYS_CLK_BTIM1    17
#define SF32LB52_HPSYS_CLK_BTIM2    18
#define SF32LB52_HPSYS_CLK_SPI1     20
#define SF32LB52_HPSYS_CLK_SPI2     21
#define SF32LB52_HPSYS_CLK_EXTDMA   22
#define SF32LB52_HPSYS_CLK_SECU1    23
#define SF32LB52_HPSYS_CLK_PDM1     25
#define SF32LB52_HPSYS_CLK_I2C1     27
#define SF32LB52_HPSYS_CLK_I2C2     28
#define SF32LB52_HPSYS_CLK_PTC1     31
#define SF32LB52_HPSYS_CLK_GPIO1    32
#define SF32LB52_HPSYS_CLK_MPI1     33
#define SF32LB52_HPSYS_CLK_MPI2     34
#define SF32LB52_HPSYS_CLK_SDMMC1   36
#define SF32LB52_HPSYS_CLK_USBC     38
#define SF32LB52_HPSYS_CLK_I2C3     40
#define SF32LB52_HPSYS_CLK_ATIM1    41
#define SF32LB52_HPSYS_CLK_USART3   44
#define SF32LB52_HPSYS_CLK_AUDCODEC 51
#define SF32LB52_HPSYS_CLK_AUDPRC   52
#define SF32LB52_HPSYS_CLK_GPADC    54
#define SF32LB52_HPSYS_CLK_TSEN     55
#define SF32LB52_HPSYS_CLK_I2C4     57
/** @} */

/** @name HPSYS RCC clocks before the gates, the same on every series
 * @{
 */
/** clk_hpsys: HRC48, HXT48 or DLL1. */
#define SF32LB_HPSYS_CLK_SYS 64
/** hclk_hpsys, clk_hpsys divided: AHB, CPU and memories. */
#define SF32LB_HPSYS_CLK_HCLK 65
/** pclk_hpsys, hclk_hpsys divided: APB. */
#define SF32LB_HPSYS_CLK_PCLK 66
/** pclk2_hpsys, hclk_hpsys divided: HPSYS AON registers. */
#define SF32LB_HPSYS_CLK_PCLK2 67
/** DLL1, from HXT48. */
#define SF32LB_HPSYS_CLK_DLL1 68
/** DLL2, from HXT48. */
#define SF32LB_HPSYS_CLK_DLL2 69
/** clk_peri_hpsys: HRC48 or HXT48, for USART, SPI and I2C. */
#define SF32LB_HPSYS_CLK_PERI 70
/** clk_peri_hpsys divided by 2, for GPTIM2 and BTIM2. */
#define SF32LB_HPSYS_CLK_PERI_DIV2 71
/** MPI1 kernel clock: clk_peri_hpsys, DLL1 or DLL2. */
#define SF32LB_HPSYS_CLK_MPI1_K 72
/** MPI2 kernel clock: clk_peri_hpsys, DLL1 or DLL2. */
#define SF32LB_HPSYS_CLK_MPI2_K 73
/** USB kernel clock: clk_hpsys or DLL2, divided. */
#define SF32LB_HPSYS_CLK_USBC_K 74
/** MPI3 kernel clock: clk_peri_hpsys, DLL1 or DLL2. SF32LB57 only. */
#define SF32LB_HPSYS_CLK_MPI3_K 75
/** @} */

/** @cond INTERNAL_HIDDEN */
#define SF32LB_HPSYS_NUM_CLKS 76

#define SF32LB52_HPSYS_CLK_SYS       64
#define SF32LB52_HPSYS_CLK_HCLK      65
#define SF32LB52_HPSYS_CLK_PCLK      66
#define SF32LB52_HPSYS_CLK_PCLK2     67
#define SF32LB52_HPSYS_CLK_DLL1      68
#define SF32LB52_HPSYS_CLK_DLL2      69
#define SF32LB52_HPSYS_CLK_PERI      70
#define SF32LB52_HPSYS_CLK_PERI_DIV2 71
#define SF32LB52_HPSYS_CLK_MPI1_K    72
#define SF32LB52_HPSYS_CLK_MPI2_K    73
#define SF32LB52_HPSYS_CLK_USBC_K    74
/** @endcond */

/** @name SF32LB57 HPSYS RCC gates
 * @{
 */
#define SF32LB57_HPSYS_CLK_DMAC1    0
#define SF32LB57_HPSYS_CLK_MAILBOX1 1
#define SF32LB57_HPSYS_CLK_PINMUX1  2
#define SF32LB57_HPSYS_CLK_USART2   4
#define SF32LB57_HPSYS_CLK_EZIP1    5
#define SF32LB57_HPSYS_CLK_EPIC     6
#define SF32LB57_HPSYS_CLK_LCDC1    7
#define SF32LB57_HPSYS_CLK_I2S1     8
#define SF32LB57_HPSYS_CLK_I2S2     9
#define SF32LB57_HPSYS_CLK_SYSCFG1  10
#define SF32LB57_HPSYS_CLK_EFUSEC   11
#define SF32LB57_HPSYS_CLK_AES      12
#define SF32LB57_HPSYS_CLK_CRC1     13
#define SF32LB57_HPSYS_CLK_TRNG     14
#define SF32LB57_HPSYS_CLK_GPTIM1   15
#define SF32LB57_HPSYS_CLK_GPTIM2   16
#define SF32LB57_HPSYS_CLK_BTIM1    17
#define SF32LB57_HPSYS_CLK_BTIM2    18
#define SF32LB57_HPSYS_CLK_PWM1     19
#define SF32LB57_HPSYS_CLK_SPI1     20
#define SF32LB57_HPSYS_CLK_SPI2     21
#define SF32LB57_HPSYS_CLK_SECU1    23
#define SF32LB57_HPSYS_CLK_PDM1     25
#define SF32LB57_HPSYS_CLK_PDM2     26
#define SF32LB57_HPSYS_CLK_I2C1     27
#define SF32LB57_HPSYS_CLK_I2C2     28
#define SF32LB57_HPSYS_CLK_PTM1     31
#define SF32LB57_HPSYS_CLK_GPIO1    32
#define SF32LB57_HPSYS_CLK_MPI1     33
#define SF32LB57_HPSYS_CLK_MPI2     34
#define SF32LB57_HPSYS_CLK_MPI3     35
#define SF32LB57_HPSYS_CLK_SDMMC1   36
#define SF32LB57_HPSYS_CLK_SDMMC2   37
#define SF32LB57_HPSYS_CLK_USBC     38
#define SF32LB57_HPSYS_CLK_I2C3     40
#define SF32LB57_HPSYS_CLK_ATIM1    41
#define SF32LB57_HPSYS_CLK_ATIM2    42
#define SF32LB57_HPSYS_CLK_USART3   44
#define SF32LB57_HPSYS_CLK_DCMI     45
#define SF32LB57_HPSYS_CLK_CAN1     49
#define SF32LB57_HPSYS_CLK_CAN2     50
#define SF32LB57_HPSYS_CLK_AUDCODEC 51
#define SF32LB57_HPSYS_CLK_AUDPRC   52
#define SF32LB57_HPSYS_CLK_GPADC    54
#define SF32LB57_HPSYS_CLK_TSEN     55
#define SF32LB57_HPSYS_CLK_JPEGD    56
#define SF32LB57_HPSYS_CLK_I2C4     57
/** @} */

/** @cond INTERNAL_HIDDEN */
#define SF32LB57_HPSYS_CLK_SYS       64
#define SF32LB57_HPSYS_CLK_HCLK      65
#define SF32LB57_HPSYS_CLK_PCLK      66
#define SF32LB57_HPSYS_CLK_PCLK2     67
#define SF32LB57_HPSYS_CLK_DLL1      68
#define SF32LB57_HPSYS_CLK_DLL2      69
#define SF32LB57_HPSYS_CLK_PERI      70
#define SF32LB57_HPSYS_CLK_PERI_DIV2 71
#define SF32LB57_HPSYS_CLK_MPI1_K    72
#define SF32LB57_HPSYS_CLK_MPI2_K    73
#define SF32LB57_HPSYS_CLK_MPI3_K    75
#define SF32LB57_HPSYS_CLK_USBC_K    74
/** @endcond */

/** @brief HPSYS clocks saved while HXT48 is unusable. */
struct pbl_clk_sf32lb_hxt48_state {
  /** @cond INTERNAL_HIDDEN */
  uint32_t csr;
  uint32_t dll1cr;
  uint32_t dll2cr;
  /** @endcond */
};

/**
 * @brief Move clk_hpsys to HRC48 and stop the DLLs, before HXT48 stops or changes.
 *
 * For deep sleep and the crystal trim. Runs from RAM, with the flash down, and interrupts off.
 *
 * @param[out] state What pbl_clk_sf32lb_hxt48_resume() restores.
 */
void pbl_clk_sf32lb_hxt48_suspend(struct pbl_clk_sf32lb_hxt48_state *state);

/**
 * @brief Restart the DLLs that ran and move clk_hpsys back, once HXT48 is ready.
 *
 * Runs from RAM, with the flash down, and interrupts off.
 *
 * @param state What pbl_clk_sf32lb_hxt48_suspend() saved.
 */
void pbl_clk_sf32lb_hxt48_resume(const struct pbl_clk_sf32lb_hxt48_state *state);

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_clk_controller pbl_clk_sf32lb52_pmuc;
/** @endcond */

/** PMUC clock controller, the oscillators. */
#define SF32LB52_PMUC (&pbl_clk_sf32lb52_pmuc)

/**
 * @name PMUC clocks
 *
 * The rate of LRC10 is the filtered estimate of its calibration, updated every
 * @c CONFIG_CLK_SF32LB52_PMUC_LRC10_CAL_PERIOD_S seconds from the tick interrupt, and a
 * placeholder until the first gate, with or without Bluetooth; see the RC10K calibration page in
 * the driver documentation. LRC32 runs while used, HXT48_DLL gates the DLL buffer, and the
 * other oscillators always run.
 * @{
 */
#define SF32LB52_CLK_LRC10 0
#define SF32LB52_CLK_LRC32 1
#define SF32LB52_CLK_LXT32 2
#define SF32LB52_CLK_HRC48 3
#define SF32LB52_CLK_HXT48 4
#define SF32LB52_CLK_WDT   5
/** HXT48 as the DLLs take it, through a buffer of its own. */
#define SF32LB52_CLK_HXT48_DLL 6
/** @} */

/** @cond INTERNAL_HIDDEN */
#define SF32LB52_PMUC_NUM_CLKS 7
/** @endcond */

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_clk_controller pbl_clk_sf32lb57_pmuc;
/** @endcond */

/** SF32LB57 PMUC clock controller, the oscillators and clk_rtc. */
#define SF32LB57_PMUC (&pbl_clk_sf32lb57_pmuc)

/**
 * @name SF32LB57 PMUC clocks
 *
 * LRC10 runs at its nominal rate, uncalibrated. LRC32 and LXT32 run while used, HXT48_DLL gates
 * the DLL buffer, and the other oscillators always run.
 * @{
 */
#define SF32LB57_CLK_LRC10 0
#define SF32LB57_CLK_LRC32 1
#define SF32LB57_CLK_LXT32 2
#define SF32LB57_CLK_HRC48 3
#define SF32LB57_CLK_HXT48 4
/** clk_wdt: LRC10 or LRC32. */
#define SF32LB57_CLK_WDT 5
/** HXT48 as the DLLs take it, through a buffer of its own. */
#define SF32LB57_CLK_HXT48_DLL 6
/** clk_rtc: LRC10 or LXT32. */
#define SF32LB57_CLK_RTC 7
/** @} */

/** @cond INTERNAL_HIDDEN */
#define SF32LB57_PMUC_NUM_CLKS 8
/** @endcond */

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_clk_controller pbl_clk_sf32lb52_rtc;
/** @endcond */

/**
 * @brief RTC clock controller, with one clock, clk_rtc: LRC10 or LXT32, as the RTC selects.
 *
 * The RTC, LPTIM1 and LPTIM2 count it.
 */
#define SF32LB52_RTC_CLK (&pbl_clk_sf32lb52_rtc)

/** clk_rtc clock number. */
#define SF32LB52_CLK_RTC 0

/**
 * @name clk_rtc
 *
 * The clock the RTC, LPTIM1 and LPTIM2 count, whichever controller selects it: the RTC on the
 * SF32LB52, the PMUC on the SF32LB57.
 * @{
 */
#ifdef CONFIG_SOC_SF32LB57
#define SF32LB_CLK_RTC_CTRL SF32LB57_PMUC
#define SF32LB_CLK_RTC_ID   SF32LB57_CLK_RTC
#else
#define SF32LB_CLK_RTC_CTRL SF32LB52_RTC_CLK
#define SF32LB_CLK_RTC_ID   SF32LB52_CLK_RTC
#endif
/** @} */

/**
 * @name LPSYS RCC clocks
 *
 * The clocks of the LCPU. The LCPU changes them itself once it runs.
 * @{
 */
/** clk_lpsys: HRC48 or HXT48. */
#define SF32LB52_LPSYS_CLK_SYS 0
/** hclk_lpsys, clk_lpsys divided: the LCPU. */
#define SF32LB52_LPSYS_CLK_HCLK 1
/** pclk_lpsys, hclk_lpsys divided. */
#define SF32LB52_LPSYS_CLK_PCLK 2
/** pclk2_lpsys, hclk_lpsys divided. */
#define SF32LB52_LPSYS_CLK_PCLK2 3
/** clk_peri_lpsys: HRC48 or HXT48. */
#define SF32LB52_LPSYS_CLK_PERI 4
/** @} */

/** @cond INTERNAL_HIDDEN */
#define SF32LB52_LPSYS_NUM_CLKS 5
/** @endcond */

/** @} */
