/* SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd. */
/* SPDX-License-Identifier: Apache-2.0 */

/*
 * [[ThrowTypeError]] description
 *
 * See also: ECMA-262 v5, 13.2.3
 */

#ifndef OBJECT_ID
# define OBJECT_ID(builtin_object_id)
#endif /* !OBJECT_ID */

#ifndef NUMBER_VALUE
# define NUMBER_VALUE(name, number_value, prop_attributes)
#endif /* !NUMBER_VALUE */

/* Object identifier */
OBJECT_ID (ECMA_BUILTIN_ID_TYPE_ERROR_THROWER)

/* Number properties:
 *  (property name, number value, writable, enumerable, configurable) */

NUMBER_VALUE (LIT_MAGIC_STRING_LENGTH,
              0,
              ECMA_PROPERTY_FIXED)

#undef OBJECT_ID
#undef SIMPLE_VALUE
#undef NUMBER_VALUE
#undef STRING_VALUE
#undef OBJECT_VALUE
#undef ROUTINE
