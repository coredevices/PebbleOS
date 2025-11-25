/* SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd. */
/* SPDX-FileCopyrightText: 2015 University of Szeged. */
/* SPDX-License-Identifier: Apache-2.0 */

/*
 * Date built-in description
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

#ifndef ROUTINE
# define ROUTINE(name, c_function_name, args_number, length_prop_value)
#endif /* !ROUTINE */

/* Object identifier */
OBJECT_ID (ECMA_BUILTIN_ID_DATE)

// ECMA-262 v5, 15.9.4.1
OBJECT_VALUE (LIT_MAGIC_STRING_PROTOTYPE,
              ECMA_BUILTIN_ID_DATE_PROTOTYPE,
              ECMA_PROPERTY_FIXED)

NUMBER_VALUE (LIT_MAGIC_STRING_LENGTH,
              7,
              ECMA_PROPERTY_FIXED)

ROUTINE (LIT_MAGIC_STRING_PARSE, ecma_builtin_date_parse, 1, 1)
ROUTINE (LIT_MAGIC_STRING_UTC_U, ecma_builtin_date_utc, NON_FIXED, 7)
ROUTINE (LIT_MAGIC_STRING_NOW, ecma_builtin_date_now, 0, 0)

#undef OBJECT_ID
#undef SIMPLE_VALUE
#undef NUMBER_VALUE
#undef STRING_VALUE
#undef OBJECT_VALUE
#undef ROUTINE
