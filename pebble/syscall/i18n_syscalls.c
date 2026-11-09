/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/i18n/i18n.h>
#include <pbl/kernel/mpu.h>
#include <pbl/kernel/syscall.h>
#include <pbl/logging/logging.h>

#include <kernel/pebble_tasks.h>
#include <process_management/process_regions.h>

PBL_SYSCALL(void, sys_i18n_get_locale, (OUT_BUF, char *, buf, PBL_I18N_LOCALE_LEN)) {
  if (from_user && pebble_task_get_current() == PebbleTask_Worker) {
    // Not allowed from workers
    pbl_syscall_fail();
  }
  strncpy(buf, pbl_i18n_get_locale(), PBL_I18N_LOCALE_LEN);
}

PBL_SYSCALL(void, sys_i18n_get_with_buffer, (VAL, const char *, string),
            (OUT_BUF, char *, buffer, length), (VAL, size_t, length)) {
  if (from_user && pebble_task_get_current() == PebbleTask_Worker) {
    // Not allowed from workers
    pbl_syscall_fail();
  }
  if (from_user) {
    if (!pbl_mpu_region_contains_cstring(process_region_app(), string, 100) &&
        !pbl_mpu_region_contains_cstring(pbl_mpu_region_firmware(), string, 100)) {
      PBL_LOG_ERR("Pointer %p not in app or microflash region", string);
      pbl_syscall_fail();
    }
  }

  pbl_i18n_get_with_buffer(string, buffer, length);
}

PBL_SYSCALL(size_t, sys_i18n_get_length, (VAL, const char *, string)) {
  if (from_user && pebble_task_get_current() == PebbleTask_Worker) {
    // Not allowed from workers
    pbl_syscall_fail();
  }
  if (from_user) {
    if (!pbl_mpu_region_contains_cstring(process_region_app(), string, 100) &&
        !pbl_mpu_region_contains_cstring(pbl_mpu_region_firmware(), string, 100)) {
      PBL_LOG_ERR("Pointer %p not in app or microflash region", string);
      pbl_syscall_fail();
    }
  }

  return pbl_i18n_get_length(string);
}
