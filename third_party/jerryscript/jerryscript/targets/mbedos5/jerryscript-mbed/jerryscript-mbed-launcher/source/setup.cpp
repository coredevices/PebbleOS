/* SPDX-FileCopyrightText: 2016 ARM Limited */
/* SPDX-License-Identifier: Apache-2.0 */
#include <stdlib.h>
#include <stdio.h>

#include "jerryscript-mbed-launcher/setup.h"
#include "jerryscript-mbed-util/logging.h"

extern uint32_t jsmbed_js_magic_string_count;
extern uint32_t jsmbed_js_magic_string_values[];

extern const jerry_char_ptr_t jsmbed_js_magic_strings[];
extern const jerry_length_t jsmbed_js_magic_string_lengths[];

void jsmbed_js_load_magic_strings() {
    if (jsmbed_js_magic_string_count == 0) {
        return;
    }

    jerry_register_magic_strings(jsmbed_js_magic_strings,
                                 jsmbed_js_magic_string_count,
                                 jsmbed_js_magic_string_lengths);

    jerry_value_t global = jerry_get_global_object();

    for (unsigned int idx = 0; idx < jsmbed_js_magic_string_count; idx++) {
        jerry_value_t constant_value = jerry_create_number(jsmbed_js_magic_string_values[idx]);
        jerry_value_t magic_string = jerry_create_string(jsmbed_js_magic_strings[idx]);

        jerry_set_property(global, magic_string, constant_value);

        jerry_release_value(constant_value);
        jerry_release_value(magic_string);
    }

    jerry_release_value(global);
}
