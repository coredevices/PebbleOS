/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "preferred_content_size.h"

#include "shell/system_theme.h"
#include "syscall/syscall_internal.h"

DEFINE_SYSCALL(PreferredContentSize, preferred_content_size, void) {
  return system_theme_get_content_size();
}
