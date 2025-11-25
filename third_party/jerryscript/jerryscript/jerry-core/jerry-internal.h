/* SPDX-FileCopyrightText: 2015-2016 Samsung Electronics Co., Ltd. */
/* SPDX-License-Identifier: Apache-2.0 */

#ifndef JERRY_INTERNAL
 # error "The header is for Jerry's internal interfaces"
#endif /* !JERRY_INTERNAL */

#ifndef JERRY_INTERNAL_H
#define JERRY_INTERNAL_H

#include "ecma-globals.h"
#include "jerry-api.h"

extern ecma_value_t
jerry_dispatch_external_function (ecma_object_t *,
                                  ecma_external_pointer_t,
                                  ecma_value_t,
                                  const ecma_value_t *,
                                  ecma_length_t);

extern void
jerry_dispatch_object_free_callback (ecma_external_pointer_t, ecma_external_pointer_t);

#endif /* !JERRY_INTERNAL_H */
