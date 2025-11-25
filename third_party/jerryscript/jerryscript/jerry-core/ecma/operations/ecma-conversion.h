/* SPDX-FileCopyrightText: 2014-2016 Samsung Electronics Co., Ltd. */
/* SPDX-License-Identifier: Apache-2.0 */

#ifndef ECMA_CONVERSION_H
#define ECMA_CONVERSION_H

#include "ecma-globals.h"
#include "ecma-helpers.h"

/** \addtogroup ecma ECMA
 * @{
 *
 * \addtogroup ecmaconversion ECMA conversion routines
 * @{
 */

/**
 * Second argument of 'ToPrimitive' operation that is a hint,
 * specifying the preferred type of conversion result.
 */
typedef enum
{
  ECMA_PREFERRED_TYPE_NO, /**< no preferred type is specified */
  ECMA_PREFERRED_TYPE_NUMBER, /**< Number */
  ECMA_PREFERRED_TYPE_STRING /**< String */
} ecma_preferred_type_hint_t;

extern ecma_value_t ecma_op_check_object_coercible (ecma_value_t);
extern bool ecma_op_same_value (ecma_value_t, ecma_value_t);
extern ecma_value_t ecma_op_to_primitive (ecma_value_t, ecma_preferred_type_hint_t);
extern bool ecma_op_to_boolean (ecma_value_t);
extern ecma_value_t ecma_op_to_number (ecma_value_t);
extern ecma_value_t ecma_op_to_string (ecma_value_t);
extern ecma_value_t ecma_op_to_object (ecma_value_t);

extern ecma_object_t *ecma_op_from_property_descriptor (const ecma_property_descriptor_t *);
extern ecma_value_t ecma_op_to_property_descriptor (ecma_value_t, ecma_property_descriptor_t *);

/**
 * @}
 * @}
 */

#endif /* !ECMA_CONVERSION_H */
