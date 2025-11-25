/* SPDX-FileCopyrightText: 2016 ARM Limited */
/* SPDX-License-Identifier: Apache-2.0 */
#include "jerryscript-mbed-event-loop/EventLoop.h"

namespace mbed {
namespace js {

EventLoop EventLoop::instance;

void event_loop() {
    EventLoop::getInstance().go();
}

}  // namespace js
}  // namespace mbed
