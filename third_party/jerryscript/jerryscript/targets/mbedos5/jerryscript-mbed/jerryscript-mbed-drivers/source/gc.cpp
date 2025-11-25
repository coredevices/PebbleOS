/* SPDX-FileCopyrightText: 2016 ARM Limited */
/* SPDX-License-Identifier: Apache-2.0 */
#include "jerryscript-mbed-drivers/gc-js.h"

/**
 * gc (native JavaScript function)
 *
 * Manually cause JerryScript to run garbage collection.
 */
DECLARE_GLOBAL_FUNCTION(gc) {
    jerry_gc();
    return jerry_create_undefined();
}
