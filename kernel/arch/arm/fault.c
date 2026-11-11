/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "arm_cpu.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/fatal.h>
#include <pbl/kernel/mpu.h>
#include <pbl/kernel/thread.h>
#include <pbl/logging/logging.h>
#include <pbl/util/size.h>

#include <pbl_arch_coredump.h>

enum {
  FRAME_R0,
  FRAME_R1,
  FRAME_R2,
  FRAME_R3,
  FRAME_R12,
  FRAME_LR,
  FRAME_PC,
  FRAME_XPSR,
  FRAME_WORDS,
};

#define FP_FRAME_WORDS 26U

#define EXC_RETURN_SPSEL (1UL << 2)
#define EXC_RETURN_FTYPE (1UL << 4)

#define XPSR_T (1UL << 24)

// How far an overflowed thread's SP is moved up, back into its stack, for the landing function.
#define OVERFLOW_SP_SKIP_WORDS 256U

struct fault_bit {
  uint8_t bit;
  const char *name;
};

static const struct fault_bit s_ufsr_bits[] = {
  {9, "DIVBYZERO"}, {8, "UNALIGNED"}, {4, "STKOF"},      {3, "NOCP"},
  {2, "INVPC"},     {1, "INVSTATE"},  {0, "UNDEFINSTR"},
};

static const struct fault_bit s_bfsr_bits[] = {
  {4, "STKERR"}, {3, "UNSTKERR"}, {2, "IMPRECISERR"}, {1, "PRECISERR"}, {0, "IBUSERR"},
};

static const struct fault_bit s_mmfsr_bits[] = {
  {4, "MSTKERR"},
  {3, "MUNSTKERR"},
  {1, "DACCVIOL"},
  {0, "IACCVIOL"},
};

static void prv_dump_bits(uint32_t value, const struct fault_bit *bits, size_t num_bits) {
  for (size_t i = 0; i < num_bits; i++) {
    if (value & (1UL << bits[i].bit)) {
      PBL_LOG_FAULT("    %s = yes", bits[i].name);
    }
  }
}

static void prv_dump_cfsr(uint32_t cfsr) {
  PBL_LOG_FAULT("CFSR (Configurable Fault) = 0x%" PRIx32, cfsr);

  const uint32_t ufsr = cfsr >> 16;
  if (ufsr != 0U) {
    PBL_LOG_FAULT("  Usage Fault Status Register:");
    prv_dump_bits(ufsr, s_ufsr_bits, ARRAY_LENGTH(s_ufsr_bits));
  }

  const uint32_t bfsr = (cfsr >> 8) & 0xFFU;
  if (bfsr != 0U) {
    PBL_LOG_FAULT("  Bus Fault Status Register:");
    if (cfsr & ARM_SCB_CFSR_BFARVALID) {
      PBL_LOG_FAULT("    BFARVALID = yes 0x%" PRIx32, arch_readl(ARM_SCB_BFAR));
    }
    prv_dump_bits(bfsr, s_bfsr_bits, ARRAY_LENGTH(s_bfsr_bits));
  }

  const uint32_t mmfsr = cfsr & 0xFFU;
  if (mmfsr != 0U) {
    PBL_LOG_FAULT("  Memory Management Fault Status Register:");
    if (cfsr & ARM_SCB_CFSR_MMARVALID) {
      PBL_LOG_FAULT("    MMFARVALID = yes 0x%" PRIx32, arch_readl(ARM_SCB_MMFAR));
    }
    prv_dump_bits(mmfsr, s_mmfsr_bits, ARRAY_LENGTH(s_mmfsr_bits));
  }
}

static void prv_dump_frame(const uint32_t *frame) {
  PBL_LOG_FAULT("R0 = 0x%" PRIx32, frame[FRAME_R0]);
  PBL_LOG_FAULT("R1 = 0x%" PRIx32, frame[FRAME_R1]);
  PBL_LOG_FAULT("R2 = 0x%" PRIx32, frame[FRAME_R2]);
  PBL_LOG_FAULT("R3 = 0x%" PRIx32, frame[FRAME_R3]);
  PBL_LOG_FAULT("R12 = 0x%" PRIx32, frame[FRAME_R12]);
  PBL_LOG_FAULT("SP = %p", (const void *)&frame[FRAME_WORDS]);
  PBL_LOG_FAULT("LR [R14] = 0x%" PRIx32 "  subroutine call return address", frame[FRAME_LR]);
  PBL_LOG_FAULT("PC [R15] = 0x%" PRIx32 "  program counter", frame[FRAME_PC]);
  PBL_LOG_FAULT("PSR = 0x%" PRIx32, frame[FRAME_XPSR]);
}

// No thread runs before the scheduler starts.
static const char *prv_thread_name(void) {
  const struct pbl_thread *t = pbl_thread_current();
  return (t != nullptr) ? pbl_thread_name(t) : "none";
}

static void prv_dump(const uint32_t *frame, uint32_t cfsr) {
  if (frame != nullptr) {
    prv_dump_frame(frame);
  }
  prv_dump_cfsr(cfsr);
  PBL_LOG_FAULT("Thread: %s", prv_thread_name());
}

static bool prv_hit_stack_guard(const uint32_t *frame, uint32_t exc_return, uint32_t cfsr) {
  if (cfsr & ARM_SCB_CFSR_MMARVALID) {
    const uintptr_t addr = arch_readl(ARM_SCB_MMFAR);
    return pbl_mpu_region_is_stack_guard(addr, addr + 1U);
  }
  if (cfsr & ARM_SCB_CFSR_MSTKERR) {
    // Stacking faults leave MMFAR invalid, but SP already points at the frame.
    const uintptr_t start = (uintptr_t)frame;
    const size_t words = (exc_return & EXC_RETURN_FTYPE) ? FRAME_WORDS : FP_FRAME_WORDS;
    return pbl_mpu_region_is_stack_guard(start, start + words * sizeof(uint32_t));
  }
  return false;
}

// Only an unprivileged thread, faulting on its own stack, can be sent to the landing function.
static bool prv_is_recoverable(uint32_t exc_return) {
#ifdef CONFIG_USERSPACE
  return (exc_return & EXC_RETURN_SPSEL) && (arm_control_get() & ARM_CONTROL_NPRIV);
#else
  return false;
#endif
}

static void prv_fill_info(struct pbl_fault_info *info, enum pbl_fault_reason reason,
                          const uint32_t *frame, uint32_t cfsr) {
  *info = (struct pbl_fault_info){.reason = reason};
  if (frame != nullptr) {
    info->pc = frame[FRAME_PC];
    info->lr = frame[FRAME_LR];
  }
  if (cfsr & ARM_SCB_CFSR_BFARVALID) {
    info->addr = arch_readl(ARM_SCB_BFAR);
  } else if (cfsr & ARM_SCB_CFSR_MMARVALID) {
    info->addr = arch_readl(ARM_SCB_MMFAR);
  }
}

#ifdef CONFIG_USERSPACE
[[noreturn]] static void prv_thread_fault(uint32_t reason, uintptr_t pc, uintptr_t lr,
                                          uintptr_t addr) {
  const struct pbl_fault_info info = {
    .reason = (enum pbl_fault_reason)reason,
    .pc = pc,
    .lr = lr,
    .addr = addr,
  };
  pbl_fault_thread_handler(&info);
}

// Return to the thread privileged, in prv_thread_fault(), which never goes back to the code that
// faulted. The fault is passed on in the argument registers.
static void prv_redirect(uint32_t *frame, const struct pbl_fault_info *info) {
  arch_writel(arch_readl(ARM_SCB_CFSR), ARM_SCB_CFSR);

  frame[FRAME_R0] = (uint32_t)info->reason;
  frame[FRAME_R1] = info->pc;
  frame[FRAME_R2] = info->lr;
  frame[FRAME_R3] = info->addr;
  frame[FRAME_LR] = 0U;
  frame[FRAME_PC] = (uint32_t)(uintptr_t)prv_thread_fault;
  // Clears the IT/ICI state the faulting instruction may have left.
  frame[FRAME_XPSR] = XPSR_T;

  arm_control_set(arm_control_get() & ~ARM_CONTROL_NPRIV);
}
#endif

static void prv_handle(const struct pbl_fault_info *info, uint32_t *frame, bool recoverable) {
#ifdef CONFIG_USERSPACE
  if (recoverable) {
    prv_redirect(frame, info);
    return;
  }
#endif
  pbl_fault_fatal_handler(info);
}

static void prv_stack_overflow(uint32_t *frame, uint32_t exc_return, uint32_t cfsr,
                               bool can_recover) {
  // A stacking error leaves the frame (partly) in the guard, unreadable.
  const bool frame_valid = !(cfsr & (ARM_SCB_CFSR_MSTKERR | ARM_SCB_CFSR_STKOF));
  struct pbl_fault_info info;
  prv_fill_info(&info, PBL_FAULT_STACK_OVERFLOW, frame_valid ? frame : nullptr, cfsr);
  if (info.addr == 0U) {
    // Where the frame went, for want of a valid MMFAR.
    info.addr = (uintptr_t)frame;
  }

  const bool recoverable = can_recover && prv_is_recoverable(exc_return);
  if (!recoverable) {
    pbl_fault_fatal_record(&info);
  }

  PBL_LOG_FAULT("Stack overflow [thread: %s]", prv_thread_name());
  prv_dump(frame_valid ? frame : nullptr, cfsr);

  if (!recoverable) {
    pbl_fault_fatal_handler(&info);
  }

  // Move SP back up into the stack, out of the guard, for the landing function.
  frame += OVERFLOW_SP_SKIP_WORDS;
  arm_psp_set((uint32_t)(uintptr_t)frame);
  prv_handle(&info, frame, true);
}

static void prv_fault(enum pbl_fault_reason reason, uint32_t *frame, uint32_t exc_return) {
  const uint32_t cfsr = arch_readl(ARM_SCB_CFSR);

  struct pbl_fault_info info;
  prv_fill_info(&info, reason, frame, cfsr);

  const bool recoverable = prv_is_recoverable(exc_return);
  if (!recoverable) {
    pbl_fault_fatal_record(&info);
  }

  prv_dump(frame, cfsr);

  prv_handle(&info, frame, recoverable);
}

static void prv_log_mpu_line(void *ctx, const char *line) {
  PBL_LOG_FAULT("%s", line);
}

static PBL_USED void prv_mem_manage(uint32_t *frame, uint32_t exc_return) {
  // On a stack overflow the frame may be in the guard: don't touch it before checking.
  PBL_LOG_FAULT("\r\n\r\n[Memory Management Failure!]");
  PBL_LOG_FAULT("Configured Regions: ");
  pbl_mpu_regions_print(prv_log_mpu_line, nullptr);

  const uint32_t cfsr = arch_readl(ARM_SCB_CFSR);
  if (prv_hit_stack_guard(frame, exc_return, cfsr)) {
    prv_stack_overflow(frame, exc_return, cfsr, true);
    return;
  }

  prv_fault(PBL_FAULT_MEMORY, frame, exc_return);
}

static PBL_USED void prv_bus_fault(uint32_t *frame, uint32_t exc_return) {
  PBL_LOG_FAULT("\r\n\r\n[BusFault_Handler!]");
  prv_fault(PBL_FAULT_BUS, frame, exc_return);
}

static PBL_USED void prv_usage_fault(uint32_t *frame, uint32_t exc_return) {
  PBL_LOG_FAULT("\r\n\r\n[UsageFault_Handler!]");

  // Stack limit violation (PSPLIM/MSPLIM); nothing was stacked.
  const uint32_t cfsr = arch_readl(ARM_SCB_CFSR);
  if (cfsr & ARM_SCB_CFSR_STKOF) {
    prv_stack_overflow(frame, exc_return, cfsr, true);
    return;
  }

  prv_fault(PBL_FAULT_USAGE, frame, exc_return);
}

static PBL_USED void prv_hard_fault(uint32_t *frame, uint32_t exc_return) {
  // Faults inside exception handlers, e.g. the context switch saving a thread's registers onto
  // an exhausted stack, escalate straight to a HardFault.
  const uint32_t cfsr = arch_readl(ARM_SCB_CFSR);
  if (prv_hit_stack_guard(frame, exc_return, cfsr)) {
    prv_stack_overflow(frame, exc_return, cfsr, false);
  }

  struct pbl_fault_info info;
  prv_fill_info(&info, PBL_FAULT_HARD, frame, cfsr);
  pbl_fault_fatal_record(&info);

  const uint32_t hfsr = arch_readl(ARM_SCB_HFSR);
  PBL_LOG_FAULT("\r\n\r\n[Hard fault handler]");
  PBL_LOG_FAULT("SHCSR (System Handler)    = 0x%" PRIx32, arch_readl(ARM_SCB_SHCSR));
  PBL_LOG_FAULT("HFSR (Hard Fault)         = 0x%" PRIx32, hfsr);
  PBL_LOG_FAULT("    Forced = %s", (hfsr & ARM_SCB_HFSR_FORCED) ? "yes" : "no");
  prv_dump(frame, cfsr);

  pbl_fault_fatal_handler(&info);
}

// r0: the exception frame, on the stack EXC_RETURN says; r1: EXC_RETURN.
#define FAULT_HANDLER(name, handler)    \
  void PBL_NAKED name(void) {           \
    __asm(PBL_ARCH_COREDUMP_FAULT_ENTRY \
          "tst lr, #4\n"                \
          "ite eq\n"                    \
          "mrseq r0, msp\n"             \
          "mrsne r0, psp\n"             \
          "mov r1, lr\n"                \
          "b %0\n" ::"i"(handler));     \
  }

FAULT_HANDLER(HardFault_Handler, prv_hard_fault)
FAULT_HANDLER(MemManage_Handler, prv_mem_manage)
FAULT_HANDLER(BusFault_Handler, prv_bus_fault)
FAULT_HANDLER(UsageFault_Handler, prv_usage_fault)
