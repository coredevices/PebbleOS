/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>

#include <applib/persist.h>

PBL_WEAK bool persist_exists(const uint32_t key) {
  return 1;
}

PBL_WEAK int persist_get_size(const uint32_t key) {
  return 1;
}

PBL_WEAK bool persist_read_bool(const uint32_t key) {
  return 1;
}

PBL_WEAK int32_t persist_read_int(const uint32_t key) {
  return 1;
}

PBL_WEAK int persist_read_data(const uint32_t key, void *buffer, const size_t buffer_size) {
  return 1;
}

PBL_WEAK int persist_read_data__deprecated(const uint32_t key, const size_t buffer_size,
                                           void *buffer) {
  return 1;
}

PBL_WEAK int persist_read_string(const uint32_t key, char *buffer, const size_t buffer_size) {
  return 1;
}

PBL_WEAK int persist_read_string__deprecated(const uint32_t key, const size_t buffer_size,
                                             char *buffer) {
  return 1;
}

PBL_WEAK status_t persist_write_bool(const uint32_t key, const bool value) {
  return 1;
}

PBL_WEAK status_t persist_write_int(const uint32_t key, const int32_t value) {
  return 1;
}

PBL_WEAK int persist_write_data(const uint32_t key, const void *buffer, const size_t buffer_size) {
  return 1;
}

PBL_WEAK int persist_write_data__deprecated(const uint32_t key, const size_t buffer_size,
                                            const void *buffer) {
  return 1;
}

PBL_WEAK int persist_write_string(const uint32_t key, const char *cstring) {
  return 1;
}

PBL_WEAK status_t persist_delete(const uint32_t key) {
  return 1;
}

PBL_WEAK void persist_service_client_open(const Uuid *uuid) {
}

PBL_WEAK void persist_service_client_close(const Uuid *uuid) {
}

PBL_WEAK status_t persist_service_delete_file(const Uuid *uuid) {
  return S_SUCCESS;
}
