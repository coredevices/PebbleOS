/* SPDX-FileCopyrightText: 2014-2016 Samsung Electronics Co., Ltd. */
/* SPDX-License-Identifier: Apache-2.0 */

#ifndef ECMA_COMPARISON_H
#define ECMA_COMPARISON_H

#include "ecma-globals.h"
#include "ecma-helpers.h"

/** \addtogroup ecma ECMA
 * @{
 *
 * \addtogroup ecmacomparison ECMA comparison
 * @{
 */

extern ecma_value_t ecma_op_abstract_equality_compare (ecma_value_t, ecma_value_t);
extern bool ecma_op_strict_equality_compare (ecma_value_t, ecma_value_t);
extern ecma_value_t ecma_op_abstract_relational_compare (ecma_value_t, ecma_value_t, bool);

/**
 * @}
 * @}
 */

#endif /* !ECMA_COMPARISON_H */
