/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

void* app_malloc(size_t bytes) {
  return malloc(bytes);
}

void app_free(void* ptr) {
  free(ptr);
}
