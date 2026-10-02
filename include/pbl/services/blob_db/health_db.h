/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/services/activity/activity.h"
#include "system/status_codes.h"
#include "pbl/util/time.h"

/**
 * @defgroup services_blob_db_health_db Health database
 * @ingroup services_blob_db
 * @brief Health typicals and averages synced from the phone (::BlobDBIdHealth).
 * @{
 */

/**
 * @brief Get the typical value of a metric for a weekday.
 *
 * Only sleep metrics have typical values. For typical steps use
 * health_db_get_typical_step_averages().
 *
 * @param metric Metric.
 * @param day Weekday.
 * @param[out] value_out Typical value.
 * @return true on success, false if unknown or unsupported.
 */
bool health_db_get_typical_value(ActivityMetric metric, enum pbl_weekday day, int32_t *value_out);

/**
 * @brief Get the average value of a metric over the last month.
 *
 * Only ActivityMetricStepCount and ActivityMetricSleepTotalSeconds are stored.
 *
 * @param metric Metric.
 * @param[out] value_out Average value.
 * @return true on success, false if unknown or unsupported.
 */
bool health_db_get_monthly_average_value(ActivityMetric metric, int32_t *value_out);

/**
 * @brief Get the typical step counts of a weekday, also known as "typical steps".
 *
 * @param day Weekday.
 * @param[out] averages Step averages; set to unknown when not available.
 * @return true on success.
 */
bool health_db_get_typical_step_averages(enum pbl_weekday day, ActivityMetricAverages *averages);

/**
 * @brief Store typical step values for a weekday, for test and debug purposes only.
 *
 * @param metric Ignored, step typicals are always written.
 * @param day Weekday.
 * @param values Values to store.
 * @param num_values Number of entries in @p values.
 * @return true on success.
 */
bool health_db_set_typical_values(ActivityMetric metric, enum pbl_weekday day, uint16_t *values,
                                  int num_values);

/** @brief Initialize the health database. */
void health_db_init(void);

/**
 * @brief Insert or replace a record in the health database.
 *
 * Keys are strings like @c monday_sleepData or @c average_dailySteps and values a multiple
 * of 4 bytes. Movement data is only forwarded to the activity service and not stored; other
 * records are stored and also forwarded.
 *
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @param val Value data.
 * @param val_len Length of @p val in bytes.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t health_db_insert(const uint8_t *key, int key_len, const uint8_t *val, int val_len);

/**
 * @brief Get the length of a record in the health database.
 *
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @return Length of the value in bytes, 0 if not found, or a negative error code.
 */
int health_db_get_len(const uint8_t *key, int key_len);

/**
 * @brief Read a record from the health database.
 *
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @param[out] val_out Buffer for the value.
 * @param val_out_len Size of @p val_out in bytes.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t health_db_read(const uint8_t *key, int key_len, uint8_t *val_out, int val_out_len);

/**
 * @brief Delete a record from the health database.
 *
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t health_db_delete(const uint8_t *key, int key_len);

/**
 * @brief Delete all records of the health database.
 *
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t health_db_flush(void);

/**
 * @brief Compact the settings file backing the health database.
 *
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t health_db_compact(void);

/** @} */
