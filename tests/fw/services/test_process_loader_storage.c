/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <clar.h>

#include <kernel/util/segment.h>
#include <pbl/services/filesystem/app_file.h>
#include <pbl/services/filesystem/pfs.h>
#include <pbl/services/process_management/app_storage.h>
#include <process_management/pebble_process_info.h>
#include <process_management/pebble_process_md.h>
#include <process_management/process_loader.h>
#include <resource/resource.h>
#include <resource/resource_storage.h>
#include <pbl/util/math.h>
#include <pbl/crc/crc.h>

#include <inttypes.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include <stubs_logging.h>
#include <stubs_passert.h>

// An image over 64 KiB, laid out the way the SDK lays one out: the header, then code and data,
// then the relocation table past load_size.
#define WIDE_LOAD_SIZE    0x12c00
#define WIDE_VIRTUAL_SIZE 0x18000
#define WIDE_ENTRY        0x84
#define WIDE_JUMP_TABLE   0x88
#define WIDE_RELOC_SLOT   0x10010
#define WIDE_RELOC_VALUE  0x17f00

#define BIG_TIME_FIXTURE "app_registry/big_time"

// Larger than any image here, so the tests reach the loader's own checks
#define RAM_SIZE (160 * 1024)

const void *const g_pbl_system_tbl[] = {NULL};

static uint8_t s_image[RAM_SIZE];
static size_t s_image_len;
static size_t s_read_offset;
static char s_opened_name[APP_FILENAME_MAX_LENGTH];
// The loader splits the segment on max_align_t boundaries, so the RAM starts on one
static _Alignas(max_align_t) uint8_t s_ram[RAM_SIZE];

// Fakes
////////////////////////////////////

int pfs_open(const char *name, uint8_t op_flags, uint8_t file_type, size_t start_size) {
  s_read_offset = 0;
  snprintf(s_opened_name, sizeof(s_opened_name), "%s", name);
  return 1;
}

int pfs_read(int fd, void *buf, size_t size) {
  const size_t len = MIN(size, s_image_len - s_read_offset);
  memcpy(buf, &s_image[s_read_offset], len);
  s_read_offset += len;
  return (int)len;
}

status_t pfs_close(int fd) {
  return S_SUCCESS;
}

status_t pfs_remove(const char *name) {
  return S_SUCCESS;
}

void app_file_name_make(char *restrict buffer, size_t buffer_len, AppInstallId app_id,
                        const char *restrict suffix, size_t suffix_len) {
  snprintf(buffer, buffer_len, "@%08" PRIx32 "/%s", (uint32_t)app_id, suffix);
}

void resource_storage_clear(ResAppNum app_num) {
}

bool resource_storage_check(ResAppNum app_num, uint32_t resource_id,
                            const ResourceVersion *expected_version) {
  return true;
}

size_t resource_load_byte_range_system(ResAppNum app_num, uint32_t resource_id,
                                       uint32_t start_offset, uint8_t *data, size_t num_bytes) {
  if (start_offset > s_image_len) {
    return 0;
  }
  const size_t len = MIN(num_bytes, s_image_len - start_offset);
  memcpy(data, &s_image[start_offset], len);
  return len;
}

// Helpers
////////////////////////////////////

static PebbleProcessInfo *prv_header(void) {
  return (PebbleProcessInfo *)s_image;
}

static void prv_set_crc(void) {
  PebbleProcessInfo *info = prv_header();
  const uint32_t load_size = process_info_get_load_size(info);
  info->crc = pbl_crc32_legacy(&s_image[PROCESS_INFO_CRC_START_OFFSET],
                               load_size - PROCESS_INFO_CRC_START_OFFSET);
}

static void prv_build_wide_image(void) {
  memset(s_image, 0, sizeof(s_image));
  PebbleProcessInfo *info = prv_header();
  *info = (PebbleProcessInfo){
    .header = "PBLAPP",
    .struct_version = {0x10, 0x01},
    .sdk_version = {PROCESS_INFO_CURRENT_SDK_VERSION_MAJOR, PROCESS_INFO_CURRENT_SDK_VERSION_MINOR},
    .load_size = WIDE_LOAD_SIZE & 0xffff,
    .load_size_hi = WIDE_LOAD_SIZE >> 16,
    .offset = WIDE_ENTRY,
    .sym_table_addr = WIDE_JUMP_TABLE,
    .num_reloc_entries = 1,
    .virtual_size = WIDE_VIRTUAL_SIZE & 0xffff,
    .virtual_size_hi = WIDE_VIRTUAL_SIZE >> 16,
  };

  // The one relocated pointer sits past 64 KiB and points further past it
  const uint32_t app_relative_value = WIDE_RELOC_VALUE;
  memcpy(&s_image[WIDE_RELOC_SLOT], &app_relative_value, sizeof(app_relative_value));

  const uint32_t reloc_entry = WIDE_RELOC_SLOT;
  memcpy(&s_image[WIDE_LOAD_SIZE], &reloc_entry, sizeof(reloc_entry));
  s_image_len = WIDE_LOAD_SIZE + sizeof(reloc_entry);

  prv_set_crc();
}

static void prv_load_fixture(const char *name) {
  char path[256];
  snprintf(path, sizeof(path), "%s/%s", CLAR_FIXTURE_PATH, name);
  struct stat st;
  cl_assert(stat(path, &st) == 0);
  cl_assert((size_t)st.st_size <= sizeof(s_image));

  FILE *file = fopen(path, "rb");
  cl_assert(file);
  memset(s_image, 0, sizeof(s_image));
  s_image_len = fread(s_image, 1, st.st_size, file);
  fclose(file);
  cl_assert_equal_i(s_image_len, st.st_size);
}

static void *prv_load_from_resource(PebbleProcessMdResource *md, MemorySegment *segment) {
  process_metadata_init_with_resource_header(md, prv_header(), 1, PebbleTask_App);
  *segment = (MemorySegment){s_ram, s_ram + sizeof(s_ram)};
  return process_loader_load(&md->common, PebbleTask_App, segment);
}

static void *prv_load_from_flash(PebbleProcessMdFlash *md, MemorySegment *segment) {
  process_metadata_init_with_flash_header(md, prv_header(), 1, PebbleTask_App, NULL);
  *segment = (MemorySegment){s_ram, s_ram + sizeof(s_ram)};
  return process_loader_load(&md->common, PebbleTask_App, segment);
}

// Tests
////////////////////////////////////

void test_process_loader_storage__initialize(void) {
  memset(s_ram, 0, sizeof(s_ram));
  s_image_len = 0;
  s_read_offset = 0;
}

void test_process_loader_storage__cleanup(void) {
}

void test_process_loader_storage__app_info_accepts_an_image_over_64k(void) {
  prv_build_wide_image();

  PebbleProcessInfo info;
  const AppStorageGetAppInfoResult result =
      app_storage_get_process_info(&info, NULL, 1, PebbleTask_App);

  cl_assert_equal_i(result, GET_APP_INFO_SUCCESS);
  cl_assert_equal_i(process_info_get_virtual_size(&info), WIDE_VIRTUAL_SIZE);
}

void test_process_loader_storage__loads_an_image_over_64k(void) {
  prv_build_wide_image();

  PebbleProcessMdResource md;
  MemorySegment segment;
  void *entry = prv_load_from_resource(&md, &segment);

  cl_assert_equal_p(entry, (void *)((uintptr_t)&s_ram[WIDE_ENTRY] | 1));

  uint32_t relocated;
  memcpy(&relocated, &s_ram[WIDE_RELOC_SLOT], sizeof(relocated));
  cl_assert_equal_i(relocated, (uint32_t)(uintptr_t)&s_ram[WIDE_RELOC_VALUE]);

  // The relocation table sat over the start of .bss and has to be zero again
  uint32_t reloc_slot;
  memcpy(&reloc_slot, &s_ram[WIDE_LOAD_SIZE], sizeof(reloc_slot));
  cl_assert_equal_i(reloc_slot, 0);

  // The heap starts past the whole static size, not past its low 16 bits
  cl_assert_equal_p(segment.start, &s_ram[WIDE_VIRTUAL_SIZE]);
}

void test_process_loader_storage__loads_an_image_over_64k_from_flash(void) {
  prv_build_wide_image();

  PebbleProcessMdFlash md;
  MemorySegment segment;
  void *entry = prv_load_from_flash(&md, &segment);

  cl_assert_equal_p(entry, (void *)((uintptr_t)&s_ram[WIDE_ENTRY] | 1));
  cl_assert_equal_p(segment.start, &s_ram[WIDE_VIRTUAL_SIZE]);
}

void test_process_loader_storage__refuses_a_virtual_size_past_the_segment(void) {
  prv_build_wide_image();
  prv_header()->virtual_size_hi = 0x03;
  prv_set_crc();

  PebbleProcessMdResource md;
  MemorySegment segment;
  void *entry = prv_load_from_resource(&md, &segment);

  cl_assert_equal_p(entry, NULL);
}

void test_process_loader_storage__loads_an_existing_sdk_app(void) {
  // Built long before struct version 0x10.0x01. Its CRC starts at 0x82 and its code at 0x84.
  prv_load_fixture(BIG_TIME_FIXTURE);
  cl_assert_equal_i(prv_header()->struct_version.minor, 0x00);

  PebbleProcessMdResource md;
  MemorySegment segment;
  void *entry = prv_load_from_resource(&md, &segment);

  cl_assert(entry != NULL);
  // The heap starts on the first boundary at or past the static size
  const uint8_t *heap_start = segment.start;
  cl_assert(heap_start >= &s_ram[prv_header()->virtual_size]);
  cl_assert(heap_start < &s_ram[prv_header()->virtual_size + alignof(max_align_t)]);
}

void test_process_loader_storage__ignores_the_high_bytes_in_a_0x10_00_image(void) {
  // Only 0x10.0x01 headers carry sizes at 0x82 and 0x83. The CRC still covers the two bytes.
  prv_load_fixture(BIG_TIME_FIXTURE);
  prv_header()->load_size_hi = 0xaa;
  prv_header()->virtual_size_hi = 0x55;
  prv_set_crc();

  PebbleProcessMdResource md;
  MemorySegment segment;
  void *entry = prv_load_from_resource(&md, &segment);

  cl_assert(entry != NULL);
  cl_assert((uint8_t *)segment.start < &s_ram[0x10000]);
}

void test_process_loader_storage__loads_legacy_app_and_worker_from_flash(void) {
  prv_load_fixture(BIG_TIME_FIXTURE);
  const PebbleTask tasks[] = {PebbleTask_App, PebbleTask_Worker};
  for (size_t i = 0; i < sizeof(tasks) / sizeof(tasks[0]); ++i) {
    const PebbleTask task = tasks[i];
    PebbleProcessMdFlash md;
    process_metadata_init_with_flash_header(&md, prv_header(), 1, task, NULL);
    MemorySegment segment = {s_ram, s_ram + sizeof(s_ram)};
    cl_assert(process_loader_load(&md.common, task, &segment) != NULL);
    char expected_name[APP_FILENAME_MAX_LENGTH];
    app_storage_get_file_name(expected_name, sizeof(expected_name), 1, task);
    cl_assert_equal_s(s_opened_name, expected_name);
  }
}

void test_process_loader_storage__accepts_legacy_entry_at_0x82(void) {
  prv_load_fixture(BIG_TIME_FIXTURE);
  prv_header()->offset = 0x82;
  s_image[0x82] = 0x70; // bx lr
  s_image[0x83] = 0x47;
  prv_set_crc();
  PebbleProcessMdFlash md;
  MemorySegment segment;
  cl_assert(prv_load_from_flash(&md, &segment) != NULL);
}

void test_process_loader_storage__accepts_legacy_relocation_at_0x82(void) {
  prv_load_fixture(BIG_TIME_FIXTURE);
  const uint32_t value = 0x100;
  memcpy(&s_image[0x82], &value, sizeof(value));
  const uint32_t slot = 0x82;
  const uint32_t load_size = process_info_get_load_size(prv_header());
  memcpy(&s_image[load_size], &slot, sizeof(slot));
  prv_header()->num_reloc_entries = 1;
  s_image_len = load_size + sizeof(slot);
  prv_set_crc();
  PebbleProcessMdFlash md;
  MemorySegment segment;
  cl_assert(prv_load_from_flash(&md, &segment) != NULL);
  uint32_t relocated;
  memcpy(&relocated, &s_ram[0x82], sizeof(relocated));
  cl_assert_equal_i(relocated, (uint32_t)(uintptr_t)&s_ram[value]);
}

void test_process_loader_storage__refuses_entry_in_wide_header(void) {
  prv_build_wide_image();
  prv_header()->offset = 0x82;
  PebbleProcessMdFlash md;
  MemorySegment segment;
  cl_assert_equal_p(prv_load_from_flash(&md, &segment), NULL);
}

void test_process_loader_storage__refuses_relocation_in_wide_header(void) {
  prv_build_wide_image();
  const uint32_t slot = 0x82;
  memcpy(&s_image[WIDE_LOAD_SIZE], &slot, sizeof(slot));
  PebbleProcessMdFlash md;
  MemorySegment segment;
  cl_assert_equal_p(prv_load_from_flash(&md, &segment), NULL);
}

void test_process_loader_storage__loads_small_image_with_large_bss(void) {
  prv_load_fixture(BIG_TIME_FIXTURE);
  prv_header()->struct_version = (Version){0x10, 0x01};
  prv_header()->load_size_hi = 0;
  prv_header()->virtual_size = WIDE_VIRTUAL_SIZE & 0xffff;
  prv_header()->virtual_size_hi = WIDE_VIRTUAL_SIZE >> 16;
  prv_set_crc();
  PebbleProcessMdFlash md;
  MemorySegment segment;
  cl_assert(prv_load_from_flash(&md, &segment) != NULL);
  cl_assert_equal_p(segment.start, &s_ram[WIDE_VIRTUAL_SIZE]);
}

void test_process_loader_storage__reads_build_id_for_both_header_versions(void) {
  const uint8_t expected[BUILD_ID_EXPECTED_LEN] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
  };
  for (uint8_t minor = 0; minor <= 1; ++minor) {
    prv_build_wide_image();
    prv_header()->struct_version.minor = minor;
    ElfExternalNote *note = (ElfExternalNote *)&s_image[0x84];
    note->name_length = BUILD_ID_NAME_EXPECTED_LEN;
    note->data_length = BUILD_ID_EXPECTED_LEN;
    note->type = 3;
    memcpy(note->data, "GNU", BUILD_ID_NAME_EXPECTED_LEN);
    memcpy(note->data + BUILD_ID_NAME_EXPECTED_LEN, expected, sizeof(expected));
    uint8_t actual[BUILD_ID_EXPECTED_LEN];
    PebbleProcessInfo info;
    cl_assert_equal_i(app_storage_get_process_info(&info, actual, 1, PebbleTask_App),
                      GET_APP_INFO_SUCCESS);
    cl_assert_equal_i(memcmp(actual, expected, sizeof(expected)), 0);
  }
}
