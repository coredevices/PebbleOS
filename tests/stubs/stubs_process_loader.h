/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "process_management/pebble_process_md.h"
#include "process_management/process_loader.h"
#include "util/attributes.h"

void * WEAK process_loader_load(const PebbleProcessMd *app_md, PebbleTask task,
                                MemorySegment *segment) {
  return app_md->main_func;
}
