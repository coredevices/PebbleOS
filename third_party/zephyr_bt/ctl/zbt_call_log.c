/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "zbt_call_log.h"

#include <string.h>

#include <pbl/kernel/mutex.h>
#include <pbl/services/settings/settings_file.h>

#include <kernel/pbl_malloc.h>
#include <system/status_codes.h>

#define LOG_FILE      "bt_classic_calls"
#define LOG_FILE_SIZE 4096

static const char LOG_KEY[] = "CALLS";

static PBL_MUTEX_DEFINE(s_lock);

// Reads the log into @p out (ZBT_CALL_LOG_MAX long) from an open file; returns the count.
static int prv_read(SettingsFile *file, struct zbt_call_record *out) {
  const int len = settings_file_get_len(file, LOG_KEY, sizeof(LOG_KEY));
  if (len <= 0 || len % (int)sizeof(*out) != 0 || len > ZBT_CALL_LOG_MAX * (int)sizeof(*out)) {
    return 0;
  }
  if (settings_file_get(file, LOG_KEY, sizeof(LOG_KEY), out, len) != S_SUCCESS) {
    return 0;
  }
  return len / (int)sizeof(*out);
}

void zbt_call_log_add(const struct zbt_call_record *rec) {
  // One more than the log, for the new record in front; only while it is written.
  struct zbt_call_record *log = kernel_malloc((ZBT_CALL_LOG_MAX + 1) * sizeof(*log));
  if (log == NULL) {
    return;
  }
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  SettingsFile file;
  if (settings_file_open(&file, LOG_FILE, LOG_FILE_SIZE) == S_SUCCESS) {
    int n = prv_read(&file, &log[1]);
    if (n == ZBT_CALL_LOG_MAX) {
      n--;
    }
    log[0] = *rec;
    log[0].number[ZBT_CLIP_MAX - 1] = '\0';
    (void)settings_file_set(&file, LOG_KEY, sizeof(LOG_KEY), log, (n + 1) * sizeof(log[0]));
    settings_file_close(&file);
  }
  pbl_mutex_unlock(&s_lock);
  kernel_free(log);
}

int zbt_call_log_read(struct zbt_call_record *out, int max) {
  // The whole log is one record: read it into @p out when it fits, else through the heap.
  struct zbt_call_record *log = out;
  if (max < ZBT_CALL_LOG_MAX) {
    log = kernel_malloc(ZBT_CALL_LOG_MAX * sizeof(*log));
    if (log == NULL) {
      return 0;
    }
  }
  int n = 0;
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  SettingsFile file;
  if (settings_file_open(&file, LOG_FILE, LOG_FILE_SIZE) == S_SUCCESS) {
    n = prv_read(&file, log);
    settings_file_close(&file);
  }
  pbl_mutex_unlock(&s_lock);
  if (n > max) {
    n = max;
  }
  if (log != out) {
    memcpy(out, log, n * sizeof(*log));
    kernel_free(log);
  }
  return n;
}
