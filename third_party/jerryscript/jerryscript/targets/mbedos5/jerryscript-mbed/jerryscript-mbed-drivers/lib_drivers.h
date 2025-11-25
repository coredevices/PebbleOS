/* SPDX-FileCopyrightText: 2016 ARM Limited */
/* SPDX-License-Identifier: Apache-2.0 */
#ifndef _JERRYSCRIPT_MBED_DRIVERS_LIB_DRIVERS_H
#define _JERRYSCRIPT_MBED_DRIVERS_LIB_DRIVERS_H

#include "jerryscript-mbed-drivers/InterruptIn-js.h"
#include "jerryscript-mbed-drivers/DigitalOut-js.h"
#include "jerryscript-mbed-drivers/setInterval-js.h"
#include "jerryscript-mbed-drivers/setTimeout-js.h"
#include "jerryscript-mbed-drivers/assert-js.h"
#include "jerryscript-mbed-drivers/I2C-js.h"
#include "jerryscript-mbed-drivers/gc-js.h"

DECLARE_JS_WRAPPER_REGISTRATION (base) {
    REGISTER_GLOBAL_FUNCTION(assert);
    REGISTER_GLOBAL_FUNCTION(gc);
    REGISTER_GLOBAL_FUNCTION(setInterval);
    REGISTER_GLOBAL_FUNCTION(setTimeout);
    REGISTER_CLASS_CONSTRUCTOR(DigitalOut);
    REGISTER_CLASS_CONSTRUCTOR(I2C);
    REGISTER_CLASS_CONSTRUCTOR(InterruptIn);
}

#endif  // _JERRYSCRIPT_MBED_DRIVERS_LIB_DRIVERS_H
