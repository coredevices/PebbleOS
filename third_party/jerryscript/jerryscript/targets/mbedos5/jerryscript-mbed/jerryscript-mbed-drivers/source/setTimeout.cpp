/* SPDX-FileCopyrightText: 2016 ARM Limited */
/* SPDX-License-Identifier: Apache-2.0 */
#include "jerryscript-mbed-drivers/setTimeout-js.h"
#include "jerryscript-mbed-event-loop/EventLoop.h"

/**
 * setTimeout (native JavaScript function)
 *
 * Call a JavaScript function once, after a fixed time period.
 *
 * @param function Function to call
 * @param wait_time Time before function is called, in ms.
 */
DECLARE_GLOBAL_FUNCTION(setTimeout) {
    CHECK_ARGUMENT_COUNT(global, setTimeout, (args_count == 2));
    CHECK_ARGUMENT_TYPE_ALWAYS(global, setTimeout, 0, function);
    CHECK_ARGUMENT_TYPE_ALWAYS(global, setTimeout, 1, number);

    jerry_acquire_value(args[0]);
    int interval = int(jerry_get_number_value(args[1]));

    mbed::js::EventLoop::getInstance().getQueue().call_in(interval, jerry_call_function, args[0], jerry_create_null(), (jerry_value_t*)NULL, 0);

    return jerry_create_undefined();
}
