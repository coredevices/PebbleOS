/* SPDX-FileCopyrightText: 2016 ARM Limited */
/* SPDX-License-Identifier: Apache-2.0 */
#include "jerryscript-mbed-drivers/assert-js.h"

/**
 * assert (native JavaScript function)
 * 
 * @param condition The condition to assert to be true
 * @returns undefined if the assertion passes, returns an error if the assertion
 * 	fails.
 */
DECLARE_GLOBAL_FUNCTION(assert) {
    CHECK_ARGUMENT_COUNT(global, assert, (args_count == 1));
    CHECK_ARGUMENT_TYPE_ALWAYS(global, assert, 0, boolean);

    if (!jerry_get_boolean_value(args[0])) {
        const char* error_msg = "Assertion failed";
        return jerry_create_error(JERRY_ERROR_TYPE, reinterpret_cast<const jerry_char_t*>(error_msg));
    }

    return jerry_create_undefined();
}
