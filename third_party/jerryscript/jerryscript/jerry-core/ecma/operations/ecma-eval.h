/* SPDX-FileCopyrightText: 2015-2016 Samsung Electronics Co., Ltd. */
/* SPDX-FileCopyrightText: 2016 University of Szeged. */
/* SPDX-License-Identifier: Apache-2.0 */

#ifndef ECMA_EVAL_H
#define ECMA_EVAL_H

#include "ecma-globals.h"

/** \addtogroup ecma ECMA
 * @{
 *
 * \addtogroup eval eval
 * @{
 */

extern ecma_value_t
ecma_op_eval (ecma_string_t *, bool, bool);

extern ecma_value_t
ecma_op_eval_chars_buffer (const lit_utf8_byte_t *, size_t, bool, bool);

/**
 * @}
 * @}
 */

#endif /* !ECMA_EVAL_H */
