/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <pbl/loader/loader.h>

#include <clar.h>

#define HEADER_SIZE 0x10
#define FILE_SIZE   0x40
#define MEM_SIZE    0x80
#define DST_SIZE    0x100

static uint8_t s_src[0x200];
static size_t s_src_len;
static int s_read_error;
static uint8_t s_dst[DST_SIZE];
static struct pbl_loader_image s_img;

static int prv_read(void *ctx, size_t offset, void *buf, size_t len) {
  if (s_read_error) {
    return s_read_error;
  }
  if ((offset > s_src_len) || (len > s_src_len - offset)) {
    return -EIO;
  }
  memcpy(buf, &s_src[offset], len);
  return 0;
}

static const struct pbl_loader_source s_source = {.read = prv_read};

static void prv_put32(uint8_t *p, uint32_t value) {
  memcpy(p, &value, sizeof(value));
}

static uint32_t prv_get32(const uint8_t *p) {
  uint32_t value;
  memcpy(&value, p, sizeof(value));
  return value;
}

static void prv_add_reloc(uint32_t target, uint32_t value) {
  prv_put32(&s_src[target], value);
  prv_put32(&s_src[s_img.reloc_offset + s_img.reloc_count * 4], target);
  s_img.reloc_count++;
  s_src_len = s_img.reloc_offset + s_img.reloc_count * 4;
}

static int prv_load(pbl_loader_address_t address, void *ctx) {
  int ret = pbl_loader_copy(&s_img, &s_source, s_dst, sizeof(s_dst));
  if (ret < 0) {
    return ret;
  }
  return pbl_loader_relocate(&s_img, &s_source, s_dst, sizeof(s_dst), address, ctx);
}

static uint32_t prv_offset_address(void *ctx, const void *addr) {
  return *(uint32_t *)ctx + (uint32_t)((const uint8_t *)addr - s_dst);
}

void test_loader__initialize(void) {
  for (size_t i = 0; i < sizeof(s_src); ++i) {
    s_src[i] = (uint8_t)i;
  }
  memset(s_dst, 0xa5, sizeof(s_dst));
  s_read_error = 0;
  s_img = (struct pbl_loader_image){
    .file_size = FILE_SIZE,
    .mem_size = MEM_SIZE,
    .header_size = HEADER_SIZE,
    .reloc_offset = FILE_SIZE,
  };
  s_src_len = FILE_SIZE;
}

void test_loader__copies_file_part_and_zero_fills(void) {
  cl_assert_equal_i(prv_load(nullptr, nullptr), 0);

  cl_assert_equal_i(memcmp(s_dst, s_src, FILE_SIZE), 0);
  for (size_t i = FILE_SIZE; i < MEM_SIZE; ++i) {
    cl_assert_equal_i(s_dst[i], 0);
  }
  cl_assert_equal_i(s_dst[MEM_SIZE], 0xa5);
}

void test_loader__relocates_to_destination(void) {
  prv_add_reloc(0x20, 0x30);
  prv_add_reloc(0x24, 0x70);

  cl_assert_equal_i(prv_load(nullptr, nullptr), 0);

  cl_assert_equal_i(prv_get32(&s_dst[0x20]), (uint32_t)(uintptr_t)&s_dst[0x30]);
  cl_assert_equal_i(prv_get32(&s_dst[0x24]), (uint32_t)(uintptr_t)&s_dst[0x70]);
}

void test_loader__relocates_through_translation(void) {
  uint32_t base = 0x20000000;
  prv_add_reloc(0x20, 0x30);

  cl_assert_equal_i(prv_load(prv_offset_address, &base), 0);

  cl_assert_equal_i(prv_get32(&s_dst[0x20]), base + 0x30);
}

void test_loader__clears_the_table_area(void) {
  prv_add_reloc(0x20, 0x30);

  cl_assert_equal_i(prv_load(nullptr, nullptr), 0);

  cl_assert_equal_i(prv_get32(&s_dst[FILE_SIZE]), 0);
}

void test_loader__clears_a_table_past_mem_size(void) {
  s_img.mem_size = FILE_SIZE;
  for (uint32_t i = 0; i < 8; ++i) {
    prv_add_reloc(0x20 + i * 4, 0x10);
  }

  cl_assert_equal_i(prv_load(nullptr, nullptr), 0);

  for (size_t i = FILE_SIZE; i < FILE_SIZE + 8 * 4; ++i) {
    cl_assert_equal_i(s_dst[i], 0);
  }
}

void test_loader__reads_table_from_any_offset(void) {
  s_img.reloc_offset = 0x180;
  prv_add_reloc(0x20, 0x30);

  cl_assert_equal_i(prv_load(nullptr, nullptr), 0);

  cl_assert_equal_i(prv_get32(&s_dst[0x20]), (uint32_t)(uintptr_t)&s_dst[0x30]);
}

void test_loader__relocates_unaligned_slots_and_table(void) {
  s_img.file_size = FILE_SIZE - 1;
  s_img.reloc_offset = FILE_SIZE - 1;
  prv_add_reloc(0x21, 0x30);

  cl_assert_equal_i(prv_load(nullptr, nullptr), 0);

  cl_assert_equal_i(prv_get32(&s_dst[0x21]), (uint32_t)(uintptr_t)&s_dst[0x30]);
}

void test_loader__accepts_one_past_the_end(void) {
  prv_add_reloc(0x20, MEM_SIZE);

  cl_assert_equal_i(prv_load(nullptr, nullptr), 0);

  cl_assert_equal_i(prv_get32(&s_dst[0x20]), (uint32_t)(uintptr_t)&s_dst[MEM_SIZE]);
}

void test_loader__accepts_a_target_right_past_the_header(void) {
  prv_add_reloc(HEADER_SIZE, 0x30);

  cl_assert_equal_i(prv_load(nullptr, nullptr), 0);
}

void test_loader__refuses_a_target_in_the_header(void) {
  prv_add_reloc(HEADER_SIZE - 1, 0x30);

  cl_assert_equal_i(prv_load(nullptr, nullptr), -EINVAL);
}

void test_loader__refuses_a_target_crossing_the_file_end(void) {
  prv_add_reloc(FILE_SIZE - 3, 0x30);

  cl_assert_equal_i(prv_load(nullptr, nullptr), -EINVAL);
}

void test_loader__refuses_a_target_past_the_file_end(void) {
  prv_add_reloc(0x20, 0x30);
  prv_put32(&s_src[s_img.reloc_offset], 0xfffffffe);

  cl_assert_equal_i(prv_load(nullptr, nullptr), -EINVAL);
}

void test_loader__refuses_a_value_past_mem_size(void) {
  prv_add_reloc(0x20, MEM_SIZE + 1);

  cl_assert_equal_i(prv_load(nullptr, nullptr), -EINVAL);
}

void test_loader__clears_the_table_after_a_bad_entry(void) {
  prv_add_reloc(0x20, 0x30);
  prv_add_reloc(0x04, 0x30);

  cl_assert_equal_i(prv_load(nullptr, nullptr), -EINVAL);

  cl_assert_equal_i(prv_get32(&s_dst[FILE_SIZE]), 0);
  cl_assert_equal_i(prv_get32(&s_dst[FILE_SIZE + 4]), 0);
}

void test_loader__refuses_an_image_larger_than_destination(void) {
  s_img.mem_size = DST_SIZE + 1;

  cl_assert_equal_i(pbl_loader_copy(&s_img, &s_source, s_dst, sizeof(s_dst)), -ENOMEM);
}

void test_loader__refuses_an_inconsistent_layout(void) {
  s_img.file_size = MEM_SIZE + 1;
  cl_assert_equal_i(pbl_loader_copy(&s_img, &s_source, s_dst, sizeof(s_dst)), -EINVAL);

  test_loader__initialize();
  s_img.header_size = FILE_SIZE + 1;
  cl_assert_equal_i(pbl_loader_copy(&s_img, &s_source, s_dst, sizeof(s_dst)), -EINVAL);
}

void test_loader__refuses_a_table_larger_than_destination(void) {
  s_img.reloc_count = (DST_SIZE - FILE_SIZE) / 4 + 1;

  cl_assert_equal_i(pbl_loader_relocate(&s_img, &s_source, s_dst, sizeof(s_dst), nullptr, nullptr),
                    -ENOMEM);
}

void test_loader__refuses_a_table_size_overflow(void) {
  s_img.reloc_count = SIZE_MAX / 2;

  cl_assert_equal_i(pbl_loader_relocate(&s_img, &s_source, s_dst, sizeof(s_dst), nullptr, nullptr),
                    -ENOMEM);
}

void test_loader__forwards_read_errors(void) {
  s_read_error = -EIO;
  cl_assert_equal_i(pbl_loader_copy(&s_img, &s_source, s_dst, sizeof(s_dst)), -EIO);

  s_read_error = 0;
  cl_assert_equal_i(pbl_loader_copy(&s_img, &s_source, s_dst, sizeof(s_dst)), 0);
  s_img.reloc_count = 1;
  s_read_error = -EIO;
  cl_assert_equal_i(pbl_loader_relocate(&s_img, &s_source, s_dst, sizeof(s_dst), nullptr, nullptr),
                    -EIO);
}
