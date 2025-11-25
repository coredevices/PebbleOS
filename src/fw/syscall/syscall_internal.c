/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "syscall_internal.h"

#include "applib/app_logging.h"
#include "kernel/memory_layout.h"
#include "mcu/privilege.h"
#include "process_management/process_loader.h"
#include "process_management/process_manager.h"
#include "syscall/syscall.h"
#include "system/logging.h"
#include "system/passert.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdint.h>
#include <inttypes.h>

// Indices into FreeRTOS thread local storage
#define TLS_SYSCALL_LR_IDX 0
#define TLS_SYSCALL_SP_IDX 1

// Helper functions to access FreeRTOS TLS
static uintptr_t prv_get_syscall_sp(void) {
  return (uintptr_t)pvTaskGetThreadLocalStoragePointer(NULL, TLS_SYSCALL_SP_IDX);
}

static void prv_set_syscall_sp(uintptr_t new_sp) {
  vTaskSetThreadLocalStoragePointer(NULL, TLS_SYSCALL_SP_IDX, (void *)new_sp);
}

USED uintptr_t get_syscall_lr(void) {
  return (uintptr_t)pvTaskGetThreadLocalStoragePointer(NULL, TLS_SYSCALL_LR_IDX);
}

static void prv_set_syscall_lr(uintptr_t new_lr) {
  vTaskSetThreadLocalStoragePointer(NULL, TLS_SYSCALL_LR_IDX, (void *)new_lr);
}

NORETURN syscall_failed(void) {
  register uint32_t lr __asm("lr");
  uint32_t saved_lr = lr;

  PBL_ASSERT(mcu_state_is_privileged(), "Insufficient Privileges!");

  PBL_LOG(LOG_LEVEL_WARNING, "Bad syscall!");

  sys_app_fault(saved_lr);

  // sys_die is no return, but it's a syscall so I don't want to mark it with that attribute
  while(1) { }
}

void syscall_assert_userspace_buffer(const void* buf, size_t num_bytes) {
  PebbleTask task = pebble_task_get_current();

  void *user_stack_end = (void *)prv_get_syscall_sp();

  if (process_manager_is_address_in_region(task, buf, user_stack_end)
      && process_manager_is_address_in_region(
          task, (uint8_t *)buf + num_bytes -1, user_stack_end)) {
    return;
  }

  APP_LOG(APP_LOG_LEVEL_ERROR, "syscall failure! %p..%p is not in app space.", buf, (char *)buf + num_bytes);
  PBL_LOG(LOG_LEVEL_ERROR, "syscall failure! %p..%p is not in app space.", buf, (char *)buf + num_bytes);
  syscall_failed();
}

// Drop privileges and return to the address stored in thread local storage
// Has to preserve r0 and r1 so the syscall's return value is passed through
EXTERNALLY_VISIBLE void NAKED_FUNC USED prv_drop_privilege(void) {
  __asm volatile (
    " push {r0, r1} \n"
    " bl process_manager_handle_syscall_exit \n"
    " bl get_syscall_lr \n"
    " push { r0 } \n" // push the correct lr onto the stack

    " mov r0, #0 \n" // mcu_state_set_thread_privilege(false)
    " bl mcu_state_set_thread_privilege \n"

    " pop {lr} \n" // Pop correct return address
    " pop {r0, r1} \n" // Restore the return values of the syscall

    " bx lr \n" // Leave the syscall
  );
}

// Just jump straight into the drop privilege code
EXTERNALLY_VISIBLE void NAKED_FUNC USED prv_drop_privilege_wrapper(void) {
  __asm volatile("b prv_drop_privilege\n");
}

// This function needs to preserve the argument registers and stack exactly as they were on
// entry, so the arguments are passed correctly into the syscall. The key purpose of this
// function is to determine whether or not the caller is privileged. If the caller is
// unprivileged, this function returns normally to the syscall wrapper, and svc 2 is
// called elevating privileges. If the caller was already privileged, this function
// returns past the svc 2 instruction so privileges are not elevated.
void NAKED_FUNC USED syscall_internal_maybe_skip_privilege(void) {
  __asm volatile (
    // Save argument registers
    " push {r0-r3, lr} \n"
    " bl mcu_state_is_privileged \n"
    " cmp r0, #1 \n" // Were we privileged?

    " pop {r0-r3, lr} \n" // Restore state

    " it eq \n" // If we were privileged, return past the svc function
    " addeq lr, #2 \n" // svc 2 is 2 bytes long

    // Store our return address in ip, which isn't caller or callee saved
    // since the linker can modify it
    " mov ip, lr \n"

    // Set lr to the wrapper's return address. This saves code space so the
    // wrapper doesn't have to do this itself. Also we need to check this value
    // here.
    " pop {lr} \n"

    " push {ip} \n" // Save the wrapper address on the stack

    // The following can occur with nested syscalls, when the 2nd syscall is at
    // the end of the first. Since PRIVILEGE_WAS_ELEVATED depends on the return
    // address of the function being equal to syscall_internal_drop_privilege,
    // changing to the wrapper prevents a false positive in the nested syscall

    // if lr == syscall_internal_drop_privilege,
    // lr = syscall_internal_drop_privilege_wrapper
    " ldr ip, =prv_drop_privilege \n"
    " cmp lr, ip \n"
    " it eq \n"
    " ldreq lr, =prv_drop_privilege_wrapper \n"

    " pop {pc} \n" // Return to the wrapper
  );
}

// This is more space efficient than inlining the equality
// expression into every syscall since the address literal
// only needs to be stored at the end of this one function
bool syscall_internal_check_return_address(void * ret_addr) {
  return ret_addr == &prv_drop_privilege;
}

// This function is called by the SVC handler with the pre-syscall
// stack pointer, and a pointer to the saved LR on the stack.
// It then stores the SP and LR in thread local storage,
// and updates the saved LR to point at the drop privilege code.
void vSetupSyscallRegisters(uintptr_t orig_sp, uintptr_t *lr_ptr) {
  // These calls should be safe because the scheduler needs to call the svc handler
  // before the current task is changed. Since this function is called from the svc
  // handler, modifying the current task will always finish before a context switch

  // Save the correct return address so the drop privilege code knows where to
  // return to.
  prv_set_syscall_lr(*lr_ptr);

  // Save the value of the SP before entry into the syscall so
  // syscall_assert_userspace_buffer can ensure that a user provided buffer doesn't
  // point into the syscall's stack frame, and that the syscall has enough space
  prv_set_syscall_sp(orig_sp);

  // Set the return address of the syscall to be the drop privilege code
  *lr_ptr = (uintptr_t)&prv_drop_privilege;
}
