/* SPDX-FileCopyrightText: 2014-2015 Samsung Electronics Co., Ltd. */
/* SPDX-License-Identifier: Apache-2.0 */

#include "ecma-alloc.h"
#include "ecma-builtins.h"
#include "ecma-conversion.h"
#include "ecma-exceptions.h"
#include "ecma-gc.h"
#include "ecma-globals.h"
#include "ecma-helpers.h"
#include "ecma-builtin-helpers.h"
#include "ecma-objects.h"
#include "ecma-try-catch-macro.h"
#include "jrt.h"

#define ECMA_BUILTINS_INTERNAL
#include "ecma-builtins-internal.h"

#define BUILTIN_INC_HEADER_NAME "ecma-builtin-error.inc.h"
#define BUILTIN_UNDERSCORED_ID error
#include "ecma-builtin-internal-routines-template.inc.h"

/** \addtogroup ecma ECMA
 * @{
 *
 * \addtogroup ecmabuiltins
 * @{
 *
 * \addtogroup error ECMA Error object built-in
 * @{
 */

/**
 * Handle calling [[Call]] of built-in Error object
 *
 * @return ecma value
 */
ecma_value_t
ecma_builtin_error_dispatch_call (const ecma_value_t *arguments_list_p, /**< arguments list */
                                  ecma_length_t arguments_list_len) /**< number of arguments */
{
  return ecma_builtin_helper_error_dispatch_call (ECMA_ERROR_COMMON, arguments_list_p, arguments_list_len);
} /* ecma_builtin_error_dispatch_call */

/**
 * Handle calling [[Construct]] of built-in Error object
 *
 * @return ecma value
 */
ecma_value_t
ecma_builtin_error_dispatch_construct (const ecma_value_t *arguments_list_p, /**< arguments list */
                                       ecma_length_t arguments_list_len) /**< number of arguments */
{
  return ecma_builtin_error_dispatch_call (arguments_list_p, arguments_list_len);
} /* ecma_builtin_error_dispatch_construct */

/**
 * @}
 * @}
 * @}
 */
