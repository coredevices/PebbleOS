/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "fake_resource_syscalls.h"

#define PATH_STRING_LENGTH 512

#define MAX_OPEN_FILES 512

static FILE* resource_files[MAX_OPEN_FILES] = {NULL};
static const uint32_t resource_start_index = 1; // must start at 1 so font resources work
static uint32_t resource_index = resource_start_index;

ResAppNum sys_get_current_resource_num(void) {
  return 0;
}

uint32_t sys_resource_load_file_as_resource(const char *filepath, const char *filename) {
  uint32_t resource_id = UINT32_MAX;
  char full_path[PATH_STRING_LENGTH];
  if (filepath) {
    snprintf(full_path, sizeof(full_path), "%s/%s", filepath, filename);
  } else {
    snprintf(full_path, sizeof(full_path), "%s", filename);
  }
  FILE* resource_file = fopen(full_path, "r");
  if (resource_file) {
    resource_files[resource_index] = resource_file;
    resource_id = resource_index;
    resource_index++;  // Increment to next slot
  }
  return resource_id;
}

size_t sys_resource_size(ResAppNum app_num, uint32_t handle) {
  if (handle < UINT32_MAX) {
    FILE* resource_file = resource_files[handle];
    fseek(resource_file, 0, SEEK_END);
    size_t resource_size = ftell(resource_file);
    fseek(resource_file, 0, SEEK_SET);
    return resource_size;
  }
  return 0;
}

size_t sys_resource_load_range(ResAppNum app_num, uint32_t id, uint32_t start_bytes, 
                               uint8_t *buffer, size_t num_bytes) {
  if (buffer && id < UINT32_MAX) {
    FILE* resource_file = resource_files[id];
    fseek(resource_file, start_bytes, SEEK_SET);
    return fread(buffer, 1, num_bytes, resource_file); 
  }
  return 0;
}

bool sys_resource_bytes_are_readonly(void *bytes) {
  return false;
}

const uint8_t *sys_resource_read_only_bytes(ResAppNum app_num, uint32_t resource_id,
                                            size_t *num_bytes_out) {
  return NULL;
}

uint32_t sys_resource_get_and_cache(ResAppNum app_num, uint32_t resource_id) {
  return resource_id;
}

bool sys_resource_is_valid(ResAppNum app_num, uint32_t resource_id) {
  return true;
}

void fake_resource_syscalls_cleanup(void) {
  for (int i = resource_start_index; i <= resource_index; i++) {
    FILE *resource_file = resource_files[i];
    if (resource_file) {
      fclose(resource_file);
      resource_files[i] = NULL;
    }
  }

  resource_index = resource_start_index;
}
