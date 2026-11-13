/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/clk/sf32lb.h>
#include <pbl/drivers/flash.h>
#include <pbl/drivers/ram.h>
#include <pbl/kernel/idle.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/kernel/section.h>
#include <pbl/kernel/time.h>
#include <pbl/soc/sf32lb/sleep.h>
#include <pbl/util/math.h>
#include <pbl/util/size.h>

#ifdef CONFIG_SHELL
#include <pbl/shell/shell.h>
#endif

#include <devicetree/regs.h>
#include <hardware/sf32lb52_hpsys_aon.h>

#define HPSYS_AON(reg) (PBL_DT_REG_HPSYS_AON + SF32LB52_HPSYS_AON_##reg##_OFFSET)

// Settling time after waking up from deep sleep
#define DEEPSLEEP_EXIT_NOPS 10

enum {
  IDLE_WFI,
  IDLE_DEEPWFI,
  IDLE_DEEPSLEEP,
};

static bool s_force_wfi;

#ifdef CONFIG_FLASH
static uint32_t s_enabled[(CONFIG_NUM_IRQS + 31) / 32];

// Leave AON as the only wakeup source, the other handlers run from flash
static void prv_irqs_aon_only(void) {
  for (pbl_irq_t irq = 0; irq < CONFIG_NUM_IRQS; irq++) {
    if (pbl_irq_is_enabled(irq)) {
      s_enabled[irq / 32] |= PBL_BIT(irq % 32);
      pbl_irq_disable(irq);
    } else {
      s_enabled[irq / 32] &= ~PBL_BIT(irq % 32);
    }
  }

  pbl_irq_enable(PBL_IRQN(AON));
}

// AON stays enabled: its handler clears the wakeup status, then disables it
static void prv_irqs_restore(void) {
  for (pbl_irq_t irq = 0; irq < CONFIG_NUM_IRQS; irq++) {
    if ((s_enabled[irq / 32] & PBL_BIT(irq % 32)) != 0U) {
      pbl_irq_enable(irq);
    }
  }
}

// Inline, unlike pbl_modifyl(): the flash is down
static PBL_ALWAYS_INLINE void prv_update(uint32_t clear, uint32_t set, uintptr_t addr) {
  pbl_writel((pbl_readl(addr) & ~clear) | set, addr);
}
#endif // CONFIG_FLASH

PBL_SECTION_RAM static void prv_enter_wfi(void) {
  pbl_cpu_idle();
}

PBL_SECTION_RAM static void prv_enter_deepwfi(void) {
#ifdef CONFIG_RAM
  pbl_ram_quiesce(EXTRAM);
#endif
  pbl_cpu_idle_deep();
}

#ifdef CONFIG_FLASH
// Runs with the flash powered down
PBL_SECTION_RAM static void prv_deepsleep(void *arg) {
  struct pbl_clk_sf32lb52_hxt48_state clocks;

  // HXT48 stops in deep sleep
  pbl_clk_sf32lb52_hxt48_suspend(&clocks);

  prv_update(0U, HPSYS_AON_ANACR_PA_ISO, HPSYS_AON(ANACR));
  prv_update(0U, HPSYS_AON_ANACR_VHP_ISO, HPSYS_AON(ANACR));

  prv_update(HPSYS_AON_ISSR_HP_ACTIVE, 0U, HPSYS_AON(ISSR));
  pbl_writel(PBL_FIELD_PREP(HPSYS_AON_PMR_MODE_MASK, HPSYS_AON_PMR_MODE_DEEP_SLEEP),
             HPSYS_AON(PMR));

  pbl_cpu_idle();
  for (int i = 0; i < DEEPSLEEP_EXIT_NOPS; i++) {
    __asm volatile("nop");
  }

  prv_update(HPSYS_AON_ANACR_PA_ISO, 0U, HPSYS_AON(ANACR));
  prv_update(HPSYS_AON_ANACR_VHP_ISO, 0U, HPSYS_AON(ANACR));

  prv_update(0U, HPSYS_AON_ISSR_HP_ACTIVE, HPSYS_AON(ISSR));
  pbl_writel(0U, HPSYS_AON(PMR));

  while ((pbl_readl(HPSYS_AON(ACR)) & HPSYS_AON_ACR_HXT48_RDY) == 0U) {
  }

  pbl_clk_sf32lb52_hxt48_resume(&clocks);
}

PBL_SECTION_RAM static void prv_enter_deepsleep(void) {
  prv_irqs_aon_only();
#ifdef CONFIG_RAM
  pbl_ram_suspend(EXTRAM);
#endif
  pbl_flash_power_down(FLASH, prv_deepsleep, nullptr);
#ifdef CONFIG_RAM
  pbl_ram_resume(EXTRAM);
#endif
  prv_irqs_restore();
}
#endif // CONFIG_FLASH

const struct pbl_idle_state pbl_soc_idle_states[] = {
  [IDLE_WFI] = {.name = "wfi", .min_ticks = 1, .exit_ticks = 0, .enter = prv_enter_wfi},
  [IDLE_DEEPWFI] = {.name = "deepwfi", .min_ticks = 1, .exit_ticks = 0, .enter = prv_enter_deepwfi},
#ifdef CONFIG_FLASH
  [IDLE_DEEPSLEEP] = {
    .name = "deepsleep",
    .min_ticks = PBL_TICK_HZ / 20,
    .exit_ticks = 4,
    .enter = prv_enter_deepsleep
  },
#endif
};
const size_t pbl_soc_num_idle_states = ARRAY_LENGTH(pbl_soc_idle_states);

static_assert(IDLE_WFI == SOC_SF32LB_WFI - 1 && IDLE_DEEPSLEEP == SOC_SF32LB_DEEPSLEEP - 1,
              "idle states follow the sleep levels");

PBL_SECTION_RAM int pbl_soc_idle_state_limit(void) {
  // The debug flag forces plain WFI (deep WFI and deep sleep disabled).
  if (s_force_wfi) {
    return IDLE_WFI;
  }

  return (int)soc_sf32lb_sleep_max_level() - 1;
}

PBL_IRQ_DIRECT(AON, 0, PBL_IRQ_ZERO_LATENCY) {
  uint32_t status;

  pbl_irq_disable(PBL_IRQN(AON));
  pbl_writel(0U, HPSYS_AON(PMR));

  status = pbl_readl(HPSYS_AON(WSR)) & ~HPSYS_AON_WSR_PIN_ALL;
  pbl_writel(status | HPSYS_AON_WCR_AON, HPSYS_AON(WCR));
}

#ifdef CONFIG_SHELL
static int prv_cmd_wfi(const struct pbl_shell *sh, size_t argc, char **argv) {
  if (argv[1][0] == '1') {
    s_force_wfi = true;
    pbl_shell_print(sh, "WFI forced ON (deep WFI and deep sleep disabled)");
  } else {
    s_force_wfi = false;
    pbl_shell_print(sh, "WFI forced OFF (deep WFI and deep sleep allowed)");
  }
  return 0;
}

PBL_SHELL_SUBCMD_ADD(sub_sys, wfi, nullptr, "Force plain WFI when idle <0|1>", prv_cmd_wfi, 2, 0);
#endif
