/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "arm_syscall.h"

#include <stddef.h>
#include <stdint.h>

#include <pbl/kernel/assert.h>
#include <pbl/kernel/compiler.h>
#include <pbl/kernel/syscall.h>

#include <cmsis_core.h>
#include <kernel.h>
#include <pbl_arch_syscall.h>

static_assert(sizeof(((struct arch_user_call *)0)->saved_r4_r11) == 32,
              "pbl_user_call asm assumes push {r4-r11} saves 32 bytes");
static_assert(sizeof(uintptr_t) == sizeof(uint32_t), "pbl_user_call asm assumes 32-bit pointers");

uintptr_t arch_syscall_caller_sp(void) {
  return pbl_cur->backend.arch.syscall_sp;
}

PBL_USED uintptr_t arch_syscall_return_address(void) {
  return pbl_cur->backend.arch.syscall_lr;
}

PBL_USED bool arch_syscall_from_kernel(void) {
  return !arch_is_user_context();
}

void pbl_user_mode_enter(void) {
  __set_CONTROL(__get_CONTROL() | CONTROL_nPRIV_Msk);
  __ISB();
}

// ---- kernel-to-user calls ---------------------------------------------------

static PBL_USED void arch_user_call_enter(void (*fn)(void *), void *ctx, uintptr_t caller_lr,
                                          uintptr_t entry_sp, const uint32_t *saved_regs) {
  (void)ctx;

  PBL_ASSERTN(!arch_is_user_context());
  PBL_ASSERTN(fn != nullptr);

  struct pbl_arch_thread *arch = &pbl_cur->backend.arch;
  struct arch_user_call *call = &arch->user_call;
  PBL_ASSERTN(!call->active);

  *call = (struct arch_user_call){
    .active = true,
    .caller_lr = caller_lr,
    .entry_sp = entry_sp,
    .outer_syscall_lr = arch->syscall_lr,
    .outer_syscall_sp = arch->syscall_sp,
  };

  for (size_t i = 0; i < 8; ++i) {
    call->saved_r4_r11[i] = saved_regs[i];
  }
}

PBL_EXTERNALLY_VISIBLE void arch_user_call_resume(void);

static uintptr_t prv_user_call_return_pc(void) {
  extern const uint16_t __arch_user_call_svc[];

  // A 16-bit SVC returns to the next halfword.
  return (uintptr_t)__arch_user_call_svc + sizeof(uint16_t);
}

bool arch_user_call_return(uint32_t *frame) {
  // frame is the Cortex-M basic exception frame: r0 r1 r2 r3 r12 lr pc xpsr
  struct pbl_arch_thread *arch = &pbl_cur->backend.arch;
  struct arch_user_call *call = &arch->user_call;
  if (frame[6] != prv_user_call_return_pc() || !call->active) {
    return false;
  }

  call->active = false;

  // The callback may have made syscalls of its own; restore the state of the
  // one that entered the runtime.
  arch->syscall_lr = call->outer_syscall_lr;
  arch->syscall_sp = call->outer_syscall_sp;

  // Return from the exception into a fixed continuation, not back into the
  // callback. The continuation restores SP before using the stack.
  frame[0] = (uint32_t)call;
  frame[6] = (uint32_t)&arch_user_call_resume;
  frame[5] = 0;

  return true;
}

// The re-entry SVC is deliberately outside .syscall_text. It is only accepted
// while this call is active for the current thread, and the SVC handler
// resumes through saved kernel state instead of the callback's stack.
PBL_EXTERNALLY_VISIBLE PBL_NAKED PBL_USED void pbl_user_call(void (*fn)(void *), void *ctx) {
  __asm volatile(
      // Keep fn/ctx across the setup call. Copy r4-r11 too; the unprivileged
      // callee is not trusted to preserve the privileged caller's callee-saved registers.
      "  push {r4-r11}                    \n"
      "  push {r0, r1}                    \n"
      "  mov r2, lr                       \n" // r2 = privileged caller LR
      // These offsets are tied to the two pushes above:
      //   push {r4-r11} = 32 bytes, push {r0,r1} = 8 bytes.
      "  add r3, sp, #40                  \n" // r3 = entry SP before pushes
      "  add r12, sp, #8                  \n" // r12 = saved r4-r11 pointer
      "  sub sp, sp, #8                   \n" // keep stack 8-byte aligned for C call
      "  str r12, [sp]                    \n" // 5th arg: saved_regs pointer
      "  bl arch_user_call_enter   \n"
      "  add sp, sp, #8                   \n" // discard 5th arg + alignment pad
      "  pop {r2, r3}                     \n" // r2 = fn, r3 = ctx
      "  add sp, sp, #32                  \n" // saved r4-r11 copied to kernel state

      // Drop privilege inline (CONTROL.nPRIV = 1). Calling the C helper would
      // clobber the callback registers we just restored.
      "  mrs r0, control                  \n"
      "  orr r0, r0, #1                   \n"
      "  msr control, r0                  \n"
      "  isb                              \n"

      // Invoke fn(ctx) unprivileged. MPU-violating accesses fault here.
      "  mov r0, r3                       \n"
      "  blx r2                           \n"

      // Re-enter privileged mode. The handler accepts this exact callsite only
      // while this pbl_user_call() call is active for the thread.
      "  .global __arch_user_call_svc \n"
      "__arch_user_call_svc:       \n"
      "  svc 2                            \n"
      // Deliberate fail-closed trap. If the SVC was denied or not rewritten, do
      // not keep executing in the callback's control flow.
      "  udf #0                           \n");
}

PBL_EXTERNALLY_VISIBLE PBL_NAKED PBL_USED void arch_user_call_resume(void) {
  __asm volatile(
      // r0 is the state pointer stamped into the frame by Handler mode. Restore
      // SP before any stack use.
      "  ldr r12, [r0, #%c[caller_lr_off]] \n"
      "  ldr r1, [r0, #%c[entry_sp_off]]  \n"
      "  mov sp, r1                       \n"
      "  adds r0, r0, #%c[regs_off]       \n"
      "  ldmia r0, {r4-r11}               \n"
      "  bx r12                           \n"
      :
      : [caller_lr_off] "i"(offsetof(struct arch_user_call, caller_lr)),
        [entry_sp_off] "i"(offsetof(struct arch_user_call, entry_sp)),
        [regs_off] "i"(offsetof(struct arch_user_call, saved_r4_r11)));
}

// ---- syscall entry and exit -------------------------------------------------

// Task SP/PSPLIM to restore if the finishing syscall ran on the thread's syscall
// stack, packed as (psplim << 32 | sp) to return in r0:r1; 0 = no switch needed.
PBL_USED uint64_t arch_syscall_stack_restore(void) {
  const struct pbl_thread *t = pbl_cur;
  const uintptr_t psp = __get_PSP();
  const uintptr_t base = (uintptr_t)t->syscall_stack;
  if (t->syscall_stack != nullptr && psp > base && psp <= base + t->syscall_stack_size) {
    const uint32_t sp = (uint32_t)t->backend.arch.syscall_sp;
    const uint32_t psplim = (uint32_t)t->stack;
    return ((uint64_t)psplim << 32) | sp;
  }
  return 0;
}

// Drop privilege and return to the caller. If the syscall ran on the syscall
// stack, restore PSP/PSPLIM to the thread stack first (while still privileged).
// Preserves r0 and r1, the syscall's return value.
PBL_EXTERNALLY_VISIBLE void PBL_NAKED PBL_USED arch_syscall_exit(void) {
  __asm volatile(
      " push {r0, r1} \n" // save syscall return value
      " bl pbl_kernel_syscall_exit \n"
      " bl arch_syscall_return_address \n" // r0 = real return address
      " push {r0, r1} \n"                  // save real LR (r1 = pad; keeps 8-byte align)
      " bl arch_syscall_stack_restore \n"  // r0 = restore SP (0 = none), r1 = restore PSPLIM
      " mov r2, r0 \n"                     // r2 = restore SP
      " mov r3, r1 \n"                     // r3 = restore PSPLIM
      " pop {r0, r1} \n"                   // r0 = real LR
      " mov r12, r0 \n"                    // r12 = real LR (caller-saved; no bl follows)
      " pop {r0, r1} \n"                   // r0,r1 = syscall return value
      " cbz r2, 1f \n"                     // skip stack switch if not relocated
      " msr psp, r2 \n" // back to the thread stack (higher addr; safe vs low limit)
#ifdef CONFIG_ARMV8_M_MAINLINE
      " msr psplim, r3 \n" // restore the thread stack limit
#endif
      " isb \n"
      "1: \n"
      " mrs r2, control \n" // drop privilege: CONTROL.nPRIV = 1
      " orr r2, r2, #1 \n"
      " msr control, r2 \n"
      " isb \n"
      " bx r12 \n" // return to the caller
  );
}

// Just jump straight into the exit code
PBL_EXTERNALLY_VISIBLE void PBL_NAKED PBL_USED arch_syscall_exit_nested(void) {
  __asm volatile("b arch_syscall_exit\n");
}

// Switch to @p sp, limited by @p psplim, drop privilege and branch to @p fn.
static PBL_NAKED PBL_USED void prv_syscall_abort(void (*fn)(void), uint32_t sp, uint32_t psplim) {
  __asm volatile(
#ifdef CONFIG_ARMV8_M_MAINLINE
      " movs r3, #0 \n"
      " msr psplim, r3 \n" // lift the limit, sp may be above or below the current one
      " mov sp, r1 \n"
      " msr psplim, r2 \n"
#else
      " mov sp, r1 \n"
#endif
      " isb \n"
      " mrs r3, control \n" // drop privilege: CONTROL.nPRIV = 1
      " orr r3, r3, #1 \n"
      " msr control, r3 \n"
      " isb \n"
      " movs r1, #0 \n"
      " mov lr, r1 \n" // fn must not return
      " bx r0 \n");
}

void pbl_syscall_abort_to(void (*fn)(void)) {
  PBL_ASSERTN(!arch_is_user_context());

  pbl_kernel_syscall_exit();

  const struct pbl_thread *t = pbl_cur;
  prv_syscall_abort(fn, (uint32_t)t->backend.arch.syscall_sp & ~7u, (uint32_t)t->stack);
  __builtin_unreachable();
}

// This function needs to preserve the argument registers and stack exactly as they were on
// entry, so the arguments are passed correctly into the syscall. The key purpose of this
// function is to determine whether or not the caller is privileged. If the caller is
// unprivileged, this function returns normally to the syscall stub, and svc 2 is
// called elevating privileges. If the caller was already privileged, this function
// returns past the svc 2 instruction so privileges are not elevated.
PBL_EXTERNALLY_VISIBLE void PBL_NAKED PBL_USED arch_syscall_enter(void) {
  __asm volatile(
      // Save argument registers
      " push {r0-r3, lr} \n"
      " bl arch_syscall_from_kernel \n"
      " cmp r0, #1 \n" // Were we privileged?

      " pop {r0-r3, lr} \n" // Restore state

      " it eq \n"        // If we were privileged, return past the svc function
      " addeq lr, #2 \n" // svc 2 is 2 bytes long

      // Store our return address in ip, which isn't caller or callee saved
      // since the linker can modify it
      " mov ip, lr \n"

      // Set lr to the stub's return address. This saves code space so the
      // stub doesn't have to do this itself. Also we need to check this value
      // here.
      " pop {lr} \n"

      " push {ip} \n" // Save the stub address on the stack

      // The following can occur with nested syscalls, when the 2nd syscall is at
      // the end of the first. Since ARCH_SYSCALL_FROM_USER depends on the return
      // address of the function being equal to arch_syscall_exit, changing to
      // the nested variant prevents a false positive in the nested syscall

      // if lr == arch_syscall_exit, lr = arch_syscall_exit_nested
      " ldr ip, =arch_syscall_exit \n"
      " cmp lr, ip \n"
      " it eq \n"
      " ldreq lr, =arch_syscall_exit_nested \n"

      " pop {pc} \n" // Return to the stub
  );
}

// This is more space efficient than inlining the equality
// expression into every syscall since the address literal
// only needs to be stored at the end of this one function
bool arch_syscall_returns_to_user(void *ret_addr) {
  return ret_addr == &arch_syscall_exit;
}

bool arch_syscall_raise_allowed(uint32_t caller_pc) {
  // All syscall stubs are lumped together in one place in the image
  // to reduce the attack surface. Don't allow privilege to be raised by any
  // code outside of that region, even if that code is in flash.
  extern const uint32_t __syscall_text_start__[];
  extern const uint32_t __syscall_text_end__[];
  return caller_pc >= (uint32_t)__syscall_text_start__ &&
         caller_pc < (uint32_t)__syscall_text_end__;
}

void arch_syscall_entered(uintptr_t orig_sp, uint32_t *lr_slot) {
  // The scheduler cannot switch threads until this SVC returns, so the current
  // thread is the caller.
  struct pbl_arch_thread *arch = &pbl_cur->backend.arch;
  arch->syscall_lr = *lr_slot;
  arch->syscall_sp = orig_sp;

  // Return from the syscall through the code that drops privilege again.
  *lr_slot = (uint32_t)&arch_syscall_exit;
}
