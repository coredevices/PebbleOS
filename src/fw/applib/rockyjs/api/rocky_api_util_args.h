/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <util/size.h>

#include <stdint.h>
#include "jerry-api.h"

typedef struct GRect GRect;

typedef enum {
  RockyArgTypeUnsupported = -1,

  RockyArgTypeUInt8,
  RockyArgTypeUInt16,
  RockyArgTypeUInt32,
  RockyArgTypeUInt64,
  RockyArgTypeInt8,
  RockyArgTypeInt16,
  RockyArgTypeInt32,
  RockyArgTypeInt64,
  RockyArgTypeDouble,
  RockyArgTypeFixedS16_3,

  RockyArgTypeBool,
  RockyArgTypeStringArray,
  RockyArgTypeStringMalloc,
  RockyArgTypeGRectPrecise,
  RockyArgTypeGColor,
  RockyArgTypeAngle,

  RockyArgType_Count,
} RockyArgType;

typedef struct {
  void *ptr;
  RockyArgType type;
  union {
    //! Valid when type == RockyArgTypeStringArray
    struct {
      size_t buffer_size;
    } string;
  } options;
} RockyArgBinding;

#define ROCKY_ARG_MAKE(v, binding_type, opts) \
  ((const RockyArgBinding) { .ptr = (v), .type = binding_type, .options = opts })

//! Binds a JS string argument to a C-string for which a buffer is provided by the client code and
//! for which the size of the buffer cannot be derived from the variable type.
//! @note For char[] arrays (with a static size), use ROCKY_ARG() instead.
//! @note If the buffer is too small, nothing will be copied!
#define ROCKY_ARG_STR(char_buf, _buffer_size) \
  ROCKY_ARG_MAKE(char_buf, RockyArgTypeStringArray, { .string = { .buffer_size = _buffer_size }})

#define ROCKY_ARG_ANGLE(var) ROCKY_ARG_MAKE(&(var), RockyArgTypeAngle, {})

#define RockyIfCTypeElse(var, c_type, then, else) \
  __builtin_choose_expr(__builtin_types_compatible_p(__typeof__(var), c_type), (then), (else))

#define ROCKY_ARG(var) \
  RockyIfCTypeElse(var, uint8_t,      ROCKY_ARG_MAKE(&(var), RockyArgTypeUInt8,        {}), \
  RockyIfCTypeElse(var, uint16_t,     ROCKY_ARG_MAKE(&(var), RockyArgTypeUInt16,       {}), \
  RockyIfCTypeElse(var, uint32_t,     ROCKY_ARG_MAKE(&(var), RockyArgTypeUInt32,       {}), \
  RockyIfCTypeElse(var, uint32_t,     ROCKY_ARG_MAKE(&(var), RockyArgTypeUInt32,       {}), \
  RockyIfCTypeElse(var, uint64_t,     ROCKY_ARG_MAKE(&(var), RockyArgTypeUInt64,       {}), \
  RockyIfCTypeElse(var, int8_t,       ROCKY_ARG_MAKE(&(var), RockyArgTypeInt8,         {}), \
  RockyIfCTypeElse(var, int16_t,      ROCKY_ARG_MAKE(&(var), RockyArgTypeInt16,        {}), \
  RockyIfCTypeElse(var, int32_t,      ROCKY_ARG_MAKE(&(var), RockyArgTypeInt32,        {}), \
  RockyIfCTypeElse(var, int32_t,      ROCKY_ARG_MAKE(&(var), RockyArgTypeInt32,        {}), \
  RockyIfCTypeElse(var, int64_t,      ROCKY_ARG_MAKE(&(var), RockyArgTypeInt64,        {}), \
  RockyIfCTypeElse(var, double,       ROCKY_ARG_MAKE(&(var), RockyArgTypeDouble,       {}), \
  RockyIfCTypeElse(var, Fixed_S16_3,  ROCKY_ARG_MAKE(&(var), RockyArgTypeFixedS16_3,   {}), \
  RockyIfCTypeElse(var, bool,         ROCKY_ARG_MAKE(&(var), RockyArgTypeBool,         {}), \
  RockyIfCTypeElse(var, char *,       ROCKY_ARG_MAKE(&(var), RockyArgTypeStringMalloc, {}), \
  RockyIfCTypeElse(var, char[],       ROCKY_ARG_STR(&(var), sizeof(var)), \
  RockyIfCTypeElse(var, GRectPrecise, ROCKY_ARG_MAKE(&(var), RockyArgTypeGRectPrecise, {}), \
  RockyIfCTypeElse(var, GColor,       ROCKY_ARG_MAKE(&(var), RockyArgTypeGColor,       {}), \
    ROCKY_ARG_MAKE(&(var), RockyArgTypeUnsupported, {}))))))))))))))))))

//! Helper that uses arg_bindings to check whether all mandatory arguments are given, of the
//! expected type and the input values are within the limits of the C type. If the checks pass,
//! the function will transform the JerryScript values to the native equivalents and assign them
//! to the storage as specified by the arg_bindings.
//! @return `undefined` on success and an error object in case of a problem.
jerry_value_t rocky_args_assign(const jerry_length_t argc, const jerry_value_t argv[],
                                const RockyArgBinding *arg_bindings, size_t num_arg_bindings);

#define ROCKY_ARGS_ASSIGN_OR_RETURN_ERROR(...) \
  do { \
    const RockyArgBinding bindings[] = { \
      __VA_ARGS__ \
    }; \
    const jerry_value_t error_value = \
        rocky_args_assign(argc, argv, bindings, ARRAY_LENGTH(bindings)); \
    if (jerry_value_has_error_flag(error_value)) { \
      return error_value; \
    } \
  } while (0)
