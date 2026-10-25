/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// SYS_INIT functions are collected in a section and run by zbt_init().

#pragma once

#include <zephyr/sys/iterable_sections.h>
#include <zephyr/toolchain.h>

struct zbt_init_entry {
  int (*init_fn)(void);
};

#define SYS_INIT(init_fn, level, prio)                 \
  static const STRUCT_SECTION_ITERABLE(zbt_init_entry, \
                                       _CONCAT(zbt_init_, init_fn)) = {.init_fn = (init_fn)}

#define SYS_INIT_NAMED(name, init_fn, level, prio)     \
  static const STRUCT_SECTION_ITERABLE(zbt_init_entry, \
                                       _CONCAT(zbt_init_, name)) = {.init_fn = (init_fn)}
