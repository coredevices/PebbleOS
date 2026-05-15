/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "process_management/pebble_process_md.h"

typedef struct {
  uint8_t data;
} TestArgsData;

const PebbleProcessMd* test_args_sender_get_app_info();
