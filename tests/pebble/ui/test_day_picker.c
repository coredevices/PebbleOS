/* SPDX-License-Identifier: Apache-2.0 */

#include <applib/ui/day_picker.c>
#include <clar.h>

void test_day_picker__repeating_only(void) {
  DayPickerData data = {.allow_once = false};
  cl_assert_equal_i(prv_day_picker_get_num_rows(nullptr, 0, &data), 4);
  for (int row = 0; row < 4; ++row) {
    cl_assert_equal_i(prv_row_to_kind(false, row), row);
    cl_assert_equal_i(prv_kind_to_row(false, (DayPickerKind)row), row);
  }
  cl_assert_equal_i(prv_kind_to_row(false, DayPickerKindJustOnce), 0);
  cl_assert_equal_i(prv_kind_to_row(false, DayPickerKindJustOnceThenDelete), 0);
}

void test_day_picker__one_time_kinds_first(void) {
  DayPickerData data = {.allow_once = true};
  const DayPickerKind expected[] = {
    DayPickerKindJustOnce, DayPickerKindJustOnceThenDelete, DayPickerKindEveryday,
    DayPickerKindWeekdays, DayPickerKindWeekends,           DayPickerKindCustom,
  };
  cl_assert_equal_i(prv_day_picker_get_num_rows(nullptr, 0, &data), 6);
  for (int row = 0; row < 6; ++row) {
    cl_assert_equal_i(prv_row_to_kind(true, row), expected[row]);
    cl_assert_equal_i(prv_kind_to_row(true, expected[row]), row);
  }
}
