/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "services/normal/blob_db/health_db.h"

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
