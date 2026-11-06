/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/drivers/nvmem.h>

#include <clar.h>

#define MEM_SIZE 16U

static uint8_t s_mem[MEM_SIZE];
static int s_read_calls;
static int s_read_ret;

static int prv_reg_read(const struct pbl_nvmem *nvmem, unsigned int offset, void *val,
                        size_t bytes) {
  s_read_calls++;
  if (s_read_ret != 0) {
    return s_read_ret;
  }

  memcpy(val, &s_mem[offset], bytes);

  return 0;
}

static const struct pbl_nvmem_ops s_ops = {
  .reg_read = prv_reg_read,
};

PBL_DEVICE_STATE_DEFINE(s_nvmem);
static const struct pbl_nvmem s_nvmem =
    PBL_NVMEM_INIT(s_nvmem, "mem", nullptr, nullptr, &s_ops, MEM_SIZE, 4U, 4U);

PBL_DEVICE_STATE_DEFINE(s_nvmem_not_init);
static const struct pbl_nvmem s_nvmem_not_init =
    PBL_NVMEM_INIT(s_nvmem_not_init, "absent", nullptr, nullptr, &s_ops, MEM_SIZE, 4U, 4U);

static const struct pbl_nvmem_cell s_cell = PBL_NVMEM_CELL(&s_nvmem, 8U, 4U);
static const struct pbl_nvmem_cell s_cell_oob = PBL_NVMEM_CELL(&s_nvmem, 12U, 8U);

void test_nvmem__initialize(void) {
  for (size_t i = 0U; i < MEM_SIZE; i++) {
    s_mem[i] = (uint8_t)i;
  }
  s_read_calls = 0;
  s_read_ret = 0;
  cl_assert_equal_i(pbl_device_init(&s_nvmem.dev), 0);
}

void test_nvmem__device_read(void) {
  uint8_t buf[MEM_SIZE];

  cl_assert_equal_i(pbl_nvmem_device_read(&s_nvmem, 4U, 8U, buf), 0);
  cl_assert_equal_i(buf[0], 4);
  cl_assert_equal_i(buf[7], 11);
}

void test_nvmem__device_read_whole(void) {
  uint8_t buf[MEM_SIZE];

  cl_assert_equal_i(pbl_nvmem_device_read(&s_nvmem, 0U, MEM_SIZE, buf), 0);
  cl_assert_equal_i(buf[MEM_SIZE - 1U], MEM_SIZE - 1U);
}

void test_nvmem__device_read_rejects_bad_geometry(void) {
  uint8_t buf[MEM_SIZE];

  cl_assert_equal_i(pbl_nvmem_device_read(&s_nvmem, 2U, 4U, buf), -EINVAL);
  cl_assert_equal_i(pbl_nvmem_device_read(&s_nvmem, 0U, 3U, buf), -EINVAL);
  cl_assert_equal_i(pbl_nvmem_device_read(&s_nvmem, 0U, 0U, buf), -EINVAL);
  cl_assert_equal_i(pbl_nvmem_device_read(&s_nvmem, 12U, 8U, buf), -EINVAL);
  cl_assert_equal_i(pbl_nvmem_device_read(&s_nvmem, MEM_SIZE, 4U, buf), -EINVAL);
  cl_assert_equal_i(s_read_calls, 0);
}

void test_nvmem__device_read_not_ready(void) {
  uint8_t buf[4];

  cl_assert_equal_i(pbl_nvmem_device_read(&s_nvmem_not_init, 0U, 4U, buf), -ENODEV);
  cl_assert_equal_i(s_read_calls, 0);
}

void test_nvmem__provider_error(void) {
  uint8_t buf[4];

  s_read_ret = -ETIMEDOUT;
  cl_assert_equal_i(pbl_nvmem_device_read(&s_nvmem, 0U, 4U, buf), -ETIMEDOUT);
  cl_assert_equal_i(pbl_nvmem_cell_read(&s_cell, buf, sizeof(buf)), -ETIMEDOUT);
}

void test_nvmem__cell_read(void) {
  uint8_t buf[8];

  cl_assert_equal_i(pbl_nvmem_cell_read(&s_cell, buf, sizeof(buf)), 0);
  cl_assert_equal_i(buf[0], 8);
  cl_assert_equal_i(buf[3], 11);
}

void test_nvmem__cell_read_small_buffer(void) {
  uint8_t buf[3];

  cl_assert_equal_i(pbl_nvmem_cell_read(&s_cell, buf, sizeof(buf)), -EINVAL);
  cl_assert_equal_i(s_read_calls, 0);
}

void test_nvmem__cell_read_out_of_bounds(void) {
  uint8_t buf[8];

  cl_assert_equal_i(pbl_nvmem_cell_read(&s_cell_oob, buf, sizeof(buf)), -EINVAL);
}
