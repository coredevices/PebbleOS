/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>
#include <stdio.h>

#include <pbl/kernel/fatal.h>
#include <pbl/kernel/heap.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mpu.h>
#include <pbl/kernel/sched.h>
#include <pbl/kernel/syscall.h>
#include <pbl/kernel/thread.h>
#include <pbl/kernel/work.h>
#include <pbl/logging/logging.h>
#include <pbl/mcu/interrupts.h>
#include <pbl/util/size.h>

#include <applib/app_logging.h>
#include <kernel/fault_handling.h>
#include <process_management/app_manager.h>
#include <process_management/process_manager.h>
#include <process_management/worker_manager.h>
#include <process_state/app_state/app_state.h>
#include <process_state/worker_state/worker_state.h>
#include <syscall/syscall.h>
#include <system/die.h>
#include <system/reboot_reason.h>

typedef struct CrashInfo {
  PebbleTask task;
  Uuid app_uuid;
  uint8_t build_id[BUILD_ID_EXPECTED_LEN];
  uintptr_t lr;
  uintptr_t pc;
  bool lr_known;
  bool pc_known;
} CrashInfo;

CrashInfo make_crash_info_pc(uintptr_t pc) {
  return (CrashInfo){.pc = pc, .pc_known = true};
}

static void prv_log_app_lr_and_pc_sys_work(void *data) {
  CrashInfo *crash_info = (CrashInfo *)data;

  char lr_str[16];
  if (crash_info->lr_known) {
    sniprintf(lr_str, sizeof(lr_str), "%p", (void *)crash_info->lr);
  } else {
    strncpy(lr_str, "???", sizeof(lr_str));
  }

  char pc_str[16];
  if (crash_info->pc_known) {
    sniprintf(pc_str, sizeof(pc_str), "%p", (void *)crash_info->pc);
  } else {
    strncpy(pc_str, "???", sizeof(pc_str));
  }

  char buffer[UUID_STRING_BUFFER_LENGTH];
  uuid_to_string(&crash_info->app_uuid, buffer);

  char *process_string = (crash_info->task == PebbleTask_Worker) ? "Worker" : "App";

  APP_LOG(APP_LOG_LEVEL_ERROR, "%s fault! %s PC: %s LR: %s", process_string, buffer, pc_str,
          lr_str);

  PBL_LOG_ERR("%s fault! %s", process_string, buffer);
  PBL_LOG_ERR(" --> PC: %s LR: %s", pc_str, lr_str);
}

//! Converts an address from an absolute address in our memory space to one that's relative to the
//! start of the loaded app/worker
static void convert_to_process_offset(bool known, uintptr_t *pc, PebbleTask task) {
  if (known) {
    *pc = (uintptr_t)process_manager_address_to_offset(task, (void *)*pc);
  }
}

static CrashInfo s_current_app_crash_info;

static void setup_log_app_crash_info(CrashInfo crash_info) {
  // Write the information out into a global variable so it can be logged out at a less critical
  // time.
  s_current_app_crash_info = crash_info;

  const PebbleProcessMd *md = sys_process_manager_get_current_process_md();
  s_current_app_crash_info.app_uuid = md->uuid;

  const uint8_t *build_id = process_metadata_get_build_id(md);
  if (build_id) {
    memcpy(s_current_app_crash_info.build_id, build_id, sizeof(s_current_app_crash_info.build_id));
  }

  PebbleTask task = pebble_task_get_current();
  s_current_app_crash_info.task = task;
  convert_to_process_offset(s_current_app_crash_info.pc_known, &s_current_app_crash_info.pc, task);
  convert_to_process_offset(s_current_app_crash_info.lr_known, &s_current_app_crash_info.lr, task);
}

[[noreturn]] static void kernel_fault(RebootReasonCode reason_code, uint32_t lr) {
  RebootReason reason = {.code = reason_code, .extra = {.value = lr}};
  reboot_reason_set(&reason);
  if (reason_code == RebootReasonCode_Assert) {
    prepare_for_software_failure();
    reset_with_core_dump(false /* is_forced */);
  } else {
    reset_due_to_software_failure();
  }
}

// TODO: Can we tell if it was the worker and not the app?
extern void sys_app_fault(uint32_t lr);

[[noreturn]] void trigger_fault(RebootReasonCode reason_code, uint32_t lr) {
  if (!pbl_is_user_context()) {
    kernel_fault(reason_code, lr);
  } else {
    sys_app_fault(lr);
  }
}

[[noreturn]] void pbl_fatal_error_handler(uintptr_t lr) {
  trigger_fault(RebootReasonCode_Assert, lr);
}

[[noreturn]] void pbl_heap_oom_handler(struct pbl_heap *heap_ptr, size_t bytes, uintptr_t lr) {
  // OOM on a process's own heap is the app's fault, not the kernel's: kill just
  // that process even when privileged (Moddable apps run privileged inside the
  // moddable_createMachine syscall). Only kernel-heap OOM reboots.
  PebbleTask task = pebble_task_get_current();
  bool process_heap_oom = (task == PebbleTask_App && heap_ptr == app_state_get_heap()) ||
                          (task == PebbleTask_Worker && heap_ptr == worker_state_get_heap());

  // sys_app_fault suspends this task for KernelMain to reap; only safe with the
  // scheduler running and outside an ISR, else reboot.
  bool can_kill_safely = !mcu_state_is_isr() && (pbl_kernel_is_running());

  if (pbl_is_user_context() || (process_heap_oom && can_kill_safely)) {
    sys_app_fault(lr);
  }

  // Kernel-heap OOM (or OOM on a kernel task): unrecoverable, reboot.
  RebootReason reason = {
    .code = RebootReasonCode_OutOfMemory,
    .heap_data = {
      .heap_alloc_lr = lr,
      .heap_ptr = (uint32_t)(uintptr_t)heap_ptr,
    }
  };
  reboot_reason_set(&reason);
  reset_due_to_software_failure();
}

void PBL_NOINLINE app_crashed(void) {
  // Just sit here and look pretty. The purpose of this function is to give app developers a symbol
  // that they can set a breakpoint on to debug app crashes. We need to make sure this function is
  // not going to get optimized away and that it's globally visible.
  __asm volatile("");
}

static void prv_kill_user_process(uint32_t stashed_lr) {
  PebbleTask task = pebble_task_get_current();
  if (task == PebbleTask_App) {
    app_crashed();
    app_manager_get_task_context()->safe_to_kill = true;
  } else if (task == PebbleTask_Worker) {
    app_crashed();
    worker_manager_get_task_context()->safe_to_kill = true;
    // If not release mode, generate a core dump so we can get debugging information
#ifdef WORKER_CRASH_CAUSES_RESET
    kernel_fault(RebootReasonCode_WorkerHardFault, stashed_lr);
#endif
  } else {
    PBL_LOG_FAULT("WTF?");
    kernel_fault(RebootReasonCode_HardFault, stashed_lr);
  }

  process_manager_put_kill_process_event(task, false /* gracefully */);

  // Wait for the kernel to kill us...
  pbl_thread_suspend(nullptr);
}

PBL_SYSCALL_NORETURN(sys_app_fault, (VAL, uint32_t, stashed_lr)) {
  // This is the privileged side of handling a failed assert/panic from unprivileged code.
  // Always run on the current task.

  CrashInfo crash_info = make_crash_info_pc(stashed_lr);
  setup_log_app_crash_info(crash_info);
  pbl_sys_work_call(prv_log_app_lr_and_pc_sys_work, &s_current_app_crash_info);

  prv_kill_user_process(stashed_lr);
  for (;;) {
  } // Not Reached
}

[[noreturn]] void pbl_kernel_syscall_fault(uintptr_t lr) {
  sys_app_fault(lr);
}

void pbl_fault_fatal_record(const struct pbl_fault_info *info) {
  RebootReason reason;
  if (info->reason == PBL_FAULT_STACK_OVERFLOW) {
    reason = (RebootReason){
      .code = RebootReasonCode_StackOverflow,
      .data8[0] = pebble_task_get_current(),
      .extra = {.value = info->pc ? info->pc : info->addr},
    };
  } else {
    // Prefer LR, PC is often madness on a fault; crash reports need a non-zero address.
    uintptr_t addr = info->lr ? info->lr : (info->pc ? info->pc : info->addr);
    reason = (RebootReason){.code = RebootReasonCode_HardFault, .extra = {.value = addr}};
  }
  reboot_reason_set(&reason);
}

[[noreturn]] void pbl_fault_fatal_handler(const struct pbl_fault_info *info) {
  reset_due_to_software_failure();
}

[[noreturn]] void pbl_fault_thread_handler(const struct pbl_fault_info *info) {
  CrashInfo crash_info = {
    .pc = info->pc,
    .pc_known = info->pc != 0,
    .lr = info->lr,
    .lr_known = info->lr != 0,
  };
  setup_log_app_crash_info(crash_info);
  prv_log_app_lr_and_pc_sys_work(&s_current_app_crash_info);

  // A Worker fault that resets reports the original address.
  uintptr_t lr = 0;
  if (s_current_app_crash_info.lr_known && s_current_app_crash_info.lr != 0) {
    lr = s_current_app_crash_info.lr;
  } else if (s_current_app_crash_info.pc_known && s_current_app_crash_info.pc != 0) {
    lr = s_current_app_crash_info.pc;
  }
  prv_kill_user_process(lr);
  for (;;) {
  }
}
