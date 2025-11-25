/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "rocky_api_util_args.h"

#include "rocky_api_errors.h"
#include "rocky_api_graphics_color.h"
#include "rocky_api_util.h"

#include "applib/graphics/gtypes.h"
#include "system/passert.h"

#include <math.h>
#include <util/math.h>

// from lit-magic-string.inc.h
static const char ECMA_STRING_TYPE_NUMBER[] = "Number";
static const char COLOR_TYPES[] = "String ('color name' or '#hex') or Number";
static const char COLOR_ERROR_MSG[] = "Expecting String ('color name' or '#hex') or Number";

typedef struct {
  const char *expected_type_name;
  int arg_offset;
} RockyArgTypeCheckError;

typedef struct {
  const char *error_msg;
  int arg_offset;
} RockyArgValueCheckError;

typedef struct {
  bool (* check_value_and_assign)(const RockyArgBinding *binding, const jerry_value_t argv[],
                  RockyArgValueCheckError *val_error_out);
  bool (* check_type)(const jerry_value_t argv[], RockyArgTypeCheckError *type_error_out);
  uint8_t expected_num_args;
} RockyArgAssignImp;

static bool prv_check_value_number_within_bounds(RockyArgType type, const double *val,
                                                 RockyArgValueCheckError *value_error_out) {
  const struct {
    double min;
    double max;
  } bounds[] = {
    [RockyArgTypeUInt8] = {0.0, 255.0},
    [RockyArgTypeUInt16] = {0.0, 65535.0},
    [RockyArgTypeUInt32] = {0.0, 4294967295.0},
    [RockyArgTypeUInt64] = {0.0, 9223372036854775807.0},
    [RockyArgTypeInt8] = {-128.0, 127.0},
    [RockyArgTypeInt16] = {-32768.0, 32767.0},
    [RockyArgTypeInt32] = {-2147483648.0, 2147483647.0},
    [RockyArgTypeInt64] = {-9223372036854775808.0, 9223372036854775807.0},
    [RockyArgTypeDouble] = {-1.7976931348623157e+308, 1.7976931348623157e+308},
    [RockyArgTypeFixedS16_3] = {-32768.0 / FIXED_S16_3_FACTOR, 32767.0 / FIXED_S16_3_FACTOR},
  };
  const bool is_within_bounds = WITHIN(*val, bounds[type].min, bounds[type].max);
  if (!is_within_bounds) {
    *value_error_out = (RockyArgValueCheckError) {
      .error_msg = "Value out of bounds for native type",
      .arg_offset = 0,
    };
  }
  return is_within_bounds;
}

static Fixed_S16_3 prv_fixed_s3_from_double(double d) {
  return (Fixed_S16_3 ){.raw_value = round(d * FIXED_S16_3_FACTOR)};
}

static bool prv_assign_number(const RockyArgBinding *binding, const jerry_value_t argv[],
                              RockyArgValueCheckError *value_error_out) {
  double val = jerry_get_number_value(argv[0]);

  if (!prv_check_value_number_within_bounds(binding->type, &val, value_error_out)) {
    return false;
  }

  if (binding->type != RockyArgTypeDouble && binding->type != RockyArgTypeFixedS16_3) {
    val = round(val);
  }

  void *const dest_ptr = binding->ptr;
  switch (binding->type) {
    case RockyArgTypeUInt8:
      *((uint8_t *)dest_ptr) = val;
      return true;
    case RockyArgTypeUInt16:
      *((uint16_t *)dest_ptr) = val;
      return true;
    case RockyArgTypeUInt32:
      *((uint32_t *)dest_ptr) = val;
      return true;
    case RockyArgTypeUInt64:
      *((uint64_t *)dest_ptr) = val;
      return true;
    case RockyArgTypeInt8:
      *((int8_t *)dest_ptr) = val;
      return true;
    case RockyArgTypeInt16:
      *((int16_t *)dest_ptr) = val;
      return true;
    case RockyArgTypeInt32:
      *((int32_t *)dest_ptr) = val;
      return true;
    case RockyArgTypeInt64:
      *((int64_t *)dest_ptr) = val;
      return true;
    case RockyArgTypeDouble:
      *((double *)dest_ptr) = val;
      return true;
    case RockyArgTypeFixedS16_3:
      *((Fixed_S16_3 *)dest_ptr) = prv_fixed_s3_from_double(val);
      return true;
    default:
      WTF;
      return false;
  }
}

static bool prv_assign_bool(const RockyArgBinding *binding, const jerry_value_t argv[],
                            RockyArgValueCheckError *value_error_out) {
  *((bool *)binding->ptr) = jerry_value_to_boolean(argv[0]);
  return true;
}

static void prv_convert_to_string_and_apply(const jerry_value_t val,
                                            void (*apply)(const jerry_value_t,
                                                          const RockyArgBinding *),
                                            const RockyArgBinding *binding) {
  jerry_value_t str_val;
  bool should_release = false;
  if (jerry_value_is_string(val)) {
    str_val = val;
  } else {
    str_val = jerry_value_to_string(val);
    should_release = true;
  }

  apply(str_val, binding);

  if (should_release) {
    jerry_release_value(str_val);
  }
}

static void prv_malloc_and_assign_string_applier(const jerry_value_t str,
                                                 const RockyArgBinding *binding) {
  *((char **)binding->ptr) = rocky_string_alloc_and_copy(str);
}

static bool prv_malloc_and_assign_string(const RockyArgBinding *binding, const jerry_value_t argv[],
                                         RockyArgValueCheckError *value_error_out) {
  prv_convert_to_string_and_apply(argv[0], prv_malloc_and_assign_string_applier, binding);
  return true;
}

static void prv_copy_string_applier(const jerry_value_t str, const RockyArgBinding *binding) {
  const size_t buffer_size = binding->options.string.buffer_size;
  const size_t copied_size = jerry_string_to_utf8_char_buffer(str, (jerry_char_t *)binding->ptr,
                                                              buffer_size);
  ((char *)binding->ptr)[copied_size] = '\0';
}

static bool prv_copy_string_no_malloc(const RockyArgBinding *binding, const jerry_value_t argv[],
                                      RockyArgValueCheckError *value_error_out) {
  prv_convert_to_string_and_apply(argv[0], prv_copy_string_applier, binding);
  return true;
}

static bool prv_convert_and_assign_angle(const RockyArgBinding *binding, const jerry_value_t argv[],
                                       RockyArgValueCheckError *value_error_out) {
  *((double *)binding->ptr) = jerry_get_angle_value(argv[0]);
  return true;
}

static bool prv_assign_grect_precise(const RockyArgBinding *binding, const jerry_value_t argv[],
                             RockyArgValueCheckError *value_error_out) {
  Fixed_S16_3 v[4];
  for (int i = 0; i < 4; ++i) {
    const double d = jerry_get_number_value(argv[i]);
    if (!prv_check_value_number_within_bounds(RockyArgTypeFixedS16_3, &d, value_error_out)) {
      value_error_out->arg_offset = i;
      return false;
    }
    v[i] = prv_fixed_s3_from_double(d);
  }
  *((GRectPrecise *)binding->ptr) = (GRectPrecise) {
    .origin.x = v[0],
    .origin.y = v[1],
    .size.w = v[2],
    .size.h = v[3],
  };
  return true;
}

static bool prv_convert_and_assign_gcolor(const RockyArgBinding *binding,
                                          const jerry_value_t argv[],
                                          RockyArgValueCheckError *value_error_out) {
  if (rocky_api_graphics_color_from_value(argv[0], (GColor *)binding->ptr)) {
    return true;
  }
  *value_error_out = (RockyArgValueCheckError) {
    .error_msg = COLOR_ERROR_MSG,
    .arg_offset = 0,
  };
  return false;
}


static bool prv_check_type_is_number(const jerry_value_t argv[],
                                     RockyArgTypeCheckError *type_error_out) {
  if (jerry_value_is_number(argv[0])) {
    return true;
  };
  *type_error_out = (RockyArgTypeCheckError) {
    .expected_type_name = ECMA_STRING_TYPE_NUMBER,
    .arg_offset = 0,
  };
  return false;
}

static bool prv_check_type_any(const jerry_value_t argv[],
                               RockyArgTypeCheckError *type_error_out) {
  return true;
}

static bool prv_check_4x_number(const jerry_value_t argv[],
                                RockyArgTypeCheckError *type_error_out) {
  for (uint32_t i = 0; i < 4; ++i) {
    if (!prv_check_type_is_number(&argv[i], type_error_out)) {
      type_error_out->arg_offset = i;
      return false;
    }
  }
  return true;
}

static bool prv_check_color_type(const jerry_value_t argv[],
                                 RockyArgTypeCheckError *type_error_out) {
  if (jerry_value_is_number(argv[0]) || jerry_value_is_string(argv[0])) {
    return true;
  }
  *type_error_out = (RockyArgTypeCheckError) {
    .expected_type_name = COLOR_TYPES,
    .arg_offset = 0,
  };
  return false;
}

static void prv_init_arg_assign_imp(RockyArgType arg_type, RockyArgAssignImp *imp_out) {
  // Defaults:
  imp_out->expected_num_args = 1;

  switch (arg_type) {
    case RockyArgTypeUInt8 ... RockyArgTypeFixedS16_3:
      imp_out->check_type = prv_check_type_is_number;
      imp_out->check_value_and_assign = prv_assign_number;
      break;

    case RockyArgTypeBool:
      imp_out->check_type = prv_check_type_any;
      imp_out->check_value_and_assign = prv_assign_bool;
      break;

    case RockyArgTypeStringMalloc:
      imp_out->check_type = prv_check_type_any;
      imp_out->check_value_and_assign = prv_malloc_and_assign_string;
      break;

    case RockyArgTypeStringArray:
      imp_out->check_type = prv_check_type_any;
      imp_out->check_value_and_assign = prv_copy_string_no_malloc;
      break;

    case RockyArgTypeAngle:
      imp_out->check_type = prv_check_type_is_number;
      imp_out->check_value_and_assign = prv_convert_and_assign_angle;
      break;

    case RockyArgTypeGRectPrecise:
      imp_out->expected_num_args = 4;
      imp_out->check_type = prv_check_4x_number;
      imp_out->check_value_and_assign = prv_assign_grect_precise;
      break;

    case RockyArgTypeGColor:
      imp_out->check_type = prv_check_color_type;
      imp_out->check_value_and_assign = prv_convert_and_assign_gcolor;
      break;

    default:
      WTF;
      break;
  }
}

jerry_value_t rocky_args_assign(const jerry_length_t argc, const jerry_value_t argv[],
                                const RockyArgBinding *arg_bindings, size_t num_arg_bindings) {
  for (uint32_t i = 0; i < num_arg_bindings; ++i) {
    const RockyArgBinding *binding = &arg_bindings[i];

    RockyArgAssignImp imp;
    prv_init_arg_assign_imp(binding->type, &imp);

    // Check number of arguments:
    // TODO: PBL-40644: support optional bindings
    if (i + imp.expected_num_args > argc) {
      return rocky_error_arguments_missing();
    }

    // Type check:
    RockyArgTypeCheckError type_error;
    if (!imp.check_type(&argv[i], &type_error)) {
      return rocky_error_unexpected_type(i + type_error.arg_offset,
                                         type_error.expected_type_name);
    }

    // Check value, transform & assign:
    RockyArgValueCheckError value_error;
    if (!imp.check_value_and_assign(&arg_bindings[i], &argv[i], &value_error)) {
      return rocky_error_argument_invalid_at_index(i + value_error.arg_offset,
                                                   value_error.error_msg);
    }
  }
  // Just ignore any surplus args

  return jerry_create_undefined();
}
