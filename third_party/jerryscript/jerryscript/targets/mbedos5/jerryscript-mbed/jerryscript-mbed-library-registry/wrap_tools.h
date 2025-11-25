/* SPDX-FileCopyrightText: 2016 ARM Limited */
/* SPDX-License-Identifier: Apache-2.0 */
#ifndef _JERRYSCRIPT_MBED_LIBRARY_REGISTRY_WRAP_TOOLS_H
#define _JERRYSCRIPT_MBED_LIBRARY_REGISTRY_WRAP_TOOLS_H

#include <stdlib.h>

#include "jerry-core/jerry-api.h"
 
#include "jerryscript-mbed-util/logging.h"
#include "jerryscript-mbed-util/wrappers.h"


//
// Functions used by the wrapper registration API.
//

bool
jsmbed_wrap_register_global_function (const char* name,
                          jerry_external_handler_t handler);

bool
jsmbed_wrap_register_class_constructor (const char* name,
                            jerry_external_handler_t handler);

bool
jsmbed_wrap_register_class_function (jerry_value_t this_obj_p,
                         const char* name,
                         jerry_external_handler_t handler);

#endif  // _JERRYSCRIPT_MBED_LIBRARY_REGISTRY_WRAP_TOOLS_H
