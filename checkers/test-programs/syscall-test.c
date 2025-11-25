/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

extern bool syscall_internal_elevate_privilege();
extern void syscall_assert_userspace_buffer(const void * check_buffer, int size);

extern void * app_malloc(unsigned size);

void do_stuff(void * buffer, int size) {
  strncpy(buffer, "Woooooooo", size);
}

void good_syscall(void * buffer, int size) {
  syscall_internal_elevate_privilege();

  syscall_assert_userspace_buffer(buffer, size);

  do_stuff(buffer, size);
}

void bad_syscall(void * buffer, int size) {
  syscall_internal_elevate_privilege();
  do_stuff(buffer, size);
}

void stupid_syscall(void * buffer, int size) {
  void * stupid = (char *)buffer + 1;
  syscall_internal_elevate_privilege();
  do_stuff(stupid, size);
}

void not_syscall(void * buffer, int size) {
  do_stuff(buffer, size);
}

void nested_syscall(void * buffer, int size) {
  syscall_internal_elevate_privilege();
  syscall_assert_userspace_buffer(buffer, size);
  bad_syscall(buffer, size);
  good_syscall(buffer, size);
}

void bad_nested_syscall(void * buffer, int size) {
  syscall_internal_elevate_privilege();
  bad_syscall(buffer, size);
}

void hidden_bad_syscall(void * buffer, int size) {
  syscall_internal_elevate_privilege();
  do_stuff(buffer, size);
}

void if_syscall(void * buffer, int size) {
  if (syscall_internal_elevate_privilege()) {
    syscall_assert_userspace_buffer(buffer, size);
  }
  do_stuff(buffer, size);
}

void wrapper() {
  void * buffer = NULL;
  int size = 0;

  good_syscall(buffer, size);
  // This tests to make sure analysis continues through good_syscall
  hidden_bad_syscall(buffer, size);
}

bool cond(const char *font_key) {
  return &cond == font_key;
}

void conditional_syscall(const char *font_key) {
  syscall_internal_elevate_privilege();

  if (font_key) {
    if (!cond(font_key)) {
        do_stuff(font_key, 5);
    }
  }
}

void store_syscall(char * buf, int size) {
  syscall_internal_elevate_privilege();

  buf[0] = 'a';
  char * new = buf;

  do_stuff(new, size);

}

void load_syscall(char * buf, int size) {
  syscall_internal_elevate_privilege();

  char test = buf[0];

  do_stuff(&test, size);
}

void bind_syscall(char * buf, int size) {
  syscall_internal_elevate_privilege();

  char * new = buf;

  do_stuff(new, size);
}

void malloc_syscall() {
  syscall_internal_elevate_privilege();
  void *buf = app_malloc(5);

  syscall_assert_userspace_buffer(buf, 5);

  do_stuff(buf, 5);
}
