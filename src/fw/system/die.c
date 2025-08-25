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

#include "drivers/vibe.h"
#include "kernel/core_dump.h"
#include "kernel/logging_private.h"
#include "kernel/pulse_logging.h"
#include "system/bootbits.h"
#include "system/passert.h"
#include "system/reboot_reason.h"
#include "system/reset.h"

#define CMSIS_COMPATIBLE
#include <mcu.h>

#if defined(NO_WATCHDOG)
#include "FreeRTOS.h"
#include "debug/setup.h"
#endif

NORETURN reset_due_to_software_failure(void) {
#if PULSE_EVERYWHERE
  pulse_logging_log_buffer_flush();
#endif

#if defined(NO_WATCHDOG)
  // Don't reset right away, leave it in a state we can inspect

  enable_mcu_debugging();
  __disable_irq();
  while (1) {
    continue;
  }
#endif

  PBL_LOG_FROM_FAULT_HANDLER("Resetting!");
  boot_bit_set(BOOT_BIT_SOFTWARE_FAILURE_OCCURRED);
  system_reset();
}
