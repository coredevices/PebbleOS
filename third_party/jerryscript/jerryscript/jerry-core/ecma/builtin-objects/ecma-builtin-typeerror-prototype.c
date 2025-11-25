/* SPDX-FileCopyrightText: 2014-2015 Samsung Electronics Co., Ltd. */
/* SPDX-License-Identifier: Apache-2.0 */

#include "ecma-alloc.h"
#include "ecma-builtins.h"
#include "ecma-conversion.h"
#include "ecma-exceptions.h"
#include "ecma-gc.h"
#include "ecma-globals.h"
#include "ecma-helpers.h"
#include "ecma-objects.h"
#include "ecma-string-object.h"
#include "ecma-try-catch-macro.h"
#include "jrt.h"

#ifndef CONFIG_DISABLE_ERROR_BUILTINS

#define ECMA_BUILTINS_INTERNAL
#include "ecma-builtins-internal.h"

#define BUILTIN_INC_HEADER_NAME "ecma-builtin-typeerror-prototype.inc.h"
#define BUILTIN_UNDERSCORED_ID type_error_prototype
#include "ecma-builtin-internal-routines-template.inc.h"

#endif /* !CONFIG_DISABLE_ERROR_BUILTINS */
