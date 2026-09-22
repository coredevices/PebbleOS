/* SPDX-FileCopyrightText: 2026 Aliaksandr Karnilovich */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/activity/activity_private.h"

#include "kernel/pbl_malloc.h"
#include "pbl/services/filesystem/pfs.h"
#include "pbl/util/size.h"

#include <string.h>

#define WEIGHT_HISTORY_FILE_NAME "weight_history"
#define WEIGHT_HISTORY_FILE_LEN  0x1000

static PBL_MUTEX_DEFINE(s_weight_history_mutex);

typedef struct {
  size_t count;
  size_t invalid_count;
  uint32_t oldest_utc;
  uint32_t newest_utc;
  uint16_t newest_weight_dag;
} WeightHistoryStats;

typedef struct {
  ActivityWeightSample *samples;
  size_t count;
  size_t max_samples;
} WeightHistoryReadContext;

static bool prv_is_valid_weight(uint16_t weight_dag) {
  return weight_dag >= ACTIVITY_WEIGHT_MIN_DAG && weight_dag <= ACTIVITY_WEIGHT_MAX_DAG;
}

static SettingsFile *prv_open_recent_file(void) {
  SettingsFile *file = kernel_malloc_check(sizeof(*file));
  if (settings_file_open(file, WEIGHT_HISTORY_FILE_NAME, WEIGHT_HISTORY_FILE_LEN) != S_SUCCESS) {
    kernel_free(file);
    return NULL;
  }
  return file;
}

static void prv_close_recent_file(SettingsFile *file) {
  settings_file_close(file);
  kernel_free(file);
}

static bool prv_stats_cb(SettingsFile *file, SettingsRecordInfo *info, void *context) {
  WeightHistoryStats *stats = context;
  if (info->key_len != sizeof(uint32_t) || info->val_len != sizeof(uint16_t)) {
    stats->invalid_count++;
    return true;
  }

  uint32_t utc_sec;
  uint16_t weight_dag;
  info->get_key(file, &utc_sec, sizeof(utc_sec));
  info->get_val(file, &weight_dag, sizeof(weight_dag));
  if (utc_sec == 0 || !prv_is_valid_weight(weight_dag)) {
    stats->invalid_count++;
    return true;
  }
  stats->count++;
  if (stats->oldest_utc == 0 || utc_sec < stats->oldest_utc) {
    stats->oldest_utc = utc_sec;
  }
  if (stats->newest_utc == 0 || utc_sec > stats->newest_utc) {
    stats->newest_utc = utc_sec;
    stats->newest_weight_dag = weight_dag;
  }
  return true;
}

static WeightHistoryStats prv_get_stats(SettingsFile *file) {
  WeightHistoryStats stats = {};
  settings_file_each(file, prv_stats_cb, &stats);
  return stats;
}

static bool prv_keep_valid_sample(void *key, size_t key_len, void *value, size_t value_len,
                                  void *context) {
  return key_len == sizeof(uint32_t) && value_len == sizeof(uint16_t) &&
         *(uint32_t *)key > 0 && prv_is_valid_weight(*(uint16_t *)value);
}

static WeightHistoryStats prv_sanitize_recent(SettingsFile *file) {
  WeightHistoryStats stats = prv_get_stats(file);
  if (stats.invalid_count > 0) {
    settings_file_rewrite_filtered(file, prv_keep_valid_sample, NULL);
    stats = prv_get_stats(file);
  }
  return stats;
}

static bool prv_keep_not_oldest(void *key, size_t key_len, void *value, size_t value_len,
                                void *context) {
  if (key_len != sizeof(uint32_t) || value_len != sizeof(uint16_t)) {
    return true;
  }
  return *(uint32_t *)key != *(uint32_t *)context;
}

static void prv_prune_recent(SettingsFile *file) {
  WeightHistoryStats stats = prv_get_stats(file);
  while (stats.count > ACTIVITY_WEIGHT_RECENT_MAX && stats.oldest_utc != 0) {
    settings_file_rewrite_filtered(file, prv_keep_not_oldest, &stats.oldest_utc);
    stats = prv_get_stats(file);
  }
}

static bool prv_read_recent_cb(SettingsFile *file, SettingsRecordInfo *info, void *context) {
  WeightHistoryReadContext *read_context = context;
  if (info->key_len != sizeof(uint32_t) || info->val_len != sizeof(uint16_t)) {
    return true;
  }

  ActivityWeightSample sample;
  info->get_key(file, &sample.utc_sec, sizeof(sample.utc_sec));
  info->get_val(file, &sample.weight_dag, sizeof(sample.weight_dag));
  if (sample.utc_sec == 0 || !prv_is_valid_weight(sample.weight_dag)) {
    return true;
  }

  size_t insert_at = 0;
  while (insert_at < read_context->count &&
         read_context->samples[insert_at].utc_sec > sample.utc_sec) {
    insert_at++;
  }
  if (insert_at >= read_context->max_samples) {
    return true;
  }

  const size_t move_count =
      MIN(read_context->count, read_context->max_samples - 1) - insert_at;
  if (move_count > 0) {
    memmove(&read_context->samples[insert_at + 1], &read_context->samples[insert_at],
            move_count * sizeof(sample));
  }
  read_context->samples[insert_at] = sample;
  read_context->count = MIN(read_context->count + 1, read_context->max_samples);
  return true;
}

bool activity_weight_history_add(time_t utc_sec, uint16_t weight_dag) {
  if (utc_sec <= 0 || !prv_is_valid_weight(weight_dag)) {
    return false;
  }

  bool success = false;
  pbl_mutex_lock(&s_weight_history_mutex, PBL_FOREVER);
  SettingsFile *recent_file = prv_open_recent_file();
  if (recent_file) {
    prv_sanitize_recent(recent_file);
    const uint32_t key = utc_sec;
    status_t status =
        settings_file_set(recent_file, &key, sizeof(key), &weight_dag, sizeof(weight_dag));
    if (status == E_OUT_OF_STORAGE) {
      WeightHistoryStats stats = prv_get_stats(recent_file);
      if (stats.oldest_utc != 0) {
        settings_file_rewrite_filtered(recent_file, prv_keep_not_oldest,
                                       (void *)&stats.oldest_utc);
        status = settings_file_set(recent_file, &key, sizeof(key), &weight_dag,
                                   sizeof(weight_dag));
      }
    }
    if (status == S_SUCCESS) {
      prv_prune_recent(recent_file);
      success = true;
    }
    prv_close_recent_file(recent_file);
  }

  pbl_mutex_unlock(&s_weight_history_mutex);
  return success;
}

bool activity_weight_history_remove_latest(time_t utc_sec, uint16_t *new_weight_dag) {
  if (utc_sec <= 0 || !new_weight_dag) {
    return false;
  }

  bool success = false;
  pbl_mutex_lock(&s_weight_history_mutex, PBL_FOREVER);
  SettingsFile *recent_file = prv_open_recent_file();
  if (!recent_file) {
    goto unlock;
  }

  const WeightHistoryStats stats = prv_sanitize_recent(recent_file);
  if (stats.count <= 1 || stats.newest_utc == 0) {
    prv_close_recent_file(recent_file);
    goto unlock;
  }

  const uint32_t removed_utc = stats.newest_utc;
  if (settings_file_delete(recent_file, &removed_utc, sizeof(removed_utc)) != S_SUCCESS) {
    prv_close_recent_file(recent_file);
    goto unlock;
  }
  success = true;

  const WeightHistoryStats remaining = prv_get_stats(recent_file);
  *new_weight_dag = remaining.newest_weight_dag;

  prv_close_recent_file(recent_file);

unlock:
  pbl_mutex_unlock(&s_weight_history_mutex);
  return success;
}

size_t activity_weight_history_get_recent(ActivityWeightSample *samples, size_t max_samples) {
  if (!samples || max_samples == 0) {
    return 0;
  }

  WeightHistoryReadContext context = {
    .samples = samples,
    .max_samples = MIN(max_samples, ACTIVITY_WEIGHT_RECENT_MAX),
  };

  pbl_mutex_lock(&s_weight_history_mutex, PBL_FOREVER);
  SettingsFile *file = prv_open_recent_file();
  if (file) {
    prv_sanitize_recent(file);
    settings_file_each(file, prv_read_recent_cb, &context);
    prv_close_recent_file(file);
  }
  pbl_mutex_unlock(&s_weight_history_mutex);
  return context.count;
}

void activity_weight_history_clear(void) {
  pbl_mutex_lock(&s_weight_history_mutex, PBL_FOREVER);
  pfs_remove(WEIGHT_HISTORY_FILE_NAME);
  pbl_mutex_unlock(&s_weight_history_mutex);
}
