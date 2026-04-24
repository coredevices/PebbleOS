/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/blob_db/health_db.h"

bool health_db_get_typical_value(ActivityMetric metric,
                                 DayInWeek day,
                                 int32_t *value_out) {
  return false;
}

bool health_db_get_monthly_average_value(ActivityMetric metric,
                                         int32_t *value_out) {
  return false;
}

bool health_db_get_typical_step_averages(DayInWeek day,
                                         ActivityMetricAverages *averages) {
  return false;
}

bool health_db_set_typical_values(ActivityMetric metric,
                                  DayInWeek day,
                                  uint16_t *values,
                                  int num_values) {
  return false;
}

void health_db_init(void) {}

status_t health_db_insert(const uint8_t *key, int key_len, const uint8_t *val, int val_len) {
  return S_SUCCESS;
}

int health_db_get_len(const uint8_t *key, int key_len) {
  return 0;
}

status_t health_db_read(const uint8_t *key, int key_len, uint8_t *val_out, int val_out_len) {
  return S_SUCCESS;
}

status_t health_db_delete(const uint8_t *key, int key_len) {
  return S_SUCCESS;
}

status_t health_db_flush(void) {
  return S_SUCCESS;
}
