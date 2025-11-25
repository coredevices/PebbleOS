/* SPDX-FileCopyrightText: 2014-2015 Samsung Electronics Co., Ltd. */
/* SPDX-License-Identifier: Apache-2.0 */

#ifndef ECMA_STRING_OBJECT_H
#define ECMA_STRING_OBJECT_H

#include "ecma-globals.h"

/** \addtogroup ecma ECMA
 * @{
 *
 * \addtogroup ecmastringobject ECMA String object related routines
 * @{
 */

extern ecma_value_t
ecma_op_create_string_object (const ecma_value_t *, ecma_length_t);

extern void
ecma_op_string_list_lazy_property_names (ecma_object_t *,
                                         bool,
                                         ecma_collection_header_t *,
                                         ecma_collection_header_t *);


/**
 * @}
 * @}
 */

#endif /* !ECMA_STRING_OBJECT_H */
