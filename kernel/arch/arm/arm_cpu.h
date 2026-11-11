/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/kernel/compiler.h>

#include <pbl_arch_mmio.h>

// System control block
#define ARM_SCB_ICSR                 0xE000ED04UL
#define ARM_SCB_ICSR_PENDSVSET       (1UL << 28)
#define ARM_SCB_ICSR_NMIPENDSET      (1UL << 31)
#define ARM_SCB_VTOR                 0xE000ED08UL
#define ARM_SCB_AIRCR                0xE000ED0CUL
#define ARM_SCB_AIRCR_VECTKEY        (0x05FAUL << 16)
#define ARM_SCB_AIRCR_VECTKEY_MSK    (0xFFFFUL << 16)
#define ARM_SCB_AIRCR_PRIGROUP_POS   8U
#define ARM_SCB_AIRCR_PRIGROUP_MSK   (7UL << ARM_SCB_AIRCR_PRIGROUP_POS)
#define ARM_SCB_AIRCR_SYSRESETREQ    (1UL << 2)
#define ARM_SCB_CCR                  0xE000ED14UL
#define ARM_SCB_CCR_DIV_0_TRP        (1UL << 4)
#define ARM_SCB_CCR_DC               (1UL << 16)
#define ARM_SCB_CCR_IC               (1UL << 17)
#define ARM_SCB_SHPR(exc)            (0xE000ED18UL + (exc) - 4U)
#define ARM_SCB_SHCSR                0xE000ED24UL
#define ARM_SCB_SHCSR_MEMFAULTENA    (1UL << 16)
#define ARM_SCB_SHCSR_BUSFAULTENA    (1UL << 17)
#define ARM_SCB_SHCSR_USGFAULTENA    (1UL << 18)
#define ARM_SCB_CFSR                 0xE000ED28UL
#define ARM_SCB_CFSR_MSTKERR         (1UL << 4)
#define ARM_SCB_CFSR_MMARVALID       (1UL << 7)
#define ARM_SCB_CFSR_BFARVALID       (1UL << 15)
#define ARM_SCB_CFSR_STKOF           (1UL << 20)
#define ARM_SCB_HFSR                 0xE000ED2CUL
#define ARM_SCB_HFSR_FORCED          (1UL << 30)
#define ARM_SCB_MMFAR                0xE000ED34UL
#define ARM_SCB_BFAR                 0xE000ED38UL
#define ARM_SCB_CCSIDR               0xE000ED80UL
#define ARM_SCB_CSSELR               0xE000ED84UL
#define ARM_SCB_CPACR                0xE000ED88UL
#define ARM_SCB_CPACR_CP10_CP11_FULL (0xFUL << 20)

// Exception numbers
#define ARM_EXC_MEMMANAGE  4U
#define ARM_EXC_BUSFAULT   5U
#define ARM_EXC_USAGEFAULT 6U
#define ARM_EXC_SVCALL     11U
#define ARM_EXC_PENDSV     14U

// Cache identification and set/way maintenance
#define ARM_CCSIDR_WAYS(x)   (((x) >> 3) & 0x3FFUL)
#define ARM_CCSIDR_SETS(x)   (((x) >> 13) & 0x7FFFUL)
#define ARM_CSSELR_L1_DCACHE 0U
#define ARM_DCSW_SET_SHIFT   5U
#define ARM_DCSW(set, way)   (((set) << ARM_DCSW_SET_SHIFT) | ((way) << 30))
#define ARM_ICIALLU          0xE000EF50UL
#define ARM_DCISW            0xE000EF60UL
#define ARM_DCCSW            0xE000EF6CUL
#define ARM_DCCISW           0xE000EF74UL

// Floating point extension
#define ARM_FPU_FPCCR       0xE000EF34UL
#define ARM_FPU_FPCCR_LSPEN (1UL << 30)
#define ARM_FPU_FPCCR_ASPEN (1UL << 31)
#define ARM_CONTROL_NPRIV   (1UL << 0)
#define ARM_CONTROL_FPCA    (1UL << 2)
#define ARM_PRIMASK_PM      (1UL << 0)

// Debug
#define ARM_DHCSR           0xE000EDF0UL
#define ARM_DHCSR_C_DEBUGEN (1UL << 0)

// NVIC
#define ARM_NVIC_ICER(i) (0xE000E180UL + ((i) * 4U))
#define ARM_NVIC_ICPR(i) (0xE000E280UL + ((i) * 4U))
#define ARM_NVIC_IABR(i) (0xE000E300UL + ((i) * 4U))

// MPU
#define ARM_MPU_TYPE            0xE000ED90UL
#define ARM_MPU_TYPE_DREGION(x) (((x) >> 8) & 0xFFUL)
#define ARM_MPU_CTRL            0xE000ED94UL
#define ARM_MPU_CTRL_ENABLE     (1UL << 0)
#define ARM_MPU_CTRL_HFNMIENA   (1UL << 1)
#define ARM_MPU_CTRL_PRIVDEFENA (1UL << 2)
#define ARM_MPU_RNR             0xE000ED98UL
#define ARM_MPU_RBAR            0xE000ED9CUL
#define ARM_MPU_RASR            0xE000EDA0UL
#define ARM_MPU_RLAR            0xE000EDA0UL
#define ARM_MPU_MAIR(i)         (0xE000EDC0UL + ((i) * 4U))
// ARMv7-M
#define ARM_MPU_RBAR_VALID    (1UL << 4)
#define ARM_MPU_RASR_ENABLE   (1UL << 0)
#define ARM_MPU_RASR_SIZE_POS 1U
#define ARM_MPU_RASR_SIZE_MSK (0x1FUL << ARM_MPU_RASR_SIZE_POS)
#define ARM_MPU_RASR_SRD_POS  8U
#define ARM_MPU_RASR_SRD_MSK  (0xFFUL << ARM_MPU_RASR_SRD_POS)
#define ARM_MPU_RASR_B        (1UL << 16)
#define ARM_MPU_RASR_C        (1UL << 17)
#define ARM_MPU_RASR_S        (1UL << 18)
#define ARM_MPU_RASR_TEX_POS  19U
#define ARM_MPU_RASR_AP_POS   24U
#define ARM_MPU_RASR_AP_MSK   (0x7UL << ARM_MPU_RASR_AP_POS)
#define ARM_MPU_RASR_XN       (1UL << 28)
// ARMv8-M
#define ARM_MPU_RBAR_XN            (1UL << 0)
#define ARM_MPU_RBAR_AP_POS        1U
#define ARM_MPU_RBAR_AP_MSK        (0x3UL << ARM_MPU_RBAR_AP_POS)
#define ARM_MPU_RBAR_SH_POS        3U
#define ARM_MPU_RBAR_SH_NON        0U
#define ARM_MPU_RBAR_SH_INNER      3U
#define ARM_MPU_RBAR_BASE_MSK      0xFFFFFFE0UL
#define ARM_MPU_RLAR_EN            (1UL << 0)
#define ARM_MPU_RLAR_ATTRINDX_POS  1U
#define ARM_MPU_RLAR_LIMIT_MSK     0xFFFFFFE0UL
#define ARM_MPU_MAIR_ATTR(o, i)    ((uint8_t)(((o) << 4) | (i)))
#define ARM_MPU_MAIR_DEVICE_NGNRNE 0x0U
#define ARM_MPU_MAIR_NON_CACHEABLE 0x4U
#define ARM_MPU_MAIR_WT_RA         0xAU
#define ARM_MPU_MAIR_WB_RA_WA      0xFU

#define ARM_SPECIAL_REG(name)                                    \
  static PBL_ALWAYS_INLINE uint32_t arm_##name##_get(void) {     \
    uint32_t val;                                                \
    __asm volatile("mrs %0, " #name : "=r"(val)::"memory");      \
    return val;                                                  \
  }                                                              \
  static PBL_ALWAYS_INLINE void arm_##name##_set(uint32_t val) { \
    __asm volatile("msr " #name ", %0" ::"r"(val) : "memory");   \
  }

ARM_SPECIAL_REG(primask)
ARM_SPECIAL_REG(basepri)
ARM_SPECIAL_REG(faultmask)
ARM_SPECIAL_REG(control)
ARM_SPECIAL_REG(psp)
#ifdef CONFIG_CPU_CORTEX_M_HAS_SPLIM
ARM_SPECIAL_REG(psplim)
#endif

static PBL_ALWAYS_INLINE void arm_dsb(void) {
  __asm volatile("dsb 0xf" ::: "memory");
}

static PBL_ALWAYS_INLINE void arm_isb(void) {
  __asm volatile("isb 0xf" ::: "memory");
}

static PBL_ALWAYS_INLINE void arm_dmb(void) {
  __asm volatile("dmb 0xf" ::: "memory");
}

static PBL_ALWAYS_INLINE void arm_wfi(void) {
  __asm volatile("wfi" ::: "memory");
}

static PBL_ALWAYS_INLINE void arm_irq_disable(void) {
  __asm volatile("cpsid i" ::: "memory");
}

static PBL_ALWAYS_INLINE void arm_irq_enable(void) {
  __asm volatile("cpsie i" ::: "memory");
}

static PBL_ALWAYS_INLINE void arm_shpr_set(uint32_t exc, uint8_t prio) {
  arch_writeb(prio, ARM_SCB_SHPR(exc));
}
