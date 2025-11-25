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

#ifndef CONFIG_DISABLE_ERROR_BUILTINS

#define ECMA_BUILTINS_INTERNAL
#include "ecma-builtins-internal.h"

#define BUILTIN_INC_HEADER_NAME "ecma-builtin-evalerror.inc.h"
#define BUILTIN_UNDERSCORED_ID eval_error
#include "ecma-builtin-internal-routines-template.inc.h"

/** \addtogroup ecma ECMA
 * @{
 *
 * \addtogroup ecmabuiltins
 * @{
 *
 * \addtogroup evalerror ECMA EvalError object built-in
 * @{
 */

/**
 * Handle calling [[Call]] of built-in EvalError object
 *
 * @return ecma value
 */
ecma_value_t
ecma_builtin_eval_error_dispatch_call (const ecma_value_t *arguments_list_p, /**< arguments list */
                                       ecma_length_t arguments_list_len) /**< number of arguments */
{
  return ecma_builtin_helper_error_dispatch_call (ECMA_ERROR_EVAL, arguments_list_p, arguments_list_len);
} /* ecma_builtin_eval_error_dispatch_call */

/**
 * Handle calling [[Construct]] of built-in EvalError object
 *
 * @return ecma value
 */
ecma_value_t
ecma_builtin_eval_error_dispatch_construct (const ecma_value_t *arguments_list_p, /**< arguments list */
                                            ecma_length_t arguments_list_len) /**< number of arguments */
{
  return ecma_builtin_eval_error_dispatch_call (arguments_list_p, arguments_list_len);
} /* ecma_builtin_eval_error_dispatch_construct */

/**
 * @}
 * @}
 * @}
 */

#endif /* !CONFIG_DISABLE_ERROR_BUILTINS */
