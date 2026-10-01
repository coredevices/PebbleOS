/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

//! Filter the basic statistical calculation.
//! @param index Index of the value in the data array being calculated
//! @param value Value of the current candidate data point being considered
//! @param context User data which can be used for additional context
//! @return true if the value should be included in the statistics, false otherwise
typedef bool (*pbl_stats_filter_t)(int index, int32_t value, void *context);

//! Bitfield that specifies which operations \ref pbl_stats_calculate should
//! perform. The ops will operate only on the filtered values when a filter is present.
enum pbl_stats_op {
  PBL_STATS_OP_SUM = (1 << 0),     //!< Calculate the sum
  PBL_STATS_OP_AVERAGE = (1 << 1), //!< Calculate the average
  //! Find the minimum value. If there is no data, or if no values match the filter, the minimum
  //! will default to INT32_MAX.
  PBL_STATS_OP_MIN = (1 << 2),
  //! Find the maximum value. If there is no data, or if no values match the filter, the maximum
  //! will default to INT32_MIN.
  PBL_STATS_OP_MAX = (1 << 3),
  //! Count the number of filtered values included in calculation.
  //! Equivalent to the number of data points when no filter is applied.
  PBL_STATS_OP_COUNT = (1 << 4),
  //! Find the maximum streak of consecutive filtered values included in calculation.
  //! Equivalent to the number of data points when no filter is applied.
  PBL_STATS_OP_CONSECUTIVE = (1 << 5),
  //! Find the first streak of consecutive filtered values included in calculation.
  //! Equivalent to the number of data points when no filter is applied.
  PBL_STATS_OP_CONSECUTIVE_FIRST = (1 << 6),
  //! Find the median of filtered values included in calculation.
  PBL_STATS_OP_MEDIAN = (1 << 7),
};

//! Calculate basic statistical information on a given array of int32_t values.
//! When returning the results, the values will be written sequentially as defined in the enum to
//! basic_out without gaps. For example, if given the op `(PBL_STATS_OP_MAX | PBL_STATS_OP_SUM)`,
//! basic_out[0] will contain the sum and basic_out[1] will contain the max since Sum is specified
//! before Max in enum pbl_stats_op. No gaps are present for Average or Min since those ops were
//! not specified for calculation.
//! @param op Bitfield of enum pbl_stats_op describing the operations to calculate
//! @param data int32_t pointer to an array of data. If data is NULL, there will be no output
//! @param num_data size_t number of data points in the data array
//! @param filter Optional pbl_stats_filter_t to filter data against, NULL if none specified
//! @param context Optional pbl_stats_filter_t context, NULL if non specified
//! @param[out] basic_out address to an int32_t or int32_t array to write results to
void pbl_stats_calculate(enum pbl_stats_op op, const int32_t *data, size_t num_data,
                         pbl_stats_filter_t filter, void *context, int32_t *basic_out);

int32_t pbl_stats_weighted_median(const int32_t *vals, const int32_t *weights_x100,
                                  size_t num_data);
