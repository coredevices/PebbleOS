/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "process_management/app_install_manager.h"
#include "process_management/app_manager.h"
#include "shell/system_app_state_machine.h"

// Stubs
/////////////////////////////////////////////////////////////////////////
#include "stubs_app_install_manager.h"
#include "stubs_app_manager.h"
#include "stubs_watchface.h"

bool battery_monitor_critical_lockout(void) {
  return false;
}

bool low_power_is_active(void) {
  return false;
}

uint32_t launcher_panic_get_current_error(void ) {
  return 0;
}

bool recovery_first_use_is_complete(void) {
  return true;
}

#include "system/bootbits.h"
bool boot_bit_test(BootBitValue bit) {
  return false;
}

// Use this macro to define a PebbleProcessMd* getter function and an associated constant
// that it will return.
#define DEFINE_STUB_APP(FUNC_NAME, RESULT_VAL)                             \
  static const PebbleProcessMd* FUNC_NAME ## _result = (void*) RESULT_VAL; \
  const PebbleProcessMd* FUNC_NAME(void) {                                 \
    return FUNC_NAME ## _result;                                           \
  }

DEFINE_STUB_APP(battery_critical_get_app_info, 1)
DEFINE_STUB_APP(low_power_face_get_app_info, 2)
DEFINE_STUB_APP(panic_app_get_app_info, 3)
DEFINE_STUB_APP(recovery_first_use_app_get_app_info, 4)
DEFINE_STUB_APP(launcher_menu_app_get_app_info, 5)

// Tests
/////////////////////////////////////////////////////////////////////////
void test_normal_system_app_state_machine__simple(void) {
  const PebbleProcessMd *first_app = system_app_state_machine_system_start();
  cl_assert(first_app == launcher_menu_app_get_app_info_result);
}
