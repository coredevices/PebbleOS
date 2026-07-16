/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

void util_log(const char *filename, int line, const char *string);

#define UTIL_LOG(string) \
  do { \
    util_log(__FILE_NAME__, __LINE__, string); \
  } while (0)

void util_dbgserial_str(const char *string);
