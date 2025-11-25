/* SPDX-FileCopyrightText: 2014-2016 Samsung Electronics Co., Ltd. */
/* SPDX-License-Identifier: Apache-2.0 */

#include "ecma-alloc.h"
#include "ecma-boolean-object.h"
#include "ecma-builtins.h"
#include "ecma-exceptions.h"
#include "ecma-gc.h"
#include "ecma-globals.h"
#include "ecma-helpers.h"
#include "ecma-objects.h"
#include "ecma-objects-general.h"

/** \addtogroup ecma ECMA
 * @{
 *
 * \addtogroup ecmabooleanobject ECMA Boolean object related routines
 * @{
 */

/**
 * Boolean object creation operation.
 *
 * See also: ECMA-262 v5, 15.6.2.1
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value
 */
ecma_value_t
ecma_op_create_boolean_object (ecma_value_t arg) /**< argument passed to the Boolean constructor */
{
  bool boolean_value = ecma_op_to_boolean (arg);

#ifndef CONFIG_DISABLE_BOOLEAN_BUILTIN
  ecma_object_t *prototype_obj_p = ecma_builtin_get (ECMA_BUILTIN_ID_BOOLEAN_PROTOTYPE);
#else /* CONFIG_DISABLE_BOOLEAN_BUILTIN */
  ecma_object_t *prototype_obj_p = ecma_builtin_get (ECMA_BUILTIN_ID_OBJECT_PROTOTYPE);
#endif /* !CONFIG_DISABLE_BOOLEAN_BUILTIN */

  ecma_object_t *obj_p = ecma_create_object (prototype_obj_p, false, true, ECMA_OBJECT_TYPE_GENERAL);
  ecma_deref_object (prototype_obj_p);

  ecma_value_t *class_prop_p = ecma_create_internal_property (obj_p, ECMA_INTERNAL_PROPERTY_CLASS);
  *class_prop_p = LIT_MAGIC_STRING_BOOLEAN_UL;

  ecma_value_t *prim_value_p = ecma_create_internal_property (obj_p,
                                                              ECMA_INTERNAL_PROPERTY_ECMA_VALUE);

  *prim_value_p = ecma_make_boolean_value (boolean_value);

  return ecma_make_object_value (obj_p);
} /* ecma_op_create_boolean_object */

/**
 * @}
 * @}
 */
