/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @defgroup util_stats Statistics
 * @ingroup util
 * @brief Basic statistics over arrays of @c int32_t.
 *
 * @code{.c}
 * int32_t out[2];
 *
 * pbl_stats_calculate(PBL_STATS_OP_SUM | PBL_STATS_OP_MAX, samples, num_samples, NULL, NULL,
 *                     out);
 * // out[0] is the sum, out[1] the maximum
 * @endcode
 * @{
 */

/**
 * @brief Filter of the values included in pbl_stats_calculate().
 *
 * @param index Index of the value in the data array.
 * @param value Value.
 * @param context Filter data.
 * @return true to include the value.
 */
typedef bool (*pbl_stats_filter_t)(int index, int32_t value, void *context);

/**
 * @brief Operations of pbl_stats_calculate(), combined as a bit field.
 *
 * With a filter, every operation only considers the values it accepts.
 */
enum pbl_stats_op {
  /** Sum. */
  PBL_STATS_OP_SUM = (1 << 0),
  /** Average, truncated; 0 without values. */
  PBL_STATS_OP_AVERAGE = (1 << 1),
  /** Minimum; INT32_MAX without values. */
  PBL_STATS_OP_MIN = (1 << 2),
  /** Maximum; INT32_MIN without values. */
  PBL_STATS_OP_MAX = (1 << 3),
  /** Number of values included; the number of data points without a filter. */
  PBL_STATS_OP_COUNT = (1 << 4),
  /** Longest run of consecutive values included; the number of data points without a filter. */
  PBL_STATS_OP_CONSECUTIVE = (1 << 5),
  /** Length of the run of included values at the start of the data. */
  PBL_STATS_OP_CONSECUTIVE_FIRST = (1 << 6),
  /** Median; the lower of the two middle values for an even count. */
  PBL_STATS_OP_MEDIAN = (1 << 7),
};

/**
 * @brief Calculate basic statistics over an array.
 *
 * Results are written to @p basic_out in the order of enum pbl_stats_op, without gaps: for
 * <tt>PBL_STATS_OP_MAX | PBL_STATS_OP_SUM</tt>, @p basic_out[0] is the sum and @p basic_out[1]
 * the maximum.
 *
 * @param op Operations, a combination of enum pbl_stats_op.
 * @param data Values. Nothing is written if NULL.
 * @param num_data Number of values.
 * @param filter Filter, or NULL to include every value.
 * @param context Filter data.
 * @param[out] basic_out One result per operation in @p op.
 */
void pbl_stats_calculate(enum pbl_stats_op op, const int32_t *data, size_t num_data,
                         pbl_stats_filter_t filter, void *context, int32_t *basic_out);

/**
 * @brief Calculate the weighted median of an array.
 *
 * The weighted median is the value x[k] such that the total weight of the values below it and
 * the total weight of the values above it are each at most half the total weight. When two
 * values qualify, their mean is returned. Uses integer division throughout and allocates a
 * temporary copy with calloc().
 *
 * @param vals Values.
 * @param weights_x100 Positive weights, scaled by 100.
 * @param num_data Number of values.
 * @return Weighted median, or 0 for invalid arguments, zero total weight or allocation failure.
 */
int32_t pbl_stats_weighted_median(const int32_t *vals, const int32_t *weights_x100,
                                  size_t num_data);

/** @} */
