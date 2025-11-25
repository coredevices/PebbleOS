/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

int default_only_define(void);
int overridden_define(void);
int custom_only_define(void);

void test_test_infra__default_override_header(void) {
  // Check that the header in the default override directory was included.
  cl_assert(default_only_define());
}

void test_test_infra__overridden_custom_header(void) {
  // Check that the header in the custom override directory shadowed the default
  // override directory's version.
  cl_assert(overridden_define() == 42);
}

void test_test_infra__custom_only_header(void) {
  // Check that the header only in the custom override directory was included.
  cl_assert(custom_only_define());
}
