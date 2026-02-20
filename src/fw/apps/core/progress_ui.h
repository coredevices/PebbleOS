/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "process_management/pebble_process_md.h"

typedef enum {
  PROGRESS_UI_SOURCE_COREDUMP,
  PROGRESS_UI_SOURCE_LOGS,
  PROGRESS_UI_SOURCE_FW_UPDATE,
} ProgressUISource;

typedef struct {
  ProgressUISource progress_source;
} ProgressUIAppArgs;

const PebbleProcessMd* progress_ui_app_get_info();
