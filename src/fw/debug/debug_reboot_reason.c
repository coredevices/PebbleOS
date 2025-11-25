/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "debug.h"
#include "advanced_logging.h"

#include <stdbool.h>
#include <stdint.h>

#include "comm/ble/gatt_service_changed.h"
#include "drivers/pmic.h"
#include "kernel/core_dump.h"
#include "kernel/event_loop.h"
#include "kernel/pbl_malloc.h"
#include "popups/crashed_ui.h"
#include "services/common/analytics/analytics.h"
#include "system/logging.h"
#include "system/reboot_reason.h"

static void log_reboot_reason_cb(void *reason) {
  AnalyticsEventCrash *crash_report = (AnalyticsEventCrash *)reason;
  analytics_event_crash(crash_report->crash_code, crash_report->link_register);
  kernel_free(crash_report);
}

static RebootReasonCode s_last_reboot_reason_code = RebootReasonCode_Unknown;
RebootReasonCode reboot_reason_get_last_reboot_reason(void) {
  return s_last_reboot_reason_code;
}

void debug_reboot_reason_print(McuRebootReason mcu_reboot_reason) {
  RebootReason reason;
  reboot_reason_get(&reason);
  bool show_reset_alert = !reason.restarted_safely;
  s_last_reboot_reason_code = reason.code;

  // We're out of flash space, scrape a few bytes back!
  static const char* rebooted_due_to = " rebooted due to ";

  const char* restarted_safely_string = "Safely";
  if (!reason.restarted_safely) {
    restarted_safely_string = "Dangerously";
  }

  // Keep hourly logging to keep track of hours without crashes.
  analytics_set(ANALYTICS_DEVICE_METRIC_SYSTEM_CRASH_CODE,
                0xDEAD0000 | reason.code, AnalyticsClient_System);
  uint32_t lr = reason.extra.value;

  // Leave this NULL to do your own printing.
  const char *reason_string = NULL;
  switch (reason.code) {
  // Normal stuff
  case RebootReasonCode_Unknown:
    reason_string = "We don't know why we %s rebooted.";
    lr = mcu_reboot_reason.reset_mask;
    break;
  case RebootReasonCode_LowBattery:
    reason_string = "%s%sLowBattery";
    break;
  case RebootReasonCode_SoftwareUpdate:
    gatt_service_changed_server_handle_fw_update();
    reason_string = "%s%sSoftwareUpdate";
    break;
  case RebootReasonCode_ResetButtonsHeld:
    // Since we forced the reset, it isn't unexpected
    show_reset_alert = false;
    reason_string = "%s%sResetButtonsHeld";
    break;
  case RebootReasonCode_ShutdownMenuItem:
    reason_string = "%s%sLowBattery";
    break;
  case RebootReasonCode_FactoryResetReset:
    reason_string = "%s%sFactoryResetReset";
    break;
  case RebootReasonCode_FactoryResetShutdown:
    reason_string = "%s%sFactoryResetShutdown";
    break;
  case RebootReasonCode_MfgShutdown:
    reason_string = "%s%sMfgShutdown";
    break;
  case RebootReasonCode_Serial:
    reason_string = "%s%sSerial";
    break;
  case RebootReasonCode_RemoteReset:
    reason_string = "%s%sa Remote Reset";
    break;
  case RebootReasonCode_ForcedCoreDump:
    reason_string = "%s%sa Forced Coredump";
    break;
  case RebootReasonCode_PrfIdle:
    reason_string = "%s%sIdle PRF";
    break;
  // Error occurred
  case RebootReasonCode_Assert:
    show_reset_alert = true;
    reason_string = "%s%sAssert: LR %#"PRIxPTR;
    break;
  case RebootReasonCode_HardFault:
    show_reset_alert = true;
    reason_string = "%s%sHardFault: LR %#"PRIxPTR;
    break;
  case RebootReasonCode_LauncherPanic:
    show_reset_alert = true;
    reason_string = "%s%sLauncherPanic: code 0x%"PRIx32;
    break;
  case RebootReasonCode_ClockFailure:
    reason_string = "%s%sClock Failure";
    break;
  case RebootReasonCode_WorkerHardFault:
    show_reset_alert = true;
    reason_string = "%s%sWorker HardFault";
    break;
  case RebootReasonCode_OutOfMemory:
    show_reset_alert = true;
    reason_string = "%s%sOOM";
    break;
  case RebootReasonCode_BtCoredump:
    show_reset_alert = true;
    reason_string = "%s%sBT Coredump";
    break;
  default:
    reason_string = "%s%sUnrecognized Reason";
    break;
  // Error occurred
  case RebootReasonCode_Watchdog:
    show_reset_alert = true;
    DEBUG_LOG(LOG_LEVEL_INFO, "%s%sWatchdog: Bits 0x%" PRIx8 ", Mask 0x%" PRIx8,
              restarted_safely_string, rebooted_due_to, reason.data8[0], reason.data8[1]);

    if (reason.watchdog.stuck_task_pc != 0) {
      DEBUG_LOG(LOG_LEVEL_INFO, "Stuck task PC: 0x%" PRIx32 ", LR: 0x%" PRIx32,
                reason.watchdog.stuck_task_pc, reason.watchdog.stuck_task_lr);

      if (reason.watchdog.stuck_task_callback) {
        DEBUG_LOG(LOG_LEVEL_INFO, "Stuck callback: 0x%" PRIx32,
                  reason.watchdog.stuck_task_callback);
      }
    }
    break;
  case RebootReasonCode_StackOverflow:
    show_reset_alert = true;
    PebbleTask task = (PebbleTask) reason.data8[0];
    DEBUG_LOG(LOG_LEVEL_INFO, "%s%sStackOverflow: Task #%d (%s)", restarted_safely_string,
              rebooted_due_to, task, pebble_task_get_name(task));
    break;
  case RebootReasonCode_EventQueueFull:
    show_reset_alert = true;
    DEBUG_LOG(LOG_LEVEL_INFO, "%s%sEvent Queue Full", restarted_safely_string, rebooted_due_to);
    DEBUG_LOG(LOG_LEVEL_INFO, "LR: 0x%"PRIx32" Current: 0x%"PRIx32" Dropped: 0x%"PRIx32,
              reason.event_queue.push_lr,
              reason.event_queue.current_event,
              reason.event_queue.dropped_event);
    break;
  }
  // Generic reason string
  if (reason_string) {
    DEBUG_LOG(LOG_LEVEL_INFO, reason_string, restarted_safely_string, rebooted_due_to,
              reason.extra.value);
  }

  analytics_set(ANALYTICS_DEVICE_METRIC_SYSTEM_CRASH_LR, lr, AnalyticsClient_System);

  // We need to wait for the logging service to initialize.
  AnalyticsEventCrash *crash_report = kernel_malloc_check(sizeof(AnalyticsEventCrash));
  *crash_report = (AnalyticsEventCrash) {
    .crash_code = reason.code,
    .link_register = lr
  };
  launcher_task_add_callback(log_reboot_reason_cb, crash_report);

  if (is_unread_coredump_available()) {
    DEBUG_LOG(LOG_LEVEL_INFO, "Unread coredump file is present!");
  }

  DEBUG_LOG(LOG_LEVEL_INFO, "MCU reset reason mask: 0x%x", (int)mcu_reboot_reason.reset_mask);
#if CAPABILITY_HAS_PMIC
  uint32_t pmic_reset_reason = pmic_get_last_reset_reason();
  if (pmic_reset_reason != 0) {
    DEBUG_LOG(LOG_LEVEL_INFO, "PMIC reset reason mask: 0x%x", (int)pmic_reset_reason);
  }
#endif

  // Core dumps always get an alert display, since the user asked for it.
  if (reason.code == RebootReasonCode_ForcedCoreDump) {
    crashed_ui_show_forced_core_dump();
  }

#ifdef SHOW_PEBBLE_JUST_RESET_ALERT
  // Trigger an alert display so that the user knows the watch rebooted due to a crash. This event
  // will be caught and handled by the launcher.c event loop.
  if (show_reset_alert && reason.code != RebootReasonCode_ForcedCoreDump) {
    crashed_ui_show_pebble_reset();
  }
#endif

  reboot_reason_clear();
}
