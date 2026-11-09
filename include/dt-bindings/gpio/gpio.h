/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/* Flags of a GPIO specifier, the values of the upstream binding */
#define GPIO_ACTIVE_HIGH      0
#define GPIO_ACTIVE_LOW       1
#define GPIO_PUSH_PULL        0
#define GPIO_SINGLE_ENDED     2
#define GPIO_LINE_OPEN_SOURCE 0
#define GPIO_LINE_OPEN_DRAIN  4
#define GPIO_OPEN_DRAIN       (GPIO_SINGLE_ENDED | GPIO_LINE_OPEN_DRAIN)
#define GPIO_OPEN_SOURCE      (GPIO_SINGLE_ENDED | GPIO_LINE_OPEN_SOURCE)
#define GPIO_PULL_UP          16
#define GPIO_PULL_DOWN        32
