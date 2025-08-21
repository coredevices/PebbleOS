/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "kernel/logging_private.h"
#include "process_management/process_manager.h"
#include "process_management/app_manager.h"
#include "process_management/worker_manager.h"

#include "applib/app_logging.h"
#include "applib/app_heap_analytics.h"
#include "kernel/memory_layout.h"
#include "mcu/privilege.h"
#include "services/common/analytics/analytics_event.h"
#include "services/common/system_task.h"
#include "syscall/syscall.h"
#include "syscall/syscall_internal.h"
#include "system/logging.h"
#include "syscall/syscall.h"

#include <util/heap.h>

#define CMSIS_COMPATIBLE
#include <mcu.h>

#include "FreeRTOS.h"
#include "portmacro.h"
#include "task.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

// These variables are assigned values when the watch faults
// TODO PBL-PBL-36253: We should probably save these in CrashInfo in the future. Saved here because
// they are easier to pull out through GDB and because we can keep the Bluetooth FW and Normal FW
// fault handling the way they are now.
static uint32_t s_fault_saved_sp;
static uint32_t s_fault_saved_lr;
static uint32_t s_fault_saved_pc;

void enable_fault_handlers(void) {
  NVIC_SetPriority(MemoryManagement_IRQn, configMAX_SYSCALL_INTERRUPT_PRIORITY);
  NVIC_SetPriority(BusFault_IRQn, configMAX_SYSCALL_INTERRUPT_PRIORITY);
  NVIC_SetPriority(UsageFault_IRQn, configMAX_SYSCALL_INTERRUPT_PRIORITY);

  SCB->SHCSR |= SCB_SHCSR_MEMFAULTENA_Msk;
  SCB->SHCSR |= SCB_SHCSR_BUSFAULTENA_Msk;
  SCB->SHCSR |= SCB_SHCSR_USGFAULTENA_Msk;
}


typedef struct CrashInfo {
  PebbleTask task;
  Uuid app_uuid;
  uint8_t build_id[BUILD_ID_EXPECTED_LEN];
  uintptr_t lr;
  uintptr_t pc;
  bool lr_known;
  bool pc_known;
  bool is_rocky_app;
} CrashInfo;

CrashInfo make_crash_info_pc(uintptr_t pc) {
  return (CrashInfo) { .pc = pc, .pc_known = true };
}

CrashInfo make_crash_info_pc_lr(uintptr_t pc, uintptr_t lr) {
  return (CrashInfo) { .pc = pc, .pc_known = true,
                       .lr = lr, .lr_known = true };
}

static void prv_save_debug_registers(unsigned int* stacked_args) {
  s_fault_saved_lr = (uint32_t)stacked_args[5];
  s_fault_saved_pc = (uint32_t)stacked_args[6];
  s_fault_saved_sp = (uint32_t)&stacked_args[8];
}

static void prv_log_app_lr_and_pc_system_task(void *data) {
  CrashInfo* crash_info = (CrashInfo*) data;

  char lr_str[16];
  if (crash_info->lr_known) {
    sniprintf(lr_str, sizeof(lr_str), "%p", (void*) crash_info->lr);
  } else {
    strncpy(lr_str, "???", sizeof(lr_str));
  }

  char pc_str[16];
  if (crash_info->pc_known) {
    sniprintf(pc_str, sizeof(pc_str), "%p", (void*) crash_info->pc);
  } else {
    strncpy(pc_str, "???", sizeof(pc_str));
  }

  char buffer[UUID_STRING_BUFFER_LENGTH];
  uuid_to_string(&crash_info->app_uuid, buffer);

  char *process_string = (crash_info->task == PebbleTask_Worker) ? "Worker" : "App";

  APP_LOG(APP_LOG_LEVEL_ERROR, "%s fault! %s PC: %s LR: %s", process_string, buffer, pc_str, lr_str);

  PBL_LOG(LOG_LEVEL_ERROR, "%s fault! %s", process_string, buffer);
  PBL_LOG(LOG_LEVEL_ERROR, " --> PC: %s LR: %s", pc_str, lr_str);

  analytics_event_app_crash(&crash_info->app_uuid,
                            (crash_info->pc_known) ? crash_info->pc : 0,
                            (crash_info->lr_known) ? crash_info->lr : 0,
                            crash_info->build_id, crash_info->is_rocky_app);
}

//! Converts an address from an absolute address in our memory space to one that's relative to the start
//! of the loaded app/worker
static void convert_to_process_offset(bool known, uintptr_t* pc, PebbleTask task) {
  if (known) {
    *pc = (uintptr_t) process_manager_address_to_offset(task, (void*) *pc);
  }
}

static CrashInfo s_current_app_crash_info;

static void setup_log_app_crash_info(CrashInfo crash_info) {
  // Write the information out into a global variable so it can be logged out at a less critical time.
  s_current_app_crash_info = crash_info;

  const PebbleProcessMd *md = sys_process_manager_get_current_process_md();
  s_current_app_crash_info.app_uuid = md->uuid;
  s_current_app_crash_info.is_rocky_app = md->is_rocky_app;

  const uint8_t *build_id = process_metadata_get_build_id(md);
  if (build_id) {
    memcpy(s_current_app_crash_info.build_id, build_id, sizeof(s_current_app_crash_info.build_id));
  }

  PebbleTask task = pebble_task_get_current();
  s_current_app_crash_info.task = task;
  convert_to_process_offset(s_current_app_crash_info.pc_known, &s_current_app_crash_info.pc, task);
  convert_to_process_offset(s_current_app_crash_info.lr_known, &s_current_app_crash_info.lr, task);
}

static NORETURN kernel_fault(RebootReasonCode reason_code, uint32_t lr) {
  RebootReason reason = { .code = reason_code, .extra = { .value = lr } };
  reboot_reason_set(&reason);
  reset_due_to_software_failure();
}

// TODO: Can we tell if it was the worker and not the app?
extern void sys_app_fault(uint32_t lr);

NORETURN trigger_fault(RebootReasonCode reason_code, uint32_t lr) {
  if (mcu_state_is_privileged()) {
    kernel_fault(reason_code, lr);
  } else {
    sys_app_fault(lr);
  }
}

NORETURN trigger_oom_fault(size_t bytes, uint32_t lr, Heap *heap_ptr) {
  if (mcu_state_is_privileged()) {
      RebootReason reason = {
        .code =  RebootReasonCode_OutOfMemory,
        .heap_data = {
          .heap_alloc_lr = lr,
          .heap_ptr = (uint32_t)heap_ptr,
        }
      };
      reboot_reason_set(&reason);
      reset_due_to_software_failure();
  } else {
    app_heap_analytics_log_native_heap_oom_fault(bytes, heap_ptr);
    sys_app_fault(lr);
  }
}

void NOINLINE app_crashed(void) {
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
    PBL_LOG_FROM_FAULT_HANDLER("WTF?");
    kernel_fault(RebootReasonCode_HardFault, stashed_lr);
  }

  process_manager_put_kill_process_event(task, false /* gracefully */);

  // Wait for the kernel to kill us...
  vTaskSuspend(xTaskGetCurrentTaskHandle());
}


DEFINE_SYSCALL(NORETURN, sys_app_fault, uint32_t stashed_lr) {
  // This is the privileged side of handling a failed assert/croak from unprivileged code.
  // Always run on the current task.

  CrashInfo crash_info = make_crash_info_pc(stashed_lr);
  setup_log_app_crash_info(crash_info);
  system_task_add_callback(prv_log_app_lr_and_pc_system_task, &s_current_app_crash_info);

  prv_kill_user_process(stashed_lr);
  for (;;) {} // Not Reached
}

static void hardware_fault_landing_zone(void) {
  prv_log_app_lr_and_pc_system_task(&s_current_app_crash_info);

  prv_kill_user_process(0);
}

static void prv_return_to_landing_zone(uintptr_t stacked_pc, uintptr_t stacked_lr, unsigned int* stacked_args) {
  // We got this! Let's redirect this task to a spin function and tell the app manager to kill us.

  // Log about the terrible thing that just happened.
  CrashInfo crash_info = make_crash_info_pc_lr(stacked_pc, stacked_lr);
  setup_log_app_crash_info(crash_info);

  // Alright, now to neuter the current task. We're going to do some work to make it so when we return from
  // this fault handler we'll end up in a perfectly safe place while we wait to die.

  SCB->BFAR &= 1 << 7; // Clear Bus Fault Address Register "address is valid" bit
  SCB->MMFAR &= 1 << 7; // Clear Memory Manage Address Register "address is valid" bit
  SCB->CFSR &= ~0; // Clear the complete status register

  // Redirect this task to nowhere by changing the stacked PC register.
  // We can't let this task resume to where it crashed or else it will just crash again.
  // The kernel should come by and kill the task soon, but if it's busy doing something else just spin.
  // We don't want to just spin in the fault handler because that will prevent other tasks from being
  // executed, as we're currently in a higher priority interrupt.
  stacked_args[6] = (int) hardware_fault_landing_zone;

  // Clear the ICI bits in the Program Status Register. These bits refer to microprocessor state if we
  // get interrupted during a certain set of instructions. Since we're returning to a different place, we need
  // to clean up this state or else we'll just hit an INVSTATE UsageFault immediately. The only bit we leave
  // set is the bit that says we're in thumb state, which must always be set on Cortex-M3, since the micro doesn't
  // even support non-thumb instructions.
  // See: https://pebbletech.campfirenow.com/room/508662/transcript/message/1111369053#message_1111369053
  //      http://stackoverflow.com/a/9538628/1546
  stacked_args[7] = 1 << 24;

  mcu_state_set_thread_privilege(true);

  // Now return to hardware_fault_landing_zone...
}

static void attempt_handle_stack_overflow(unsigned int* stacked_args) {
  PebbleTask task = pebble_task_get_current();
  PBL_LOG_SYNC(LOG_LEVEL_ERROR, "Stack overflow [task: %s]", pebble_task_get_name(task));

  if (mcu_state_is_thread_privileged()) {
    // We're hosed! We can't recover so just reboot everything.
    RebootReason reason = { .code = RebootReasonCode_StackOverflow, .data8[0] = task };
    reboot_reason_set(&reason);
    reset_due_to_software_failure();
    return;
  }

  // We got this! Let's redirect this task to a spin function and tell the app manager to kill us.
  prv_return_to_landing_zone(0, 0, stacked_args);   // We can't get LR or PC, so just set to 0's.
}


static void attempt_handle_generic_fault(unsigned int* stacked_args) {
  uintptr_t stacked_lr = (uintptr_t) stacked_args[5];;
  uintptr_t stacked_pc = (uintptr_t) stacked_args[6];;;

  if (mcu_state_is_thread_privileged()) {
    // We're hosed! We can't recover so just reboot everything.
    kernel_fault(RebootReasonCode_HardFault, stacked_lr);
    return;
  }

  // We got this! Let's redirect this task to a spin function and tell the app manager to kill us.
  prv_return_to_landing_zone(stacked_pc, stacked_lr, stacked_args);   // We can't get LR or PC, so just set to 0's.
}


// Hardware Fault Handlers
///////////////////////////////////////////////////////////
extern void fault_handler_dump(char buffer[80], unsigned int* stacked_args);
extern void fault_handler_dump_cfsr(char buffer[80]);

static void mem_manage_handler_c(unsigned int* stacked_args, unsigned int lr) {
  // Be very careful about touching stacked_args in this function. We can end up in the
  // memfault handler because we hit the stack guard, which indicates that we've run out of stack
  // space and therefore won't have any room to stack the args. Accessing stacked_args in this
  // case will end up triggering a hardfault.

  PBL_LOG_FROM_FAULT_HANDLER("\r\n\r\n[Memory Management Failure!]");

  char buffer[80];
  PBL_LOG_FROM_FAULT_HANDLER("Configured Regions: ");
  memory_layout_dump_mpu_regions_to_dbgserial();
  PBL_LOG_FROM_FAULT_HANDLER("");

  // If if we faulted in a stack guard region, this indicates a stack overflow
  bool stack_overflow = false;
  const uint32_t cfsr = SCB->CFSR;
  const uint8_t mmfsr = cfsr & 0xff;
  if (mmfsr & (1 << 7)) {
    uint32_t fault_addr = SCB->MMFAR;
    MpuRegion mpu_region = mpu_get_region(MemoryRegion_IsrStackGuard);
    if (memory_layout_is_pointer_in_region(&mpu_region, (void *)fault_addr)) {
      stack_overflow = true;
    } else {
      mpu_region = mpu_get_region(MemoryRegion_TaskStackGuard);
      if (memory_layout_is_pointer_in_region(&mpu_region, (void *)fault_addr)) {
        stack_overflow = true;
      }
    }
  }

  // If it's a stack overflow, backup the stack so that attempt_handle_hardware_fault() can jam in
  // our landing zone to return to
  if (stack_overflow) {
    // Zero out the saved registers. We won't have new values in them, and we want to make sure
    // that they don't contain bogus values from a fault we previously handled without crashing.
    s_fault_saved_lr = 0;
    s_fault_saved_pc = 0;
    s_fault_saved_sp = 0;

    // We can't call fault_handler_dump because stacked_args isn't going to be valid, but we can at least dump
    // the cfsr.
    fault_handler_dump_cfsr(buffer);

    stacked_args += 256;     // Should be enough to get above the guard region and execute hardware_fault_landing_zone
    if (lr & 0x04) {
      __set_PSP((uint32_t)stacked_args);
    } else {
      __set_MSP((uint32_t)stacked_args);
    }
    attempt_handle_stack_overflow(stacked_args);

  } else {
    prv_save_debug_registers(stacked_args);

    fault_handler_dump(buffer, stacked_args);

    // BREAKPOINT;
    // NOTE: If you want to get a stack trace at this point. Set a breakpoint here (you can compile in the above
    // BREAKPOINT call if you want) and issue the following commands in gdb:
    //    set var $sp=<value of SP above>
    //    set var $lr=<value of LR above>
    //    set var $pc=<value of PC above>
    //    bt
    attempt_handle_generic_fault(stacked_args);
  }
}

void MemManage_Handler(void) {
  // Grab the stack pointer, shove it into a register and call
  // the c function above.
  __asm("tst lr, #4\n"
        "ite eq\n"
        "mrseq r0, msp\n"
        "mrsne r0, psp\n"
        "mov r1, lr\n"
        "b %0\n" :: "i" (mem_manage_handler_c));
}

static void busfault_handler_c(unsigned int* stacked_args) {
  PBL_LOG_FROM_FAULT_HANDLER("\r\n\r\n[BusFault_Handler!]");
  prv_save_debug_registers(stacked_args);

  char buffer[80];
  fault_handler_dump(buffer, stacked_args);

  PBL_LOG_FROM_FAULT_HANDLER("");

  attempt_handle_generic_fault(stacked_args);
}

void BusFault_Handler(void) {
  __asm("tst lr, #4\n"
        "ite eq\n"
        "mrseq r0, msp\n"
        "mrsne r0, psp\n"
        "b %0\n" :: "i" (busfault_handler_c));
}

static void usagefault_handler_c(unsigned int* stacked_args) {
  PBL_LOG_FROM_FAULT_HANDLER("\r\n\r\n[UsageFault_Handler!]");
  prv_save_debug_registers(stacked_args);

  char buffer[80];
  fault_handler_dump(buffer, stacked_args);

  PBL_LOG_FROM_FAULT_HANDLER("");

  attempt_handle_generic_fault(stacked_args);
}

void UsageFault_Handler(void) {
  __asm("tst lr, #4\n"
        "ite eq\n"
        "mrseq r0, msp\n"
        "mrsne r0, psp\n"
        "b %0\n" :: "i" (usagefault_handler_c));
}

