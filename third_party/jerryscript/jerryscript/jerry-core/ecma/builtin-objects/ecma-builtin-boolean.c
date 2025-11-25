/* SPDX-FileCopyrightText: 2014-2015 Samsung Electronics Co., Ltd. */
/* SPDX-License-Identifier: Apache-2.0 */

#include "ecma-alloc.h"
#include "ecma-boolean-object.h"
#include "ecma-builtins.h"
#include "ecma-conversion.h"
#include "ecma-exceptions.h"
#include "ecma-gc.h"
#include "ecma-globals.h"
#include "ecma-helpers.h"
#include "ecma-objects.h"
#include "ecma-try-catch-macro.h"
#include "jrt.h"

#ifndef CONFIG_DISABLE_BOOLEAN_BUILTIN

#define ECMA_BUILTINS_INTERNAL
#include "ecma-builtins-internal.h"

#define BUILTIN_INC_HEADER_NAME "ecma-builtin-boolean.inc.h"
#define BUILTIN_UNDERSCORED_ID boolean
#include "ecma-builtin-internal-routines-template.inc.h"

/** \addtogroup ecma ECMA
 * @{
 *
 * \addtogroup ecmabuiltins
 * @{
 *
 * \addtogroup boolean ECMA Boolean object built-in
 * @{
 */

/**
 * Handle calling [[Call]] of built-in Boolean object
 *
 * @return ecma value
 */
ecma_value_t
ecma_builtin_boolean_dispatch_call (const ecma_value_t *arguments_list_p, /**< arguments list */
                                    ecma_length_t arguments_list_len) /**< number of arguments */
{
  JERRY_ASSERT (arguments_list_len == 0 || arguments_list_p != NULL);

  ecma_value_t arg_value;

  if (arguments_list_len == 0)
  {
    arg_value = ecma_make_simple_value (ECMA_SIMPLE_VALUE_UNDEFINED);
  }
  else
  {
    arg_value = arguments_list_p[0];
  }

  return ecma_make_simple_value (ecma_op_to_boolean (arg_value) ? ECMA_SIMPLE_VALUE_TRUE
                                                                : ECMA_SIMPLE_VALUE_FALSE);
} /* ecma_builtin_boolean_dispatch_call */

/**
 * Handle calling [[Construct]] of built-in Boolean object
 *
 * @return ecma value
 */
ecma_value_t
ecma_builtin_boolean_dispatch_construct (const ecma_value_t *arguments_list_p, /**< arguments list */
                                         ecma_length_t arguments_list_len) /**< number of arguments */
{
  JERRY_ASSERT (arguments_list_len == 0 || arguments_list_p != NULL);

  if (arguments_list_len == 0)
  {
    return ecma_op_create_boolean_object (ecma_make_simple_value (ECMA_SIMPLE_VALUE_FALSE));
  }
  else
  {
    return ecma_op_create_boolean_object (arguments_list_p[0]);
  }
} /* ecma_builtin_boolean_dispatch_construct */

/**
 * @}
 * @}
 * @}
 */

#endif /* !CONFIG_DISABLE_BOOLEAN_BUILTIN */
