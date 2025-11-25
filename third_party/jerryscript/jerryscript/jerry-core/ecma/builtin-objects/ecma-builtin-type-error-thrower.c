/* SPDX-FileCopyrightText: 2014-2015 Samsung Electronics Co., Ltd. */
/* SPDX-License-Identifier: Apache-2.0 */

#include "ecma-alloc.h"
#include "ecma-builtins.h"
#include "ecma-conversion.h"
#include "ecma-exceptions.h"
#include "ecma-gc.h"
#include "ecma-globals.h"
#include "ecma-helpers.h"
#include "ecma-objects.h"
#include "ecma-try-catch-macro.h"
#include "jrt.h"

#define ECMA_BUILTINS_INTERNAL
#include "ecma-builtins-internal.h"

#define BUILTIN_INC_HEADER_NAME "ecma-builtin-type-error-thrower.inc.h"
#define BUILTIN_UNDERSCORED_ID type_error_thrower
#include "ecma-builtin-internal-routines-template.inc.h"

/** \addtogroup ecma ECMA
 * @{
 *
 * \addtogroup ecmabuiltins
 * @{
 *
 * \addtogroup type_error_thrower ECMA [[ThrowTypeError]] object built-in
 * @{
 */

/**
 * Handle calling [[Call]] of built-in [[ThrowTypeError]] object
 *
 * See also:
 *          ECMA-262 v5, 13.2.3
 *
 * @return ecma value
 */
ecma_value_t
ecma_builtin_type_error_thrower_dispatch_call (const ecma_value_t *arguments_list_p, /**< arguments list */
                                               ecma_length_t arguments_list_len) /**< number of arguments */
{
  JERRY_ASSERT (arguments_list_len == 0 || arguments_list_p != NULL);

  /* The object should throw TypeError */
  return ecma_raise_type_error (ECMA_ERR_MSG (""));
} /* ecma_builtin_type_error_thrower_dispatch_call */

/**
 * Handle calling [[Construct]] of built-in [[ThrowTypeError]] object
 *
 * See also:
 *          ECMA-262 v5, 13.2.3
 *
 * @return ecma value
 */
ecma_value_t
ecma_builtin_type_error_thrower_dispatch_construct (const ecma_value_t *arguments_list_p, /**< arguments list */
                                                    ecma_length_t arguments_list_len) /**< number of arguments */
{
  JERRY_ASSERT (arguments_list_len == 0 || arguments_list_p != NULL);

  /* The object is not a constructor */
  return ecma_raise_type_error (ECMA_ERR_MSG (""));
} /* ecma_builtin_type_error_thrower_dispatch_construct */

/**
 * @}
 * @}
 * @}
 */
