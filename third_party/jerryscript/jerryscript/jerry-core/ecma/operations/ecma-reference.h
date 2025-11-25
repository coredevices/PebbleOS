/* SPDX-FileCopyrightText: 2014-2016 Samsung Electronics Co., Ltd. */
/* SPDX-License-Identifier: Apache-2.0 */

#ifndef ECMA_REFERENCE_H
#define ECMA_REFERENCE_H

#include "ecma-globals.h"
#include "jrt.h"

/** \addtogroup ecma ECMA
 * @{
 *
 * \addtogroup references ECMA-Reference
 * @{
 */

extern ecma_object_t *ecma_op_resolve_reference_base (ecma_object_t *, ecma_string_t *);
extern ecma_value_t ecma_op_resolve_reference_value (ecma_object_t *, ecma_string_t *);

/**
 * @}
 * @}
 */

#endif /* !ECMA_REFERENCE_H */
