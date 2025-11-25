/* SPDX-FileCopyrightText: 2014-2016 Samsung Electronics Co., Ltd. */
/* SPDX-License-Identifier: Apache-2.0 */

#ifndef ECMA_EXCEPTIONS_H
#define ECMA_EXCEPTIONS_H

#include "ecma-globals.h"
#include "jrt.h"

/** \addtogroup ecma ECMA
 * @{
 *
 * \addtogroup exceptions Exceptions
 * @{
 */

#ifdef JERRY_ENABLE_ERROR_MESSAGES
#define ECMA_ERR_MSG(msg) msg
#else /* !JERRY_ENABLE_ERROR_MESSAGES */
#define ECMA_ERR_MSG(msg) NULL
#endif /* JERRY_ENABLE_ERROR_MESSAGES */

/**
 * Native errors.
 *
 * See also: 15.11.1, 15.11.6
 */
typedef enum
{
  ECMA_ERROR_COMMON,    /**< Error */
  ECMA_ERROR_EVAL,      /**< EvalError */
  ECMA_ERROR_RANGE,     /**< RangeError */
  ECMA_ERROR_REFERENCE, /**< ReferenceError */
  ECMA_ERROR_SYNTAX,    /**< SyntaxError */
  ECMA_ERROR_TYPE,      /**< TypeError */
  ECMA_ERROR_URI        /**< URIError */
} ecma_standard_error_t;

extern ecma_object_t *ecma_new_standard_error (ecma_standard_error_t);
extern ecma_object_t *ecma_new_standard_error_with_message (ecma_standard_error_t, ecma_string_t *);
extern ecma_value_t ecma_raise_standard_error (ecma_standard_error_t, const lit_utf8_byte_t *);
extern ecma_value_t ecma_raise_common_error (const char *);
extern ecma_value_t ecma_raise_eval_error (const char *);
extern ecma_value_t ecma_raise_range_error (const char *);
extern ecma_value_t ecma_raise_reference_error (const char *);
extern ecma_value_t ecma_raise_syntax_error (const char *);
extern ecma_value_t ecma_raise_type_error (const char *);
extern ecma_value_t ecma_raise_uri_error (const char *);

/**
 * @}
 * @}
 */

#endif /* !ECMA_EXCEPTIONS_H */
