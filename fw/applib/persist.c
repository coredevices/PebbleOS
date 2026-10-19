/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "persist.h"

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/syscall.h>
#include <pbl/services/persist.h>
#include <pbl/services/settings/settings_file.h>
#include <pbl/util/math.h>

#include <process_management/process_manager.h>
#include <syscall/syscall.h>

static_assert(PERSIST_DATA_MAX_LENGTH <= SETTINGS_VAL_MAX_LEN,
              "PERSIST_DATA_MAX_LENGTH is larger than the max length that "
              "settings_file supports.");

static SettingsFile *prv_lock_and_get_store(void) {
  return persist_service_lock_and_get_store(&sys_process_manager_get_current_process_md()->uuid);
}

static void prv_unlock(SettingsFile **store) {
  persist_service_unlock_store(*store);
}

#define LOCK_AND_GET_STORE(name) \
  SettingsFile *name PBL_CLEANUP(prv_unlock) = prv_lock_and_get_store()

PBL_SYSCALL(size_t, persist_get_max_size) {
  return persist_service_get_max_size();
}

PBL_SYSCALL(bool, persist_exists, (VAL, const uint32_t, key)) {
  LOCK_AND_GET_STORE(store);
  return settings_file_exists(store, &key, sizeof(key));
}

PBL_SYSCALL(int, persist_get_size, (VAL, const uint32_t, key)) {
  LOCK_AND_GET_STORE(store);
  int result = settings_file_get_len(store, &key, sizeof(key));
  return result ?: E_DOES_NOT_EXIST;
}

PBL_SYSCALL(bool, persist_read_bool, (VAL, const uint32_t, key)) {
  bool value = false;
  LOCK_AND_GET_STORE(store);
  settings_file_get(store, &key, sizeof(key), &value, sizeof(value));
  return value;
}

PBL_SYSCALL(int32_t, persist_read_int, (VAL, const uint32_t, key)) {
  int32_t value = 0;
  LOCK_AND_GET_STORE(store);
  settings_file_get(store, &key, sizeof(key), &value, sizeof(value));
  return value;
}

PBL_SYSCALL(int, persist_read_data, (VAL, const uint32_t, key),
            (OUT_BUF, void *, buffer, buffer_size), (VAL, const size_t, buffer_size)) {
  LOCK_AND_GET_STORE(store);
  const int len = settings_file_get_len(store, &key, sizeof(key));
  if (len == 0) {
    return E_DOES_NOT_EXIST;
  } else if (FAILED(len)) {
    RETURN_STATUS_UP(len);
  }

  const size_t restricted_size = MIN(buffer_size, (size_t)len);
  const status_t read_result = settings_file_get(store, &key, sizeof(key), buffer, restricted_size);
  if (FAILED(read_result)) {
    RETURN_STATUS_UP(read_result);
  }
  return restricted_size;
}

// Legacy version to prevent previous app breakage, __deprecated preserves order
int persist_read_data__deprecated(const uint32_t key, const size_t buffer_size, void *buffer) {
  return persist_read_data(key, buffer, buffer_size);
}

int persist_read_string(const uint32_t key, char *buffer, const size_t buffer_size) {
  const int read_result = persist_read_data(key, buffer, buffer_size);
  if (PASSED(read_result)) {
    buffer[read_result - 1] = '\0';
  }
  return read_result;
}

// Legacy version to prevent previous app breakage, __deprecated preserves order
int persist_read_string__deprecated(const uint32_t key, const size_t buffer_size, char *buffer) {
  return persist_read_string(key, buffer, buffer_size);
}

PBL_SYSCALL(status_t, persist_write_bool, (VAL, const uint32_t, key), (VAL, const bool, value)) {
  LOCK_AND_GET_STORE(store);
  status_t result = settings_file_set(store, &key, sizeof(key), &value, sizeof(value));
  return PASSED(result) ? (status_t)sizeof(value) : result;
}

PBL_SYSCALL(status_t, persist_write_int, (VAL, const uint32_t, key), (VAL, const int32_t, value)) {
  LOCK_AND_GET_STORE(store);
  status_t result = settings_file_set(store, &key, sizeof(key), &value, sizeof(value));
  return PASSED(result) ? (status_t)sizeof(value) : result;
}

PBL_SYSCALL(int, persist_write_data, (VAL, const uint32_t, key),
            (IN_BUF, const void *, buffer, buffer_size), (VAL, const size_t, buffer_size)) {
  const size_t restricted_size = MIN(buffer_size, PERSIST_DATA_MAX_LENGTH);
  LOCK_AND_GET_STORE(store);
  int result = settings_file_set(store, &key, sizeof(key), buffer, restricted_size);
  return PASSED(result) ? (int)restricted_size : result;
}

// Legacy version to prevent previous app breakage, __deprecated preserves order
int persist_write_data__deprecated(const uint32_t key, const size_t buffer_size,
                                   const void *buffer) {
  return persist_write_data(key, buffer, buffer_size);
}

PBL_SYSCALL(int, persist_write_string, (VAL, const uint32_t, key), (STR, const char *, cstring)) {
  const size_t cstring_length = strlen(cstring) + 1;
  return persist_write_data__impl(from_user, key, cstring, cstring_length);
}

PBL_SYSCALL(status_t, persist_delete, (VAL, const uint32_t, key)) {
  LOCK_AND_GET_STORE(store);
  status_t result;
  if (settings_file_exists(store, &key, sizeof(key))) {
    result = settings_file_delete(store, &key, sizeof(key));
    if (PASSED(result)) {
      result = S_TRUE;
    }
  } else {
    result = E_DOES_NOT_EXIST;
  }
  return result;
}
