/* SPDX-FileCopyrightText: 2014-2016 Samsung Electronics Co., Ltd. */
/* SPDX-License-Identifier: Apache-2.0 */

#ifndef ECMA_OBJECTS_ARGUMENTS_H
#define ECMA_OBJECTS_ARGUMENTS_H

#include "ecma-globals.h"
#include "ecma-helpers.h"

extern void
ecma_op_create_arguments_object (ecma_object_t *, ecma_object_t *, const ecma_value_t *,
                                 ecma_length_t, const ecma_compiled_code_t *);

extern void
ecma_arguments_update_mapped_arg_value (ecma_object_t *, ecma_string_t *, ecma_property_t *);
extern ecma_value_t
ecma_op_arguments_object_delete (ecma_object_t *, ecma_string_t *, bool);
extern ecma_value_t
ecma_op_arguments_object_define_own_property (ecma_object_t *, ecma_string_t *,
                                              const ecma_property_descriptor_t *, bool);

#endif /* !ECMA_OBJECTS_ARGUMENTS_H */
