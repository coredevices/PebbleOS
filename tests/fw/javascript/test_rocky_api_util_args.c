/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "applib/rockyjs/api/rocky_api_util_args.h"

#include "applib/rockyjs/api/rocky_api_errors.h"
#include "applib/rockyjs/api/rocky_api_util.h"
#include "applib/rockyjs/pbl_jerry_port.h"

#include "test_jerry_port_common.h"
#include "test_rocky_common.h"

#include <util/size.h>

#include <limits.h>
#include <stdint.h>

// Fakes
#include "fake_pbl_malloc.h"
#include "fake_time.h"

// Stubs
#include "stubs_app_state.h"
#include "stubs_passert.h"
#include "stubs_logging.h"
#include "stubs_serial.h"
#include "stubs_sys_exit.h"

#define JERRY_ARGS_MAKE(...) \
  jerry_value_t argv[] = { \
    __VA_ARGS__ \
  }; \
  jerry_length_t argc = ARRAY_LENGTH(argv);

#define JERRY_ARGS_RELEASE() \
  while(argc--) { \
    jerry_release_value(argv[argc]); \
    argv[argc] = 0; \
  }

#define ROCKY_ARGS_ASSIGN(...) \
  const RockyArgBinding bindings[] = { \
    __VA_ARGS__ \
  }; \
  JS_VAR error_value = \
      rocky_args_assign(argc, argv, bindings, ARRAY_LENGTH(bindings)); \


void test_rocky_api_util_args__initialize(void) {
  rocky_runtime_context_init();
  jerry_init(JERRY_INIT_EMPTY);
}

void test_rocky_api_util_args__cleanup(void) {
  jerry_cleanup();
  rocky_runtime_context_deinit();
  fake_pbl_malloc_check_net_allocs();  // Make sure no memory was leaked
}

void test_rocky_api_util_args__missing_args(void) {
  JERRY_ARGS_MAKE(/* argc == 0 */);

  uint8_t v;
  ROCKY_ARGS_ASSIGN(ROCKY_ARG(v));
  ASSERT_JS_ERROR(error_value, "TypeError: Not enough arguments");
}

void test_rocky_api_util_args__numbers_get_rounded_when_converting_to_integer(void) {
  struct {
    double input;
    int16_t expected_output;
  } cases[] = {
    {
      .input = 0.5,
      .expected_output = 1,
    },
    {
      .input = -0.5,
      .expected_output = -1,
    },
    {
      .input = 0.0,
      .expected_output = 0,
    },
    {
      .input = -0.3,
      .expected_output = 0,
    },
  };
  for (int i = 0; i < ARRAY_LENGTH(cases); ++i) {
    int16_t output = ~0;
    jerry_value_t v = jerry_create_number(cases[i].input);
    JERRY_ARGS_MAKE(v);
    ROCKY_ARGS_ASSIGN(ROCKY_ARG(output));
    cl_assert_equal_i(output, cases[i].expected_output);
    ASSERT_JS_ERROR(error_value, NULL);
    jerry_release_value(v);
  }
}

void test_rocky_api_util_args__numbers(void) {
  uint8_t u8;
  uint16_t u16;
  uint32_t u32;
  uint64_t u64;
  int8_t s8;
  int16_t s16;
  int32_t s32;
  int64_t s64;
  double d;

  typedef struct {
    long double u8;
    long double u16;
    long double u32;
    long double u64;
    long double s8;
    long double s16;
    long double s32;
    long double s64;
    double d;
    char *expected_error_msg;
  } NumbersTestCase;

  enum {
    WithinLowerBounds,
    WithinUpperBounds,
    UnderLowerBounds,
    OverUpperBounds,
  };

  // FIXME: fix limits.h / stdint.h / float.h so we can use the standard defines for these values..
  NumbersTestCase cases[4] = {
    [WithinLowerBounds] = {
      .u8 = 0.0,
      .u16 = 0.0,
      .u32 = 0.0,
      .u64 = 0.0,
      .s8 = -128.0,
      .s16 = -32768.0,
      .s32 = -2147483648.0,
      .s64 = -9223372036854775808.0,
      .d = 2.2250738585072014e-308,
      .expected_error_msg = NULL,
    },
    [WithinUpperBounds] = {
      .u8 = 255.0,
      .u16 = 65535.0,
      .u32 = 4294967295.0,
      .u64 = 9223372036854775807.0,
      .s8 = 127.0,
      .s16 = 32767.0,
      .s32 = 2147483647.0,
      .s64 = 9223372036854775807.0,
      .d = 1.7976931348623157e+308,
      .expected_error_msg = NULL,
    },
  };

  const long double margin = 0.001;
  cases[UnderLowerBounds] = (NumbersTestCase) {
    .u8 = cases[WithinLowerBounds].u8 - margin,
    .u16 = cases[WithinLowerBounds].u16 - margin,
    .u32 = cases[WithinLowerBounds].u32 - margin,
    .u64 = cases[WithinLowerBounds].u64 - margin,
    .s8 = cases[WithinLowerBounds].s8 - margin,
    .s16 = cases[WithinLowerBounds].s16 - margin,
    .s32 = cases[WithinLowerBounds].s32 - margin,
    .s64 = cases[WithinLowerBounds].s64 - margin,
    .d = cases[WithinLowerBounds].d - margin,
    .expected_error_msg =
        "TypeError: Argument at index 0 is invalid: Value out of bounds for native type",
  };
  cases[OverUpperBounds] = (NumbersTestCase) {
    .u8 = cases[WithinUpperBounds].u8 + margin,
    .u16 = cases[WithinUpperBounds].u16 + margin,
    .u32 = cases[WithinUpperBounds].u32 + margin,
    .u64 = cases[WithinUpperBounds].u64 + margin,
    .s8 = cases[WithinUpperBounds].s8 + margin,
    .s16 = cases[WithinUpperBounds].s16 + margin,
    .s32 = cases[WithinUpperBounds].s32 + margin,
    .s64 = cases[WithinUpperBounds].s64 + margin,
    .d = cases[WithinUpperBounds].d - margin,
    .expected_error_msg =
        "TypeError: Argument at index 0 is invalid: Value out of bounds for native type",
  };

  for (int i = 0; i < ARRAY_LENGTH(cases); ++i) {
    NumbersTestCase *c = &cases[i];

    // Initialize to something that's not the expected value:
    u8 =  1 + (uint8_t)c->u8;
    u16 = 1 + (uint16_t)c->u16;
    u32 = 1 + (uint16_t)c->u32;
    u64 = 1 + (uint16_t)c->u64;
    s8 =  1 + (int8_t)c->s8;
    s16 = 1 + (int16_t)c->s16;
    s32 = 1 + (int16_t)c->s32;
    s64 = 1 + (int16_t)c->s64;
    d = 1 + (double)c->d;

    JERRY_ARGS_MAKE(
        jerry_create_number(c->u8),
        jerry_create_number(c->u16),
        jerry_create_number(c->u32),
        jerry_create_number(c->u64),
        jerry_create_number(c->s8),
        jerry_create_number(c->s16),
        jerry_create_number(c->s32),
        jerry_create_number(c->s64),
        jerry_create_number(c->d),
    );
    ROCKY_ARGS_ASSIGN(
        ROCKY_ARG(u8),
        ROCKY_ARG(u16),
        ROCKY_ARG(u32),
        ROCKY_ARG(u64),
        ROCKY_ARG(s8),
        ROCKY_ARG(s16),
        ROCKY_ARG(s32),
        ROCKY_ARG(s64),
        ROCKY_ARG(d),
    );
    ASSERT_JS_ERROR(error_value, c->expected_error_msg);
    if (!c->expected_error_msg) {
      cl_assert_equal_i(u8, (uint8_t)c->u8);
      cl_assert_equal_i(u16, (uint16_t)c->u16);
      cl_assert_equal_i(u32, (uint32_t)c->u32);
      cl_assert_equal_i(u64, (uint64_t)c->u64);
      cl_assert_equal_i(s8, (int8_t)c->s8);
      cl_assert_equal_i(s16, (int16_t)c->s16);
      cl_assert_equal_i(s32, (int32_t)c->s32);
      cl_assert_equal_i(s64, (int64_t)c->s64);
      cl_assert_equal_d(d, (double)c->d);
    }
    JERRY_ARGS_RELEASE();
  }
}

void test_rocky_api_util_args__number_type_mismatch(void) {
  jerry_value_t mismatch_args[] = {
    jerry_create_null(),
    jerry_create_string((const jerry_char_t *)"one"),
    jerry_create_string((const jerry_char_t *)"1"),
    jerry_create_array(1),
    jerry_create_boolean(true),
    jerry_create_object(),
  };

  for (int i = 0; i < ARRAY_LENGTH(mismatch_args); ++i) {
    jerry_value_t arg = mismatch_args[i];
    JERRY_ARGS_MAKE(arg);

    for (RockyArgType type = RockyArgTypeUInt8; type <= RockyArgTypeDouble; ++type) {
      uint8_t buffer_untouched[8];  // The type check fails, so nothing is supposed to be written.
      ROCKY_ARGS_ASSIGN(
         ROCKY_ARG_MAKE(buffer_untouched, type, {}),
      );
      ASSERT_JS_ERROR(error_value, "TypeError: Argument at index 0 is not a Number");
    }

    jerry_release_value(arg);
    mismatch_args[i] = 0;
  }
}

static jerry_value_t prv_dummy(const jerry_value_t function_obj_p,
                               const jerry_value_t this_val,
                               const jerry_value_t args_p[],
                               const jerry_length_t args_count) {
  return 0;
}

void test_rocky_api_util_args__boolean(void) {
  // No API to create NaN :(
  char *nan_script = "Number.NaN";
  jerry_value_t nan = jerry_eval((jerry_char_t *)nan_script,
                                strlen(nan_script),
                                false /* is_strict */);

  struct {
    jerry_value_t input;
    bool expected_output;
  } cases[] = {
    // Falsy: false, 0, "", null, undefined, and NaN:
    {
      .input = jerry_create_boolean(false),
      .expected_output = false,
    },
    {
      .input = jerry_create_number(0.0),
      .expected_output = false,
    },
    {
      .input = jerry_create_string((const jerry_char_t *)""),
      .expected_output = false,
    },
    {
      .input = jerry_create_null(),
      .expected_output = false,
    },
    {
      .input = jerry_create_undefined(),
      .expected_output = false,
    },
    {
      .input = nan,
      .expected_output = false,
    },
    // Truthy values:
    {
      .input = jerry_create_boolean(true),
      .expected_output = true,
    },
    {
      .input = jerry_create_number(1.0),
      .expected_output = true,
    },
    {
      .input = jerry_create_string((const jerry_char_t *)" "),
      .expected_output = true,
    },
    {
      .input = jerry_create_array(0),
      .expected_output = true,
    },
    {
      .input = jerry_create_object(),
      .expected_output = true,
    },
    {
      .input = jerry_create_external_function(prv_dummy),
      .expected_output = true,
    },
  };

  for (int i = 0; i < ARRAY_LENGTH(cases); ++i) {
    bool output = !cases[i].expected_output;

    jerry_value_t input = cases[i].input;
    JERRY_ARGS_MAKE(input);

    ROCKY_ARGS_ASSIGN(ROCKY_ARG(output));
    cl_assert_equal_b(output, cases[i].expected_output);
    ASSERT_JS_ERROR(error_value, NULL /* Never errors out! */);

    jerry_release_value(input);
    cases[i].input = 0;
  }
}

void test_rocky_api_util_args__string(void) {
  struct {
    jerry_value_t input;
    char *expected_output;
  } cases[] = {
    {
      .input = jerry_create_boolean(false),
      .expected_output = "false",
    },
    {
      .input = jerry_create_number(0.0),
      .expected_output = "0",
    },
    {
      .input = jerry_create_number(1.234e+60),
      .expected_output = "1.234e+60",
    },
    {
      .input = jerry_create_string((const jerry_char_t *)""),
      .expected_output = "",
    },
    {
      .input = jerry_create_string((const jerry_char_t *)"js"),
      .expected_output = "js",
    },
    {
      .input = jerry_create_null(),
      .expected_output = "null",
    },
    {
      .input = jerry_create_undefined(),
      .expected_output = "undefined",
    },
    {
      .input = jerry_create_array(0),
      .expected_output = "", // Kinda weird?
    },
    {
      .input = jerry_create_object(),
      .expected_output = "[object Object]",
    },
  };

  for (int i = 0; i < ARRAY_LENGTH(cases); ++i) {
    jerry_value_t input = cases[i].input;
    JERRY_ARGS_MAKE(input);

    // Exercise ROCKY_ARG (automatic binding creation, defaults to malloc'd string for char *):
    {
      char *output = NULL;
      ROCKY_ARGS_ASSIGN(ROCKY_ARG(output));
      cl_assert_equal_s(output, cases[i].expected_output);
      ASSERT_JS_ERROR(error_value, NULL /* Never errors out! */);
      task_free(output);
    }

    // Exercise ROCKY_ARG (automatic binding creation, defaults to copy/no-malloc for char[]):
    {
      char output[16];
      ROCKY_ARGS_ASSIGN(ROCKY_ARG(output));
      cl_assert_equal_s(output, cases[i].expected_output);
      ASSERT_JS_ERROR(error_value, NULL /* Never errors out! */);
    }

    // Exercise ROCKY_ARG_STR (no malloc, explicit binding creation):
    {
      char output[16];
      ROCKY_ARGS_ASSIGN(ROCKY_ARG_STR(output, sizeof(output)));
      cl_assert_equal_s(output, cases[i].expected_output);
      ASSERT_JS_ERROR(error_value, NULL /* Never errors out! */);
    }

    // Exercise ROCKY_ARG_STR (too small buffer provided):
    {
      char output[1] = {0xff};
      ROCKY_ARGS_ASSIGN(ROCKY_ARG_STR(output, 0));
      cl_assert_equal_s(output, "");
      ASSERT_JS_ERROR(error_value, NULL /* Never errors out! */);
    }

    jerry_release_value(input);
    cases[i].input = 0;
  }
}

#define PP(_x, _y, _w, _h) { \
  .origin.x.raw_value = (_x), \
  .origin.y.raw_value = (_y), \
  .size.w.raw_value = (_w), \
  .size.h.raw_value = (_h), \
  }

void test_rocky_api_util_args__grect_precise(void) {
  struct {
    jerry_value_t argv[5];
    size_t argc;
    GRectPrecise expected_output;
    char *error_msg;
  } cases[] = {
    {
      .argv = {
        [0] = jerry_create_number(0.0),
        [1] = jerry_create_number(0.0),
        [2] = jerry_create_number(0.0),
        [3] = jerry_create_number(0.0),
      },
      .argc = 4,
      .expected_output = PP(0, 0, 0, 0),
    },
    {
      .argv = {
        [0] = jerry_create_number(-0.5),
        [1] = jerry_create_number(-0.2),
        [2] = jerry_create_number(0.3),
        [3] = jerry_create_number(0.5),
      },
      .argc = 4,
      .expected_output = PP(-4, -2, 2, 4),
    },
    {
      .argv = {
        [0] = jerry_create_number(-4096.0),
        [1] = jerry_create_number(-4096.0),
        [2] = jerry_create_number(4095.875),
        [3] = jerry_create_number(4095.875),
      },
      .argc = 4,
      .expected_output = PP(-32768, -32768, 32767, 32767),
    },
    {
      .argv = {
        [0] = jerry_create_number(0),
        [1] = jerry_create_number(0),
        [2] = jerry_create_number(0),
        [3] = jerry_create_number(4096.0),
      },
      .argc = 4,
      .error_msg = "TypeError: Argument at index 3 is invalid: Value out of bounds for native type",
    },
    {
      .argv = {
        [0] = jerry_create_number(0),
        [1] = jerry_create_number(0),
        [2] = jerry_create_number(0),
      },
      .argc = 3,
      .error_msg = "TypeError: Not enough arguments",
    },
    {
      .argv = {
        [0] = jerry_create_number(0),
        [1] = jerry_create_number(0),
        [2] = jerry_create_number(0),
        [3] = jerry_create_null(),
      },
      .argc = 4,
      .error_msg = "TypeError: Argument at index 3 is not a Number",
    },
    {
      .argv = {
        [0] = jerry_create_number(0),
        [1] = jerry_create_number(0),
        [2] = jerry_create_number(0),
        [3] = jerry_create_string((const jerry_char_t *)"123"),
      },
      .argc = 4,
      .error_msg = "TypeError: Argument at index 3 is not a Number",
    },
  };

  for (int i = 0; i < ARRAY_LENGTH(cases); ++i) {
    jerry_value_t *argv = cases[i].argv;
    jerry_length_t argc = cases[i].argc;

    GRectPrecise output;
    memset(&output, 0x55, sizeof(output));

    ROCKY_ARGS_ASSIGN(ROCKY_ARG(output));
    ASSERT_JS_ERROR(error_value, cases[i].error_msg);
    if (!cases[i].error_msg) {
      cl_assert_equal_i(output.origin.x.raw_value, cases[i].expected_output.origin.x.raw_value);
      cl_assert_equal_i(output.origin.y.raw_value, cases[i].expected_output.origin.y.raw_value);
      cl_assert_equal_i(output.size.w.raw_value, cases[i].expected_output.size.w.raw_value);
      cl_assert_equal_i(output.size.h.raw_value, cases[i].expected_output.size.h.raw_value);
    }

    for (uint32_t j = 0; j < argc; ++j) {
      jerry_release_value(argv[j]);
    }
  }
}

void test_rocky_api_util_args__gcolor(void) {
  const char *type_error_msg =
      "TypeError: Argument at index 0 is not a String ('color name' or '#hex') or Number";
  const char *invalid_value_msg =
      "TypeError: Argument at index 0 is invalid: " \
      "Expecting String ('color name' or '#hex') or Number";
  struct {
    jerry_value_t input;
    GColor expected_output;
    const char *error_msg;
  } cases[] = {
    {
      .input = jerry_create_number(0.0),
      .expected_output = {
        .argb = 0,
      },
    },
    {
      .input = jerry_create_number(GColorJaegerGreenARGB8),
      .expected_output = {
        .argb = GColorJaegerGreenARGB8,
      }
    },
    {
      .input = jerry_create_string((const jerry_char_t *)"red"),
      .expected_output = {
        .r = 0b11,
        .g = 0,
        .b = 0,
        .a = 0b11,
      }
    },
    {
      .input = jerry_create_string((const jerry_char_t *)"unknown-color"),
      .error_msg = invalid_value_msg,
    },
    {
      .input = jerry_create_null(),
      .error_msg = type_error_msg,
    },
  };

  for (int i = 0; i < ARRAY_LENGTH(cases); ++i) {
    jerry_value_t input = cases[i].input;
    JERRY_ARGS_MAKE(input);

    GColor output;
    memset(&output, 0x55, sizeof(output));

    ROCKY_ARGS_ASSIGN(ROCKY_ARG(output));
    ASSERT_JS_ERROR(error_value, cases[i].error_msg);
    if (!cases[i].error_msg) {
      cl_assert_equal_i(output.a, cases[i].expected_output.a);
      cl_assert_equal_i(output.r, cases[i].expected_output.r);
      cl_assert_equal_i(output.g, cases[i].expected_output.g);
      cl_assert_equal_i(output.b, cases[i].expected_output.b);
    }

    jerry_release_value(cases[i].input);
    cases[i].input = 0;
  }
}
