/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>

//! Filter the basic statistical calculation.
//! @param index Index of the value in the data array being calculated
//! @param value Value of the current candidate data point being considered
//! @param context User data which can be used for additional context
//! @return true if the value should be included in the statistics, false otherwise
typedef bool (*StatsBasicFilter)(int index, int32_t value, void *context);

//! StatsBasicOp is a bitfield that specifies which operations \ref stats_calculate_basic should
//! perform. The ops will operate only on the filtered values when a filter is present.
typedef enum {
  StatsBasicOp_Sum         = (1 << 0), //!< Calculate the sum
  StatsBasicOp_Average     = (1 << 1), //!< Calculate the average
  //! Find the minimum value. If there is no data, or if no values match the filter, the minimum
  //! will default to INT32_MAX.
  StatsBasicOp_Min         = (1 << 2),
  //! Find the maximum value. If there is no data, or if no values match the filter, the maximum
  //! will default to INT32_MIN.
  StatsBasicOp_Max         = (1 << 3),
  //! Count the number of filtered values included in calculation.
  //! Equivalent to the number of data points when no filter is applied.
  StatsBasicOp_Count       = (1 << 4),
  //! Find the maximum streak of consecutive filtered values included in calculation.
  //! Equivalent to the number of data points when no filter is applied.
  StatsBasicOp_Consecutive = (1 << 5),
  //! Find the first streak of consecutive filtered values included in calculation.
  //! Equivalent to the number of data points when no filter is applied.
  StatsBasicOp_ConsecutiveFirst = (1 << 6),
  //! Find the median of filtered values included in calculation.
  StatsBasicOp_Median = (1 << 7),
} StatsBasicOp;

//! Calculate basic statistical information on a given array of int32_t values.
//! When returning the results, the values will be written sequentially as defined in the enum to
//! basic_out without gaps. For example, if given the op `(StatsBasicOp_Max | StatsBasicOp_Sum)`,
//! basic_out[0] will contain the sum and basic_out[1] will contain the max since Sum is specified
//! before Max in the StatsBasicOp enum. No gaps are present for Average or Min since those ops were
//! not specified for calculation.
//! @param op StatsBasicOp describing the operations to calculate
//! @param data int32_t pointer to an array of data. If data is NULL, there will be no output
//! @param num_data size_t number of data points in the data array
//! @param filter Optional StatsBasicFilter to filter data against, NULL if none specified
//! @param context Optional StatsBasicFilter context, NULL if non specified
//! @param[out] basic_out address to an int32_t or int32_t array to write results to
void stats_calculate_basic(StatsBasicOp op, const int32_t *data, size_t num_data,
                           StatsBasicFilter filter, void *context, int32_t *basic_out);


int32_t stats_calculate_weighted_median(const int32_t *vals, const int32_t *weights_x100,
                                        size_t num_data);
