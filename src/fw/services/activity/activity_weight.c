/* SPDX-FileCopyrightText: 2026 Aliaksandr Karnilovich */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/activity/activity_private.h"

#include "kernel/pbl_malloc.h"
#include "pbl/services/filesystem/pfs.h"
#include "pbl/util/size.h"
#include "util/time/time.h"

#include <string.h>

#define WEIGHT_HISTORY_FILE_NAME "weight_history"
#define WEIGHT_HISTORY_FILE_LEN  0x1000

_Static_assert(ACTIVITY_WEIGHT_HISTORY_DAYS == ACTIVITY_HISTORY_DAYS,
               "Weight history must match activity history");

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

typedef struct {
  uint16_t day;
  uint32_t newest_utc;
  uint16_t newest_weight_dag;
} WeightDayLatestContext;

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

static bool prv_find_latest_for_day_cb(SettingsFile *file, SettingsRecordInfo *info,
                                       void *context) {
  WeightDayLatestContext *day_context = context;
  if (info->key_len != sizeof(uint32_t) || info->val_len != sizeof(uint16_t)) {
    return true;
  }

  uint32_t utc_sec;
  uint16_t weight_dag;
  info->get_key(file, &utc_sec, sizeof(utc_sec));
  info->get_val(file, &weight_dag, sizeof(weight_dag));
  if (utc_sec > day_context->newest_utc && time_util_get_day(utc_sec) == day_context->day &&
      prv_is_valid_weight(weight_dag)) {
    day_context->newest_utc = utc_sec;
    day_context->newest_weight_dag = weight_dag;
  }
  return true;
}

static bool prv_align_daily_history(ActivitySettingsValueHistory *history, time_t utc_sec) {
  if (history->utc_sec == 0) {
    history->utc_sec = utc_sec;
    return true;
  }

  const uint16_t current_day = time_util_get_day(utc_sec);
  const uint16_t stored_day = time_util_get_day(history->utc_sec);
  const uint16_t elapsed_days = current_day - stored_day;
  if (elapsed_days == 0) {
    return false;
  }

  if (elapsed_days >= ACTIVITY_WEIGHT_HISTORY_DAYS) {
    memset(history->values, 0, sizeof(history->values));
  } else {
    for (int i = ACTIVITY_WEIGHT_HISTORY_DAYS - 1; i >= elapsed_days; i--) {
      history->values[i] = history->values[i - elapsed_days];
    }
    memset(history->values, 0, elapsed_days * sizeof(history->values[0]));
  }
  history->utc_sec = utc_sec;
  return true;
}

static bool prv_get_daily(SettingsFile *file, time_t utc_sec,
                          ActivitySettingsValueHistory *history) {
  const ActivitySettingsKey key = ActivitySettingsKeyWeightDailyHistory;
  *history = (ActivitySettingsValueHistory){};
  const int stored_len = settings_file_get_len(file, &key, sizeof(key));
  bool needs_save = stored_len > 0 && stored_len != sizeof(*history);
  if (stored_len == sizeof(*history)) {
    settings_file_get(file, &key, sizeof(key), history, sizeof(*history));
  }

  for (size_t i = 0; i < ARRAY_LENGTH(history->values); i++) {
    if (history->values[i] > 0 &&
        (history->values[i] < ACTIVITY_WEIGHT_MIN_DAG ||
         history->values[i] > ACTIVITY_WEIGHT_MAX_DAG)) {
      history->values[i] = 0;
      needs_save = true;
    }
  }

  needs_save |= prv_align_daily_history(history, utc_sec);
  if (needs_save) {
    return settings_file_set(file, &key, sizeof(key), history, sizeof(*history)) == S_SUCCESS;
  }
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

  ActivityState *state = activity_private_state();
  pbl_mutex_lock(&state->mutex, PBL_FOREVER);
  SettingsFile *activity_file = activity_private_settings_open();
  if (activity_file) {
    ActivitySettingsValueHistory history;
    if (prv_get_daily(activity_file, utc_sec, &history)) {
      const ActivitySettingsKey key = ActivitySettingsKeyWeightDailyHistory;
      history.values[0] = weight_dag;
      success = settings_file_set(activity_file, &key, sizeof(key), &history, sizeof(history)) ==
                    S_SUCCESS &&
                success;
    } else {
      success = false;
    }
    activity_private_settings_close(activity_file);
  } else {
    success = false;
  }
  pbl_mutex_unlock(&state->mutex);
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

  const uint16_t removed_day = time_util_get_day(removed_utc);
  WeightDayLatestContext day_context = {
    .day = removed_day,
  };
  settings_file_each(recent_file, prv_find_latest_for_day_cb, &day_context);
  prv_close_recent_file(recent_file);

  ActivityState *state = activity_private_state();
  pbl_mutex_lock(&state->mutex, PBL_FOREVER);
  SettingsFile *activity_file = activity_private_settings_open();
  if (activity_file) {
    ActivitySettingsValueHistory history;
    if (prv_get_daily(activity_file, utc_sec, &history)) {
      const uint16_t elapsed_days = time_util_get_day(utc_sec) - removed_day;
      if (elapsed_days < ACTIVITY_WEIGHT_HISTORY_DAYS) {
        const ActivitySettingsKey key = ActivitySettingsKeyWeightDailyHistory;
        history.values[elapsed_days] = day_context.newest_weight_dag;
        settings_file_set(activity_file, &key, sizeof(key), &history, sizeof(history));
      }
    }
    activity_private_settings_close(activity_file);
  }
  pbl_mutex_unlock(&state->mutex);

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

bool activity_weight_history_get_daily(time_t utc_sec, ActivitySettingsValueHistory *history) {
  if (!history || utc_sec <= 0) {
    return false;
  }

  bool success = false;
  pbl_mutex_lock(&s_weight_history_mutex, PBL_FOREVER);
  ActivityState *state = activity_private_state();
  pbl_mutex_lock(&state->mutex, PBL_FOREVER);
  SettingsFile *file = activity_private_settings_open();
  if (file) {
    success = prv_get_daily(file, utc_sec, history);
    activity_private_settings_close(file);
  }
  pbl_mutex_unlock(&state->mutex);
  pbl_mutex_unlock(&s_weight_history_mutex);
  return success;
}

void activity_weight_history_clear(void) {
  pbl_mutex_lock(&s_weight_history_mutex, PBL_FOREVER);
  pfs_remove(WEIGHT_HISTORY_FILE_NAME);
  ActivityState *state = activity_private_state();
  pbl_mutex_lock(&state->mutex, PBL_FOREVER);
  SettingsFile *file = activity_private_settings_open();
  if (file) {
    const ActivitySettingsKey key = ActivitySettingsKeyWeightDailyHistory;
    settings_file_delete(file, &key, sizeof(key));
    activity_private_settings_close(file);
  }
  pbl_mutex_unlock(&state->mutex);
  pbl_mutex_unlock(&s_weight_history_mutex);
}
