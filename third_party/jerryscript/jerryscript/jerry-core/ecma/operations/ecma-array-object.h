/* SPDX-FileCopyrightText: 2014-2015 Samsung Electronics Co., Ltd. */
/* SPDX-License-Identifier: Apache-2.0 */

#ifndef ECMA_ARRAY_OBJECT_H
#define ECMA_ARRAY_OBJECT_H

#include "ecma-globals.h"

/** \addtogroup ecma ECMA
 * @{
 *
 * \addtogroup ecmaarrayobject ECMA Array object related routines
 * @{
 */

extern ecma_value_t
ecma_op_create_array_object (const ecma_value_t *, ecma_length_t, bool);

extern ecma_value_t
ecma_op_array_object_define_own_property (ecma_object_t *, ecma_string_t *, const ecma_property_descriptor_t *, bool);

/**
 * @}
 * @}
 */

#endif /* !ECMA_ARRAY_OBJECT_H */
