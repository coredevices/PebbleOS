/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "system/rtc_registers.h"
#include "util/attributes.h"

// NOTE: We include the reboot reason in analytics and the tools we use to analyze the analytics are
// dependent on the position and ordering of these enumerated values. To keep the analysis tools
// simpler, it is best to keep these enums in the same order and add new ones to the end.
typedef enum {
  RebootReasonCode_Unknown = 0,
  // Normal stuff
  RebootReasonCode_LowBattery,
  RebootReasonCode_SoftwareUpdate,
  RebootReasonCode_ResetButtonsHeld,
  RebootReasonCode_ShutdownMenuItem,
  RebootReasonCode_FactoryResetReset,
  RebootReasonCode_FactoryResetShutdown,
  RebootReasonCode_MfgShutdown,
  RebootReasonCode_Serial,
  RebootReasonCode_RemoteReset,
  RebootReasonCode_PrfReset,
  RebootReasonCode_ForcedCoreDump,
  RebootReasonCode_PrfIdle,
  RebootReasonCode_PrfResetButtonsHeld,

  // Error occurred
  RebootReasonCode_Watchdog = 16,
  RebootReasonCode_Assert,
  RebootReasonCode_StackOverflow,
  RebootReasonCode_HardFault,
  RebootReasonCode_LauncherPanic,
  RebootReasonCode_ClockFailure, // Not used on 3.x
  RebootReasonCode_AppHardFault, // Not used on 3.x
  RebootReasonCode_EventQueueFull,
  RebootReasonCode_WorkerHardFault, // Off by default, compile in with WORKER_CRASH_CAUSES_RESET
  RebootReasonCode_OutOfMemory,
  RebootReasonCode_DialogBootFault,
  RebootReasonCode_BtCoredump,
  RebootReasonCode_CoreDump,  // Core dump initiated without a more specific reason set
  RebootReasonCode_CoreDumpEntryFailed,
} RebootReasonCode;

typedef struct PACKED {
  RebootReasonCode code:8;
  bool restarted_safely:1;
  uint8_t padding:7;
  union {
    uint16_t data16;
    uint8_t data8[2];
  };
  union {
    struct {
      uint32_t value;
    } extra;
    struct {
      uint32_t stuck_task_pc;
      uint32_t stuck_task_lr;
      uint32_t stuck_task_callback;
    } watchdog; //!< Valid if code == RebootReasonCode_Watchdog
    struct {
      uint32_t push_lr;
      uint32_t current_event;
      uint32_t dropped_event;
    } event_queue; //!< Valid if code == RebootReasonCode_EventQueueFull
    struct {
      uint32_t heap_alloc_lr;
      uint32_t heap_ptr;
    } heap_data; //!< Valid if code == RebootReasonCode_OutOfMemory
  };
} RebootReason;

void reboot_reason_set(RebootReason *reason);

void reboot_reason_set_restarted_safely(void);

void reboot_reason_get(RebootReason *reason);

void reboot_reason_clear(void);

uint32_t reboot_get_slot_of_last_launched_app(void);

void reboot_set_slot_of_last_launched_app(uint32_t app_slot);

RebootReasonCode reboot_reason_get_last_reboot_reason(void);
