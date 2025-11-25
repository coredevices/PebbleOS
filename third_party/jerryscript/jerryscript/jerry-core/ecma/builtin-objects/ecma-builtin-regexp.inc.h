/* SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd. */
/* SPDX-FileCopyrightText: 2015 University of Szeged. */
/* SPDX-License-Identifier: Apache-2.0 */

/*
 * RegExp built-in description
 */

#ifndef OBJECT_ID
# define OBJECT_ID(builtin_object_id)
#endif /* !OBJECT_ID */

#ifndef OBJECT_VALUE
# define OBJECT_VALUE(name, obj_builtin_id, prop_attributes)
#endif /* !OBJECT_VALUE */

#ifndef NUMBER_VALUE
# define NUMBER_VALUE(name, number_value, prop_attributes)
#endif /* !NUMBER_VALUE */

/* Object identifier */
OBJECT_ID (ECMA_BUILTIN_ID_REGEXP)

// ECMA-262 v5, 15.10.5
NUMBER_VALUE (LIT_MAGIC_STRING_LENGTH,
              2,
              ECMA_PROPERTY_FIXED)

// ECMA-262 v5, 15.10.5.1
OBJECT_VALUE (LIT_MAGIC_STRING_PROTOTYPE,
              ECMA_BUILTIN_ID_REGEXP_PROTOTYPE,
              ECMA_PROPERTY_FIXED)

#undef OBJECT_ID
#undef SIMPLE_VALUE
#undef NUMBER_VALUE
#undef STRING_VALUE
#undef OBJECT_VALUE
#undef ROUTINE
