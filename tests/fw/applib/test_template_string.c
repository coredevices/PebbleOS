/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "applib/template_string.h"
#include "applib/template_string_private.h"

#include "util/size.h"

#include <limits.h>
#include <string.h>

#define DEBUG_PRINTING 1
#if DEBUG_PRINTING
# include <stdio.h>
#endif

void prv_template_evaluate_filter(TemplateStringState *state, const char *filter_name,
                                  const char *params);

static const char *s_error_strings[] = {
  "Success.",
  "Can't resolve.",
  "Missing closing brace.",
  "Missing argument.",
  "No result generated.",
  "Unknown filter.",
  "format() was not last filter.",
  "Time unit in predicate is invalid.",
  "Escape character at end of string.",
  "Opening parenthesis for filter was missing.",
  "Closing parenthesis for filter was missing.",
  "Invalid conversion specifier for format.",
  "Invalid parameter.",
  "Opening quote for filter was missing.",
  "Closing quote for filter was missing.",
  "Invalid argument separator.",
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Fakes & Stubs

#include "stubs_passert.h"
#include "stubs_i18n.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Test setup

#define EVAL_FALL_THROUGH -1337
#define EVAL_DEFAULT 0

static TemplateStringState s_state;
static char s_output[256];
static TemplateStringVars s_vars;
static TemplateStringError s_error;
static TemplateStringEvalConditions s_cond;

static void prv_state_init(void) {
  s_state = (TemplateStringState){
    .output = s_output,
    .output_remaining = sizeof(s_output),
    .vars = &s_vars,
    .error = &s_error,
    .eval_cond = &s_cond,

    .time_was_until = false,
    .filter_state = 0,
    .filters_complete = false,
  };
  memset(s_output, 'Z', sizeof(s_output));
  memset(&s_vars, 0, sizeof(s_vars));
  memset(&s_error, 0, sizeof(s_error));
  memset(&s_cond, 0, sizeof(s_cond));
  s_cond.eval_time = INT_MAX;
}

void test_template_string__initialize(void) {
}

void test_template_string__cleanup(void) {
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Test truncation

static const struct {
  size_t size;
  time_t intime;
  const char *instr;
  const char *output;
} s_truncation_tests[] = {
  { .size = 1, .intime = 1000,
    .instr = "foo",
    .output = "",
  },
  { .size = 3, .intime = 1000,
    .instr = "foo",
    .output = "fo",
  },
  { .size = 3, .intime = 1000,
    .instr = "{format('foo')}",
    .output = "fo",
  },
  { .size = 1, .intime = 1000,
    .instr = "{time_until(1004)|format('%S')}",
    .output = "",
  },
  { .size = 2, .intime = 1000,
    .instr = "{time_until(1040)|format('%S')}",
    .output = "4",
  },
  { .size = 6, .intime = 1000,
    .instr = "{time_until(1040)|format('%uS')}",
    .output = "40 se",
  },
};

void test_template_string__truncation(void) {
  for(size_t i = 0; i < ARRAY_LENGTH(s_truncation_tests); i++) {
#if DEBUG_PRINTING
    printf("size: %zu\n", s_truncation_tests[i].size);
    printf("intime: %jd\n", s_truncation_tests[i].intime);
    printf("input: \"%s\"\n", s_truncation_tests[i].instr);
    printf("result: \"%s\"\n", s_truncation_tests[i].output);
#endif

    TemplateStringVars vars = {};
    TemplateStringError err = {};
    TemplateStringEvalConditions cond = {};

    vars.current_time = s_truncation_tests[i].intime;
    cond.eval_time = EVAL_FALL_THROUGH;

    memset(s_output, 'Z', sizeof(s_output));
    template_string_evaluate(s_truncation_tests[i].instr, s_output, s_truncation_tests[i].size,
                             &cond, &vars, &err);

    cl_assert_equal_s(s_output, s_truncation_tests[i].output);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Test NULL arguments

void test_template_string__null_arguments(void) {
  {
    TemplateStringVars vars = {};
    TemplateStringError err = {};
    TemplateStringEvalConditions cond = {};
    vars.current_time = 0;

    cond.eval_time = EVAL_FALL_THROUGH;
    cond.force_eval_on_time = true;
    memset(s_output, 'Z', sizeof(s_output));
    bool ret = template_string_evaluate("test string {time_until(5)|format('%uS')}", s_output,
                                        sizeof(s_output), &cond, &vars, &err);
    cl_assert_equal_b(ret, true);
    cl_assert_equal_s(s_output, "test string 5 seconds");
    cl_assert_equal_i(cond.eval_time, 1);
    cl_assert_equal_b(cond.force_eval_on_time, true);
  }

  {
    TemplateStringVars vars = {};
    TemplateStringError err = {};
    TemplateStringEvalConditions cond = {};
    vars.current_time = 0;

    cond.eval_time = EVAL_FALL_THROUGH;
    cond.force_eval_on_time = true;
    strcpy(s_output, "hurf");
    bool ret = template_string_evaluate("test string {time_until(5)|format('%uS')}", s_output, 0,
                                        &cond, &vars, &err);
    cl_assert_equal_b(ret, true);
    cl_assert_equal_s(s_output, "hurf");
    cl_assert_equal_i(cond.eval_time, 1);
    cl_assert_equal_b(cond.force_eval_on_time, true);
  }

  {
    TemplateStringVars vars = {};
    TemplateStringError err = {};
    TemplateStringEvalConditions cond = {};
    vars.current_time = 0;

    cond.eval_time = EVAL_FALL_THROUGH;
    cond.force_eval_on_time = true;
    strcpy(s_output, "hurf");
    bool ret = template_string_evaluate("test string {time_until(5)|format('%uS')}", NULL,
                                        sizeof(s_output), &cond, &vars, &err);
    cl_assert_equal_b(ret, true);
    cl_assert_equal_s(s_output, "hurf");
    cl_assert_equal_i(cond.eval_time, 1);
    cl_assert_equal_b(cond.force_eval_on_time, true);
  }

  {
    TemplateStringVars vars = {};
    TemplateStringError err = {};
    vars.current_time = 0;

    memset(s_output, 'Z', sizeof(s_output));
    bool ret = template_string_evaluate("test string {time_until(5)|format('%uS')}", s_output,
                                        sizeof(s_output), NULL, &vars, &err);
    cl_assert_equal_b(ret, true);
    cl_assert_equal_s(s_output, "test string 5 seconds");
  }

  {
    TemplateStringVars vars = {};
    TemplateStringError err = {};
    vars.current_time = 0;

    bool ret = template_string_evaluate("test string {time_until(5)|format('%uS',)}", NULL, 0,
                                        NULL, &vars, &err);
    cl_assert_equal_b(ret, false);
    cl_assert_equal_i(err.status, TemplateStringErrorStatus_MissingArgument);
    cl_assert_equal_i(err.index_in_string, 40);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Test `time_since` and `time_until` filters

static const struct {
  time_t current_time;
  const char *params;
  intmax_t done_state;
} s_time_since_tests[] = {
  { .current_time = 1234567,
    .params = "1234567)",
    .done_state = 0,
  },
  { .current_time = 1234567,
    .params = "1234560)",
    .done_state = 7,
  },
  { .current_time = 1234567,
    .params = "1234570)",
    .done_state = -3,
  },
  { .current_time = 234567,
    .params = "1234567)",
    .done_state = -1000000,
  },
};

void test_template_string__time_since_until(void) {
  for(size_t i = 0; i < ARRAY_LENGTH(s_time_since_tests); i++) {
#if DEBUG_PRINTING
    printf("current_time: %ld\n", s_time_since_tests[i].current_time);
    printf("parameter: \"%s\"\n", s_time_since_tests[i].params);
    printf("result: %jd\n", s_time_since_tests[i].done_state);
#endif

    prv_state_init();
    s_vars.current_time = s_time_since_tests[i].current_time;
    prv_template_evaluate_filter(&s_state, "time_since", s_time_since_tests[i].params);
    cl_assert_equal_b(s_state.filters_complete, false);
    cl_assert_equal_i(s_state.filter_state, s_time_since_tests[i].done_state);

    prv_state_init();
    s_vars.current_time = s_time_since_tests[i].current_time;
    prv_template_evaluate_filter(&s_state, "time_until", s_time_since_tests[i].params);
    cl_assert_equal_b(s_state.filters_complete, false);
    cl_assert_equal_i(s_state.filter_state, -s_time_since_tests[i].done_state);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Test `format` filter

typedef struct FormatTestData {
  const char *params;
  intmax_t filter_state;
  bool time_was_until;

  const char *expect_str;
  time_t expect_eval_time;
  TemplateStringErrorStatus expect_status;
  size_t expect_index;
} FormatTestData;

static const FormatTestData s_format_tests[] = {
  // Simple text tests
  { "'doo')", 3600, true,
    "doo",
    INT_MAX,
  },

  // Some error testing
  { ">5H'%T')", 1, true,
    "",
    INT_MAX,
    TemplateStringErrorStatus_InvalidTimeUnit,
    3,
  },
  { "'%T',)", 1, true,
    "1",
    1,
    TemplateStringErrorStatus_MissingArgument,
    5,
  },
  { "'%T'fj)", 1, true,
    "1",
    1,
    TemplateStringErrorStatus_InvalidArgumentSeparator,
    4,
  },

  // Basic %T tests
  { "'%T')", 1, true,
    "1",
    1,
  },
  { "'%T')", 60, true,
    "1:00",
    1,
  },
  { "'%T')", 3600, true,
    "1:00:00",
    1,
  },
  { "'%T')", -3666, true,
    "-1:01:06",
    1,
  },

  // Basic %R tests
  { "'%R')", 1, true,
    "0",
    2,
  },
  { "'%R')", 66, true,
    "1",
    7,
  },
  { "'%R')", 3607, true,
    "1:00",
    8,
  },
  { "'%R')", -3666, true,
    "-1:01",
    7,
  },

  // Advanced %T tests
  { "'%0T')", 3666, true,
    "01:01:06",
    1,
  },
  { "'%uT')", 3666, true,
    "1 hour, 1 minute, and 6 seconds",
    1,
  },
  { "'%aT')", 3666, true,
    "1 hr 1 min 6 sec",
    1,
  },
  { "'%auT')", 3666, true,
    "1 hour, 1 minute, and 6 seconds",
    1,
  },
  { "'%0uT')", 3666, true,
    "01 hour, 01 minute, and 06 seconds",
    1,
  },
  { "'%fT')", 129666, true,
    "36:01:06",
    1,
  },
  { "'%T')", 129666, true,
    "12:01:06",
    1,
  },

  // Advanced %R tests
  { "'%0R')", 3666, true,
    "01:01",
    7,
  },
  { "'%uR')", 3666, true,
    "1 hour, and 1 minute",
    7,
  },
  { "'%aR')", 3666, true,
    "1 hr 1 min",
    7,
  },
  { "'%auR')", 3666, true,
    "1 hour, and 1 minute",
    7,
  },
  { "'%0uR')", 3666, true,
    "01 hour, and 01 minute",
    7,
  },
  { "'%fR')", 129666, true,
    "36:01",
    7,
  },
  { "'%R')", 129666, true,
    "12:01",
    7,
  },

  // Predicate tests
  { ">1d12H:'%0ud',<0S:'%-uS since',<60S:'%uS')", 9, true,
    "9 seconds",
    1,
  },
  { ">1d12H:'%0ud',<0S:'%-uS since',<60S:'%0uS')", 129600, true,
    "",
    129601 - 60, // Time left until we hit <60S
    TemplateStringErrorStatus_CantResolve,
    42,
  },
  { ">1d12H:'%0fud',<0S:'%-uS since',<60S:'%uS')", 129601, true,
    "01 day",
    1,
  },
  // 1d12H1S
  { ">=1d14H:'%0fud',<1d13H:'%0fud',>1d12H:'%0fud')", 129601, true,
    "01 day",
    43202, // 12H2S (time=1d-1S)
  },
  // 1d12H
  { ">=1d14H:'%0fud',<1d13H:'%0fud',>1d12H:'%0fud')", 129600, true,
    "01 day",
    43201, // 12H1S (time=1d-1S)
  },
  // 1d13H-100S
  { ">=1d14H:'%0fud',<1d13H:'%0fud',>1d12H:'%0fud')", 133100, true,
    "01 day",
    46701, // 13H-99S (time=1d12H)
  },
  // 1d13H100S
  { ">=1d14H:'%0fud',<1d13H:'%0fud',>1d12H:'%0fud')", 133300, true,
    "01 day",
    101, // time=1d13H-1S
  },
  // 1d14H100S
  { ">=1d14H:'%0fud',<1d13H:'%0fud',>1d12H:'%0fud')", 136900, true,
    "01 day",
    101, // time=1d14H-1S
  },

  // Predicate tests w/ since
  // 1d14H
  { ">=1d14H:'%0fud',<1d13H:'%0fud',>1d12H:'%0fud')", 136800, false,
    "01 day",
    36000, // 2D
  },
  // 1d14H-100S
  { ">=1d14H:'%0fud',<1d13H:'%0fud',>1d12H:'%0fud')", 136700, false,
    "01 day",
    100, // time=1d14H
  },
  // 1d13H
  { ">=1d14H:'%0fud',<1d13H:'%0fud',>1d12H:'%0fud')", 133200, false,
    "01 day",
    3600, // 1H (time=1d14H)
  },
  // 1d13H-10S
  { ">=1d14H:'%0fud',<1d13H:'%0fud',>1d12H:'%0fud')", 133190, false,
    "01 day",
    10, // 10S (time=1d13H)
  },
  // 1d12H
  { ">=1d14H:'%0fud',<1d13H:'%0fud',>1d12H:'%0fud')", 129600, false,
    "01 day",
    1, // (time=1d12H1S)
  },
};

void test_template_string__format(void) {
  for(size_t i = 0; i < ARRAY_LENGTH(s_format_tests); i++) {
    const FormatTestData *test = &s_format_tests[i];

    prv_state_init();
    s_state.filter_state = test->filter_state;
    s_state.time_was_until = test->time_was_until;
    prv_template_evaluate_filter(&s_state, "format", test->params);

    // The filter isn't required to NUL-terminate, so we gotta do it manually sometimes.
    if (*s_state.output != 'Z') {
      cl_assert_equal_i(*s_state.output, '\0');
    } else {
      *s_state.output = '\0';
    }
    size_t err_index = s_state.position - test->params;

#if DEBUG_PRINTING
    printf("parameter: \"%s\"\n", test->params);
    printf("filter_state: %jd %s\n", test->filter_state,
           test->time_was_until ? "until" : "since");
    printf("expect: \"%s\" err %d @ %zu eval@%jd\n", test->expect_str,
           test->expect_status, test->expect_index,
           test->expect_eval_time);
    printf("got   : \"%s\" err %d @ %zu eval@%jd\n", s_output,
           s_error.status, err_index, s_cond.eval_time);
#endif

    if (s_error.status) {
#if DEBUG_PRINTING
      printf("\"%s\"\n", test->params);
      printf("%*s^\n", (int)err_index + 1, "");
      if (s_error.status >= TemplateStringErrorStatusCount) {
        printf("Invalid status code\n");
      } else {
        printf("%s\n", s_error_strings[s_error.status]);
      }
#endif
      cl_assert_equal_i(s_error.status, test->expect_status);
      cl_assert_equal_i(err_index, test->expect_index);
    }

    cl_assert_equal_s(s_output, test->expect_str);
    cl_assert_equal_i(s_cond.eval_time, test->expect_eval_time);

#if DEBUG_PRINTING
    printf("\n");
#endif
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Test pipeline parser

static const struct {
  const char *instr;
  time_t intime;
  const char *expect_str;
  time_t expect_time;
  bool expect_rv;
  TemplateStringErrorStatus expect_status;
  size_t expect_index;
} s_full_tests[] = {
  { "Basicist test~", 1000000000,
    "Basicist test~",
    0,
    true,
  },
  { "\\\\\\", 1000000000,
    "\\",
    EVAL_DEFAULT,
    false,
    TemplateStringErrorStatus_InvalidEscapeCharacter,
    3,
  },
  { "\\e", 1000000000,
    "e",
    0,
    true,
  },
  { "\\\\\\{}", 1000000000,
    "\\{}",
    0,
    true,
  },
  { "\\\\{end()}", 1000000000,
    "\\",
    0,
    true,
  },
  { "\\{end()}", 1000000000,
    "{end()}",
    0,
    true,
  },
  { "Harder test {} bazza", 1000000000,
    "Harder test ",
    EVAL_DEFAULT,
    false,
    TemplateStringErrorStatus_NoResultGenerated,
    13,
  },
  { "Failer {time_until}", 1000000000,
    "Failer ",
    EVAL_DEFAULT,
    false,
    TemplateStringErrorStatus_MissingOpeningParen,
    8,
  },
  { "B {time_until(1)|format('\\\\')}", 0,
    "B \\",
    0,
    true,
  },
  { "B {time_until(1)|format('\\%foo')}", 0,
    "B %foo",
    0,
    true,
  },
  { "B {time_until(1)|format('%%foo')}", 0,
    "B %foo",
    0,
    true,
  },
  { "B {time_until(1)|format('\\'')}", 0,
    "B '",
    0,
    true,
  },
  { "B {time_until(1)|format('\\)}", 0,
    "B )}",
    EVAL_DEFAULT,
    false,
    TemplateStringErrorStatus_MissingClosingQuote,
    28,
  },
  { "B {time_until(1)|format('%T')}", 0,
    "B 1",
    1,
    true,
  },
  { "B {time_until(1)|format('%K')}", 0,
    "B ",
    EVAL_DEFAULT,
    false,
    TemplateStringErrorStatus_InvalidConversionSpecifier,
    26,
  },
  { "B {time_until(1)|format('%f')}", 0,
    "B ",
    EVAL_DEFAULT,
    false,
    TemplateStringErrorStatus_InvalidConversionSpecifier,
    27,
  },
  { "F {time_until(100)}", 1000000000,
    "F ",
    EVAL_DEFAULT,
    false,
    TemplateStringErrorStatus_NoResultGenerated,
    18,
  },
  { "{end()", 1000000000,
    "",
    EVAL_DEFAULT,
    false,
    TemplateStringErrorStatus_MissingClosingBrace,
    6,
  },
  { "{end(hurf", 1000000000,
    "",
    EVAL_DEFAULT,
    false,
    TemplateStringErrorStatus_MissingClosingParen,
    5,
  },
  { "{end}", 1000000000,
    "",
    EVAL_DEFAULT,
    false,
    TemplateStringErrorStatus_MissingOpeningParen,
    1,
  },

  { "B {time_until(129666)|format('%T')}", 0,
    "B 12:01:06",
    1,
    true,
  },

  { "Countdown: {time_until(1)|format(>1d12H:'%0ud',<0S:'%-uS since',<60S:'%uS')} foof",
    10,
    "Countdown: 9 seconds since foof",
    10 + 1,
    true,
  },
  { "Countdown: {time_until(129601)|format(>1d12H:'%0ud',<0S:'%-uS since',<60S:'%0uS')} foof",
    1,
    "Countdown: ",
    1 + 129601 - 60, // Time left until we hit <60S
    false,
    TemplateStringErrorStatus_CantResolve,
    80,
  },

  { "B {time_until(129666)|format('boop)I\\'m a filter')}", 0,
    "B boop)I'm a filter",
    0,
    true,
  },

  { "B {time_until(129666)|format('%T')} AND {time_until(129660)|format('%T')}", 0,
    "B 12:01:06 AND 12:01:00",
    1,
    true,
  },

};

void test_template_string__full_test(void) {
  for(int i = 0; i < ARRAY_LENGTH(s_full_tests); i++) {
    TemplateStringVars vars = {};
    TemplateStringError err = {};
    TemplateStringEvalConditions cond = {};

    vars.current_time = s_full_tests[i].intime;
    cond.eval_time = EVAL_FALL_THROUGH;

    memset(s_output, 'Z', sizeof(s_output));
    bool rv = template_string_evaluate(s_full_tests[i].instr, s_output, sizeof(s_output),
                                       &cond, &vars, &err);
#if DEBUG_PRINTING
    printf("instr: \"%s\"\n", s_full_tests[i].instr);
    printf("outstr: \"%s\"\n", s_output);
    printf("next_eval: %ld\n", cond.eval_time);
    printf("rv: %s\n", rv ? "true" : "false");
    if (!rv) {
      printf("err.status: %X\n", err.status);
      printf("err.index: %zu\n", err.index_in_string);
      printf("\"%s\"\n", s_full_tests[i].instr);
      printf("%*s^\n", (int)err.index_in_string + 1, "");
      if (err.status >= TemplateStringErrorStatusCount) {
        printf("Invalid status code\n");
      } else {
        printf("%s\n", s_error_strings[err.status]);
      }
    }
    printf("\n");
#endif

    cl_assert_equal_s(s_output, s_full_tests[i].expect_str);
    cl_assert_equal_i(rv, s_full_tests[i].expect_rv);
    cl_assert_equal_i(cond.eval_time, s_full_tests[i].expect_time);
    if (cond.eval_time != 0) {
      cl_assert_equal_b(cond.force_eval_on_time, true);
    } else {
      cl_assert_equal_b(cond.force_eval_on_time, false);
    }
    if (!rv) {
      cl_assert_equal_i(err.status, s_full_tests[i].expect_status);
      cl_assert_equal_i(err.index_in_string, s_full_tests[i].expect_index);
    }
  }
}
