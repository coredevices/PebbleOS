/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/syscall.h>
#include <pbl/logging/logging.h>
#include <pbl/services/i18n/i18n.h>

#include <kernel/memory_layout.h>
#include <kernel/pebble_tasks.h>

PBL_LOG_MODULE_DECLARE(service_i18n, CONFIG_SERVICE_I18N_LOG_LEVEL);

PBL_SYSCALL(void, sys_i18n_get_locale, (OUT_BUF, char *, buf, ISO_LOCALE_LENGTH)) {
  if (from_user && pebble_task_get_current() == PebbleTask_Worker) {
    // Not allowed from workers
    pbl_syscall_fail();
  }
  strncpy(buf, i18n_get_locale(), ISO_LOCALE_LENGTH);
}

PBL_SYSCALL(void, sys_i18n_get_with_buffer, (VAL, const char *, string),
            (OUT_BUF, char *, buffer, length), (VAL, size_t, length)) {
  if (from_user && pebble_task_get_current() == PebbleTask_Worker) {
    // Not allowed from workers
    pbl_syscall_fail();
  }
  if (from_user) {
    if (!memory_layout_is_cstring_in_region(memory_layout_get_app_region(), string, 100) &&
        !memory_layout_is_cstring_in_region(memory_layout_get_microflash_region(), string, 100)) {
      PBL_LOG_ERR("Pointer %p not in app or microflash region", string);
      pbl_syscall_fail();
    }
  }

  i18n_get_with_buffer(string, buffer, length);
}

PBL_SYSCALL(size_t, sys_i18n_get_length, (VAL, const char *, string)) {
  if (from_user && pebble_task_get_current() == PebbleTask_Worker) {
    // Not allowed from workers
    pbl_syscall_fail();
  }
  if (from_user) {
    if (!memory_layout_is_cstring_in_region(memory_layout_get_app_region(), string, 100) &&
        !memory_layout_is_cstring_in_region(memory_layout_get_microflash_region(), string, 100)) {
      PBL_LOG_ERR("Pointer %p not in app or microflash region", string);
      pbl_syscall_fail();
    }
  }

  return i18n_get_length(string);
}
