/* SPDX-FileCopyrightText: 2016 ARM Limited */
/* SPDX-License-Identifier: Apache-2.0 */

// Provides jsmbed_js_launch()
#include "jerryscript-mbed-launcher/launcher.h"

// Provides JSMBED_USE_WRAPPER()
#include "jerryscript-mbed-library-registry/registry.h"

// Provides the base wrapper registration symbol that JSMBED_USE_WRAPPER uses.
// This means all of the base handlers will be registered.
#include "jerryscript-mbed-drivers/lib_drivers.h"

#include "jerryscript-mbed-event-loop/EventLoop.h"

#include "Callback.h"
#include "Serial.h"

using mbed::js::EventLoop;
using mbed::Callback;

int main() {
    mbed::Serial pc(USBTX, USBRX);
    pc.baud(115200);

    JERRY_USE_MBED_LIBRARY(base);

    // Incude more wrapper packages here if you want to use them. For exmaple:
    // JERRY_USE_MBED_LIBRARY(lwip_interface);
    // JERRY_USE_MBED_LIBRARY(esp8266_interface);
    // JERRY_USE_MBED_LIBRARY(simple_mbed_client);

    jsmbed_js_launch();

    return 0;
}
