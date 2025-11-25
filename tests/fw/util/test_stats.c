/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "util/size.h"
#include "util/stats.h"

#include "clar.h"

#include <stdio.h>

#include "stubs_pbl_malloc.h"

void test_stats__initialize(void) {
}

void test_stats__cleanup(void) {
}

void test_stats__min(void) {
  const int32_t data[] = { 10, 40, 6, 32, 73, 80, 34, 25, 62 };
  const size_t num_data = ARRAY_LENGTH(data);
  const StatsBasicOp op = StatsBasicOp_Min;
  int32_t result;
  stats_calculate_basic(op, data, num_data, NULL, NULL, &result);
  cl_assert_equal_i(result, 6);
}

void test_stats__max(void) {
  const int32_t data[] = { 10, 40, 6, 32, 73, 80, 34, 25, 62 };
  const size_t num_data = ARRAY_LENGTH(data);
  const StatsBasicOp op = StatsBasicOp_Max;
  int32_t result;
  stats_calculate_basic(op, data, num_data, NULL, NULL, &result);
  cl_assert_equal_i(result, 80);
}

void test_stats__avg(void) {
  const int32_t data[] = { 10, 40, 6, 32, 73, 80, 34, 25, 62 };
  const size_t num_data = ARRAY_LENGTH(data);
  const StatsBasicOp op = StatsBasicOp_Average;
  int32_t result;
  stats_calculate_basic(op, data, num_data, NULL, NULL, &result);
  cl_assert_equal_i(result, 40);
}

void test_stats__sum(void) {
  const int32_t data[] = { 10, 40, 6, 32, 73, 80, 34, 25, 62 };
  const size_t num_data = ARRAY_LENGTH(data);
  const StatsBasicOp op = StatsBasicOp_Sum;
  int32_t result;
  stats_calculate_basic(op, data, num_data, NULL, NULL, &result);
  cl_assert_equal_i(result, 362);
}

static void *s_context = NULL;

static bool prv_filter(int index, int32_t value, void *context) {
  cl_assert_equal_p(context, &s_context);
  return (value > 0);
}

void test_stats__filtered_count(void) {
  const int32_t data[] = { 1, 0, 1, 0, 1, 1, 1, 0, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 0 };
  const size_t num_data = ARRAY_LENGTH(data);
  const StatsBasicOp op = StatsBasicOp_Count;
  int32_t result;
  stats_calculate_basic(op, data, num_data, prv_filter, &s_context, &result);
  cl_assert_equal_i(result, 14);
}

void test_stats__filtered_consecutive(void) {
  const int32_t data[] = { 1, 0, 1, 0, 1, 1, 1, 0, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 0 };
  const size_t num_data = ARRAY_LENGTH(data);
  const StatsBasicOp op = StatsBasicOp_Consecutive;
  int32_t result;
  stats_calculate_basic(op, data, num_data, prv_filter, &s_context, &result);
  cl_assert_equal_i(result, 5);
}

void test_stats__filtered_consecutive_first(void) {
  const int32_t data[] = { 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 0 };
  const size_t num_data = ARRAY_LENGTH(data);
  const StatsBasicOp op = StatsBasicOp_ConsecutiveFirst;
  int32_t result;
  stats_calculate_basic(op, data, num_data, prv_filter, &s_context, &result);
  cl_assert_equal_i(result, 3);
}

void test_stats__median(void) {
  const int32_t data[] = { 10, 40, 6, 32, 73, 80, 34, 25, 62 };
  const size_t num_data = ARRAY_LENGTH(data);
  const StatsBasicOp op = StatsBasicOp_Median;
  int32_t result;
  stats_calculate_basic(op, data, num_data, NULL, NULL, &result);
  cl_assert_equal_i(result, 34);
}

void test_stats__all_basic_ops(void) {
  const int32_t data[] = { 10, 0, 40, 6, 0, -5, 0, 32, 73, 0, 80, 34, 25, 62, 0 };
  const size_t num_data = ARRAY_LENGTH(data);
  const StatsBasicOp op =
      (StatsBasicOp_Sum | StatsBasicOp_Average | StatsBasicOp_Min | StatsBasicOp_Max |
       StatsBasicOp_Count | StatsBasicOp_Consecutive | StatsBasicOp_ConsecutiveFirst |
       StatsBasicOp_Median);
  struct {
    int32_t sum;
    int32_t avg;
    int32_t min;
    int32_t max;
    int32_t count;
    int32_t max_streak;
    int32_t first_streak;
    int32_t median;
  } result;
  stats_calculate_basic(op, data, num_data, NULL, NULL, &result.sum);
  cl_assert_equal_i(result.sum, 357);
  cl_assert_equal_i(result.avg, 23);
  cl_assert_equal_i(result.min, -5);
  cl_assert_equal_i(result.max, 80);
  cl_assert_equal_i(result.count, num_data);
  cl_assert_equal_i(result.max_streak, num_data);
  cl_assert_equal_i(result.first_streak, num_data);
  cl_assert_equal_i(result.median, 10);
}

void test_stats__all_basic_ops_filtered(void) {
  const int32_t data[] = { 10, 0, 40, 6, 0, 0, 0, 32, 73, 0, 80, 34, 25, 62, 0 };
  const size_t num_data = ARRAY_LENGTH(data);
  const StatsBasicOp op =
      (StatsBasicOp_Sum | StatsBasicOp_Average | StatsBasicOp_Min | StatsBasicOp_Max |
       StatsBasicOp_Count | StatsBasicOp_Consecutive | StatsBasicOp_ConsecutiveFirst |
       StatsBasicOp_Median);
  struct {
    int32_t sum;
    int32_t avg;
    int32_t min;
    int32_t max;
    int32_t count;
    int32_t max_streak;
    int32_t first_streak;
    int32_t median;
  } result;
  stats_calculate_basic(op, data, num_data, prv_filter, &s_context, &result.sum);
  cl_assert_equal_i(result.sum, 362);
  cl_assert_equal_i(result.avg, 40);
  cl_assert_equal_i(result.min, 6);
  cl_assert_equal_i(result.max, 80);
  cl_assert_equal_i(result.count, 9);
  cl_assert_equal_i(result.max_streak, 4);
  cl_assert_equal_i(result.first_streak, 1);
  cl_assert_equal_i(result.median, 34);
}

void test_stats__all_basic_ops_filtered_out(void) {
  const int32_t data[] = { 0, 0, 0, 0, 0 };
  const size_t num_data = ARRAY_LENGTH(data);
  const StatsBasicOp op =
        (StatsBasicOp_Sum | StatsBasicOp_Average | StatsBasicOp_Min | StatsBasicOp_Max |
         StatsBasicOp_Count | StatsBasicOp_Consecutive | StatsBasicOp_ConsecutiveFirst |
         StatsBasicOp_Median);
  struct {
    int32_t sum;
    int32_t avg;
    int32_t min;
    int32_t max;
    int32_t count;
    int32_t max_streak;
    int32_t first_streak;
    int32_t median;
  } result;
  stats_calculate_basic(op, data, num_data, prv_filter, &s_context, &result.sum);
  cl_assert_equal_i(result.sum, 0);
  cl_assert_equal_i(result.avg, 0);
  cl_assert_equal_i(result.min, INT32_MAX);
  cl_assert_equal_i(result.max, INT32_MIN);
  cl_assert_equal_i(result.count, 0);
  cl_assert_equal_i(result.max_streak, 0);
  cl_assert_equal_i(result.first_streak, 0);
  cl_assert_equal_i(result.median, 0);
}

void test_stats__all_basic_one_value(void) {
  const int32_t data[] = { 42 };
  const size_t num_data = ARRAY_LENGTH(data);
  const StatsBasicOp op =
        (StatsBasicOp_Sum | StatsBasicOp_Average | StatsBasicOp_Min | StatsBasicOp_Max |
         StatsBasicOp_Count | StatsBasicOp_Consecutive | StatsBasicOp_ConsecutiveFirst |
         StatsBasicOp_Median);
  struct {
    int32_t sum;
    int32_t avg;
    int32_t min;
    int32_t max;
    int32_t count;
    int32_t max_streak;
    int32_t first_streak;
    int32_t median;
  } result;
  stats_calculate_basic(op, data, num_data, NULL, NULL, &result.sum);
  cl_assert_equal_i(result.sum, 42);
  cl_assert_equal_i(result.avg, 42);
  cl_assert_equal_i(result.min, 42);
  cl_assert_equal_i(result.max, 42);
  cl_assert_equal_i(result.count, 1);
  cl_assert_equal_i(result.max_streak, 1);
  cl_assert_equal_i(result.first_streak, 1);
  cl_assert_equal_i(result.median, 42);
}

void test_stats__all_basic_no_values(void) {
  const int32_t data[] = {};
  const size_t num_data = 0;
  const StatsBasicOp op =
        (StatsBasicOp_Sum | StatsBasicOp_Average | StatsBasicOp_Min | StatsBasicOp_Max |
         StatsBasicOp_Count | StatsBasicOp_Consecutive | StatsBasicOp_ConsecutiveFirst |
         StatsBasicOp_Median);
  struct {
    int32_t sum;
    int32_t avg;
    int32_t min;
    int32_t max;
    int32_t count;
    int32_t max_streak;
    int32_t first_streak;
    int32_t median;
  } result;
  stats_calculate_basic(op, data, num_data, NULL, NULL, &result.sum);
  cl_assert_equal_i(result.sum, 0);
  cl_assert_equal_i(result.avg, 0);
  cl_assert_equal_i(result.min, INT32_MAX);
  cl_assert_equal_i(result.max, INT32_MIN);
  cl_assert_equal_i(result.count, 0);
  cl_assert_equal_i(result.max_streak, 0);
  cl_assert_equal_i(result.first_streak, 0);
  cl_assert_equal_i(result.median, 0);
}

void test_stats__null_data(void) {
  const StatsBasicOp op = StatsBasicOp_Average;
  int32_t result = 0x73110;
  stats_calculate_basic(op, NULL, 0, NULL, NULL, &result);
  cl_assert_equal_i(result, 0x73110);
}

typedef struct WeightedValue {
  int32_t value;
  int32_t weight_x100;
} WeightedValue;

void test_stats__weighted_median(void) {
  //! Taken from https://en.wikipedia.org/wiki/Weighted_median
  struct {
    int32_t values[10];
    int32_t weights[10];
    int32_t num_values;
    int32_t answer;
  } test_cases[] = {
    {
      // Simple test case
      .values = {1, 3, 1},
      .weights = {2, 4, 1},
      .num_values = 3,
      .answer = 3,
    },
    {
      // Hit exactly S/2 when iterating. Take the mean of [1,2] and [3,4] -> 2
      .values = {1, 3, 1},
      .weights = {2, 4, 2},
      .num_values = 3,
      .answer = 2,
    },
    {
      // Would hit exactly S/2 when iterating if we only did integer division. Added a check to
      // prevent this.
      .values = {1, 3, 1},
      .weights = {2, 4, 3},
      .num_values = 3,
      .answer = 1,
    },
    {
      // Simple test case
      .values = {1, 100},
      .weights = {2, 1},
      .num_values = 2,
      .answer = 1,
    },
    {
      // Simple test case
      .values = {100, 1},
      .weights = {1, 2},
      .num_values = 2,
      .answer = 1,
    },
    {
      // Simple test case
      .values = {100, 1},
      .weights = {2, 1},
      .num_values = 2,
      .answer = 100,
    },
    {
      // Simple test case
      .values = {20, 3, 6},
      .weights = {1, 50, 50},
      .num_values = 3,
      .answer = 6,
    },
    {
      // Test if all weights are zero, zero should be returned
      .values = {20, 3, 6},
      .weights = {0, 0, 0},
      .num_values = 3,
      .answer = 0,
    },
    {
      // Simple test case
      .values = {10, 35, 5, 10, 15, 5, 20},
      .weights = {20, 70, 10, 20, 30, 10, 40},
      .num_values = 7,
      .answer = 20,
    },
    {
      // Only one value, return that value
      .values = {1},
      .weights = {100},
      .num_values = 1,
      .answer = 1,
    },
    {
      // Two values, equal weight. Return the lower of the two
      .values = {1, 2},
      .weights = {1, 1},
      .num_values = 2,
      .answer = 1,
    },
  };

  for (size_t i = 0; i < ARRAY_LENGTH(test_cases); i++) {
    int32_t w_median = stats_calculate_weighted_median(test_cases[i].values,
                                                       test_cases[i].weights,
                                                       test_cases[i].num_values);
    printf("W_Median test case: %d\n", (int)i);
    cl_assert_equal_i(test_cases[i].answer, w_median);
  }
}
