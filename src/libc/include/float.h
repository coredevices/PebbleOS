/* SPDX-FileCopyrightText: 2025 Apache Software Foundation (ASF) */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#ifndef FLT_EPSILON                  /* May be defined in toolchain header */
#  define FLT_EPSILON 1.1920929e-07F /* 1E-5 */
#endif

#ifndef DBL_EPSILON                  /* May be defined in toolchain header */
#  define DBL_EPSILON 2.2204460492503131e-16 /* smallest double such that 1.0 + DBL_EPSILON != 1.0 */
#endif

#ifndef DBL_DIG                      /* May be defined in toolchain header */
#  define DBL_DIG 15                 /* number of decimal digits of precision for double */
#endif

#ifndef DBL_MAX                      /* May be defined in toolchain header */
#  define DBL_MAX 1.7976931348623157e+308 /* max finite value of a double */
#endif

#ifndef DBL_MAX_10_EXP               /* May be defined in toolchain header */
#  define DBL_MAX_10_EXP 308         /* max decimal exponent of a double */
#endif

#ifndef DBL_MAX_EXP                  /* May be defined in toolchain header */
#  define DBL_MAX_EXP 1024           /* max binary exponent of a double */
#endif

#ifndef FLT_RADIX                    /* May be defined in toolchain header */
#  define FLT_RADIX 2                /* radix of exponent representation */
#endif

#ifndef FLT_EVAL_METHOD              /* May be defined in toolchain header */
#  define FLT_EVAL_METHOD 0          /* evaluate all operations just to the range and precision of the type */
#endif