/* SPDX-FileCopyrightText: 2014-2016 Samsung Electronics Co., Ltd. */
/* SPDX-License-Identifier: Apache-2.0 */

#include "ecma-globals.h"
#include "ecma-helpers.h"
#include "ecma-number-arithmetic.h"

/** \addtogroup ecma ECMA
 * @{
 *
 * \addtogroup numberarithmetic ECMA number arithmetic operations
 * @{
 */

/**
 * ECMA-defined number remainder calculation.
 *
 * See also:
 *          ECMA-262 v5, 11.5.3
 *
 * @return number - calculated remainder.
 */
ecma_number_t
ecma_op_number_remainder (ecma_number_t left_num, /**< left operand */
                          ecma_number_t right_num) /**< right operand */
{
  if (ecma_number_is_nan (left_num)
      || ecma_number_is_nan (right_num)
      || ecma_number_is_infinity (left_num)
      || ecma_number_is_zero (right_num))
  {
    return ecma_number_make_nan ();
  }
  else if (ecma_number_is_infinity (right_num)
           || (ecma_number_is_zero (left_num)
               && !ecma_number_is_zero (right_num)))
  {
    return left_num;
  }

  return ecma_number_calc_remainder (left_num, right_num);
} /* ecma_op_number_remainder */

/**
 * @}
 * @}
 */
