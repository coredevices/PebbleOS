/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>

#include <clar_asserts.h>
#include <resource/resource.h>
#include <resource/resource_ids.auto.h>

#if CONFIG_SCREEN_COLOR_DEPTH_BITS == 8
#define EMOJI_SUFFIX "~color.pbf"
#else
#define EMOJI_SUFFIX ".pbf"
#endif

static FILE *prv_open_emoji(ResAppNum app_num, uint32_t id) {
  if (app_num != SYSTEM_APP) {
    return nullptr;
  }
  const char *path;
  switch (id) {
    case RESOURCE_ID_GOTHIC_14_EMOJI:
      path = FIRMWARE_EMOJI_DIR "/EMOJI_14" EMOJI_SUFFIX;
      break;
    case RESOURCE_ID_GOTHIC_18_EMOJI:
      path = FIRMWARE_EMOJI_DIR "/EMOJI_18" EMOJI_SUFFIX;
      break;
    case RESOURCE_ID_GOTHIC_24_EMOJI:
      path = FIRMWARE_EMOJI_DIR "/EMOJI_24" EMOJI_SUFFIX;
      break;
    case RESOURCE_ID_GOTHIC_28_EMOJI:
      path = FIRMWARE_EMOJI_DIR "/EMOJI_28" EMOJI_SUFFIX;
      break;
    default:
      return nullptr;
  }
  FILE *file = fopen(path, "rb");
  cl_assert_(file, path);
  return file;
}

size_t sys_resource_size(ResAppNum app_num, uint32_t id) {
  FILE *file = prv_open_emoji(app_num, id);
  if (!file) {
    return resource_size(app_num, id);
  }
  cl_assert_equal_i(fseek(file, 0, SEEK_END), 0);
  const long size = ftell(file);
  cl_assert(size >= 0);
  fclose(file);
  return size;
}

size_t sys_resource_load_range(ResAppNum app_num, uint32_t id, uint32_t offset, uint8_t *buffer,
                               size_t num_bytes) {
  FILE *file = prv_open_emoji(app_num, id);
  if (!file) {
    return resource_load_byte_range_system(app_num, id, offset, buffer, num_bytes);
  }
  cl_assert_equal_i(fseek(file, offset, SEEK_SET), 0);
  const size_t loaded = fread(buffer, 1, num_bytes, file);
  fclose(file);
  return loaded;
}
