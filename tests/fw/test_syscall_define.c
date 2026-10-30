/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <pbl/kernel/syscall.h>

#include <clar.h>
#include <fake_pebble_tasks.h>
#include <stubs_kernel_syscall.h>

// Fakes
///////////////////////////////////////////////////////////

static uint8_t s_user_mem[256];
static uint8_t s_kernel_mem[256];
static bool s_elevated;

static bool prv_in_user(const void *buf, size_t num_bytes) {
  const uintptr_t start = (uintptr_t)s_user_mem;
  const uintptr_t end = start + sizeof(s_user_mem);
  const uintptr_t p = (uintptr_t)buf;
  return p >= start && p <= end && num_bytes <= end - p;
}

bool arch_syscall_returns_to_user(void *ret_addr) {
  return s_elevated;
}

void pbl_syscall_verify_user(const void *buf, size_t num_bytes) {
  if (!prv_in_user(buf, num_bytes)) {
    pbl_syscall_fail();
  }
}

int pbl_user_copy_from(void *dst, const void *src, size_t num_bytes) {
  if (!prv_in_user(src, num_bytes)) {
    return -EFAULT;
  }
  memcpy(dst, src, num_bytes);
  return 0;
}

int pbl_user_copy_to(void *dst, const void *src, size_t num_bytes) {
  if (!prv_in_user(dst, num_bytes)) {
    return -EFAULT;
  }
  memcpy(dst, src, num_bytes);
  return 0;
}

int pbl_user_strnlen(const char *str, size_t max) {
  if (!prv_in_user(str, 0)) {
    return -EFAULT;
  }
  size_t window = (size_t)(s_user_mem + sizeof(s_user_mem) - (const uint8_t *)str);
  if (max < window) {
    window = max + 1;
  }
  const char *nul = memchr(str, '\0', window);
  return (nul == nullptr) ? -EFAULT : (int)(nul - str);
}

// Syscalls under test
///////////////////////////////////////////////////////////

typedef struct {
  int a;
  int b;
} Pair;

static const void *s_handler_arg;
static int s_handler_calls;

PBL_SYSCALL(int, sys_test_val, (VAL, int, a), (VAL, int, b)) {
  s_handler_calls++;
  return a - b;
}

PBL_SYSCALL(int, sys_test_in, (IN, const Pair *, p)) {
  s_handler_calls++;
  s_handler_arg = p;
  return p->a + p->b;
}

PBL_SYSCALL(void, sys_test_out, (OUT, Pair *, p)) {
  s_handler_calls++;
  s_handler_arg = p;
  p->a = 7;
}

PBL_SYSCALL(bool, sys_test_out_opt, (OUT_OPT, Pair *, p)) {
  s_handler_calls++;
  s_handler_arg = p;
  if (p == nullptr) {
    return false;
  }
  p->b = 9;
  return true;
}

PBL_SYSCALL(size_t, sys_test_buf, (IN_BUF, const void *, buf, len), (VAL, size_t, len)) {
  s_handler_calls++;
  s_handler_arg = buf;
  return len;
}

PBL_SYSCALL(size_t, sys_test_str, (STR, const char *, str)) {
  s_handler_calls++;
  s_handler_arg = str;
  return strlen(str);
}

PBL_SYSCALL(size_t, sys_test_array, (IN_ARRAY, const uint32_t *, items, count),
            (VAL, size_t, count)) {
  s_handler_calls++;
  s_handler_arg = items;
  return count;
}

PBL_SYSCALL(size_t, sys_test_strn, (STRN, const char *, str, 5)) {
  s_handler_calls++;
  return strlen(str);
}

PBL_SYSCALL_NORETURN(sys_test_noreturn, (VAL, int, code)) {
  exit(code);
}

PBL_SYSCALL(bool, sys_test_from_user) {
  return from_user;
}

// Tests
///////////////////////////////////////////////////////////

static void prv_call_from(PebbleTask task) {
  s_elevated = true;
  stub_pebble_tasks_set_current(task);
}

void test_syscall_define__initialize(void) {
  memset(s_user_mem, 0, sizeof(s_user_mem));
  memset(s_kernel_mem, 0, sizeof(s_kernel_mem));
  s_elevated = false;
  s_handler_arg = nullptr;
  s_handler_calls = 0;
  stub_pebble_tasks_set_current(PebbleTask_KernelMain);
}

void test_syscall_define__kernel_callers_skip_checks(void) {
  Pair *p = (Pair *)s_kernel_mem;
  *p = (Pair){.a = 1, .b = 2};

  cl_assert_equal_i(sys_test_in(p), 3);
  cl_assert(s_handler_arg == p);

  sys_test_out(p);
  cl_assert(s_handler_arg == p);
  cl_assert_equal_i(p->a, 7);

  cl_assert_equal_i(sys_test_buf(s_kernel_mem, sizeof(s_kernel_mem)), sizeof(s_kernel_mem));
}

void test_syscall_define__val(void) {
  prv_call_from(PebbleTask_App);
  cl_assert_equal_i(sys_test_val(5, 3), 2);
}

void test_syscall_define__in_is_copied(void) {
  Pair *p = (Pair *)s_user_mem;
  *p = (Pair){.a = 1, .b = 2};

  prv_call_from(PebbleTask_App);
  cl_assert_equal_i(sys_test_in(p), 3);
  cl_assert(s_handler_arg != nullptr);
  cl_assert(s_handler_arg != p);
}

void test_syscall_define__in_rejects_bad_pointers(void) {
  prv_call_from(PebbleTask_App);
  cl_assert_panic(sys_test_in((const Pair *)s_kernel_mem));
  cl_assert_panic(sys_test_in(nullptr));
  cl_assert_panic(sys_test_in((const Pair *)(s_user_mem + sizeof(s_user_mem) - 4)));
  cl_assert_equal_i(s_handler_calls, 0);
}

void test_syscall_define__out_is_copied_back(void) {
  Pair *p = (Pair *)s_user_mem;
  *p = (Pair){.a = 1, .b = 2};

  prv_call_from(PebbleTask_App);
  sys_test_out(p);
  cl_assert(s_handler_arg != p);
  cl_assert_equal_i(p->a, 7);
  // Bytes the handler does not write keep the caller's values.
  cl_assert_equal_i(p->b, 2);
}

void test_syscall_define__out_rejects_bad_pointers(void) {
  prv_call_from(PebbleTask_App);
  cl_assert_panic(sys_test_out((Pair *)s_kernel_mem));
  cl_assert_panic(sys_test_out(nullptr));
  cl_assert_equal_i(s_handler_calls, 0);
  cl_assert_equal_i(((Pair *)s_kernel_mem)->a, 0);
}

void test_syscall_define__out_opt(void) {
  Pair *p = (Pair *)s_user_mem;
  *p = (Pair){.a = 1, .b = 2};

  prv_call_from(PebbleTask_App);
  cl_assert(sys_test_out_opt(p));
  cl_assert_equal_i(p->a, 1);
  cl_assert_equal_i(p->b, 9);

  cl_assert(!sys_test_out_opt(nullptr));
  cl_assert(s_handler_arg == nullptr);

  cl_assert_panic(sys_test_out_opt((Pair *)s_kernel_mem));
}

void test_syscall_define__buf_is_borrowed(void) {
  prv_call_from(PebbleTask_App);
  cl_assert_equal_i(sys_test_buf(s_user_mem, sizeof(s_user_mem)), sizeof(s_user_mem));
  cl_assert(s_handler_arg == s_user_mem);
}

void test_syscall_define__buf_rejects_bad_ranges(void) {
  prv_call_from(PebbleTask_App);
  cl_assert_panic(sys_test_buf(s_kernel_mem, 1));
  cl_assert_panic(sys_test_buf(s_user_mem, sizeof(s_user_mem) + 1));
  cl_assert_panic(sys_test_buf(s_user_mem + 16, SIZE_MAX - 8));
  cl_assert_equal_i(s_handler_calls, 0);
}

void test_syscall_define__str(void) {
  strcpy((char *)s_user_mem, "hello");

  prv_call_from(PebbleTask_Worker);
  cl_assert_equal_i(sys_test_str((const char *)s_user_mem), 5);
  cl_assert(s_handler_arg == s_user_mem);
}

void test_syscall_define__str_rejects_unterminated(void) {
  memset(s_user_mem, 'x', sizeof(s_user_mem));
  strcpy((char *)s_kernel_mem, "kernel");

  prv_call_from(PebbleTask_App);
  cl_assert_panic(sys_test_str((const char *)s_user_mem));
  cl_assert_panic(sys_test_str((const char *)s_kernel_mem));
  cl_assert_equal_i(s_handler_calls, 0);
}

void test_syscall_define__array(void) {
  prv_call_from(PebbleTask_App);
  cl_assert_equal_i(sys_test_array((const uint32_t *)s_user_mem, 64), 64);
  cl_assert(s_handler_arg == s_user_mem);
}

void test_syscall_define__array_rejects_bad_counts(void) {
  prv_call_from(PebbleTask_App);
  cl_assert_panic(sys_test_array((const uint32_t *)s_user_mem, 65));
  // count * sizeof(uint32_t) wraps around to a small size.
  cl_assert_panic(sys_test_array((const uint32_t *)s_user_mem, SIZE_MAX / 4 + 2));
  cl_assert_panic(sys_test_array((const uint32_t *)s_kernel_mem, 1));
  cl_assert_equal_i(s_handler_calls, 0);
}

void test_syscall_define__strn(void) {
  prv_call_from(PebbleTask_App);
  strcpy((char *)s_user_mem, "12345");
  cl_assert_equal_i(sys_test_strn((const char *)s_user_mem), 5);

  strcpy((char *)s_user_mem, "123456");
  cl_assert_panic(sys_test_strn((const char *)s_user_mem));
  cl_assert_equal_i(s_handler_calls, 1);
}

void test_syscall_define__from_user(void) {
  cl_assert(!sys_test_from_user());

  prv_call_from(PebbleTask_App);
  cl_assert(sys_test_from_user());
}
