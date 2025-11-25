/* SPDX-FileCopyrightText: 2014-2016 Samsung Electronics Co., Ltd. */
/* SPDX-License-Identifier: Apache-2.0 */

#ifndef ECMA_OBJECTS_GENERAL_H
#define ECMA_OBJECTS_GENERAL_H

#include "ecma-conversion.h"
#include "ecma-globals.h"

/** \addtogroup ecma ECMA
 * @{
 *
 * \addtogroup ecmaobjectsinternalops ECMA objects' operations
 * @{
 */

extern ecma_value_t ecma_reject (bool);
extern ecma_object_t *ecma_op_create_object_object_noarg (void);
extern ecma_value_t ecma_op_create_object_object_arg (ecma_value_t);
extern ecma_object_t *ecma_op_create_object_object_noarg_and_set_prototype (ecma_object_t *);

extern ecma_value_t ecma_op_general_object_delete (ecma_object_t *, ecma_string_t *, bool);
extern ecma_value_t ecma_op_general_object_default_value (ecma_object_t *, ecma_preferred_type_hint_t);
extern ecma_value_t ecma_op_general_object_define_own_property (ecma_object_t *, ecma_string_t *,
                                                                const ecma_property_descriptor_t *, bool);

/**
 * @}
 * @}
 */

#endif /* !ECMA_OBJECTS_GENERAL_H */
