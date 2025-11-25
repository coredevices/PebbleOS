/* SPDX-FileCopyrightText: 2016 ARM Limited */
/* SPDX-License-Identifier: Apache-2.0 */
#include "jerryscript-mbed-drivers/setInterval-js.h"
#include "jerryscript-mbed-event-loop/EventLoop.h"

/**
 * setInterval (native JavaScript function)
 *
 * Call a JavaScript function at fixed intervals.
 *
 * @param function Function to call
 * @param interval Time between function calls, in ms.
 */
DECLARE_GLOBAL_FUNCTION(setInterval) {
    CHECK_ARGUMENT_COUNT(global, setInterval, (args_count == 2));
    CHECK_ARGUMENT_TYPE_ALWAYS(global, setInterval, 0, function);
    CHECK_ARGUMENT_TYPE_ALWAYS(global, setInterval, 1, number);

    jerry_acquire_value(args[0]);
    int interval = int(jerry_get_number_value(args[1]));

    mbed::js::EventLoop::getInstance().getQueue().call_every(interval, jerry_call_function, args[0], jerry_create_null(), (jerry_value_t*)NULL, 0);
    return jerry_create_undefined();
}
