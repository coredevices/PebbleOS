/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#if defined(CONFIG_SOC_QEMU_PEBBLE_CORTEX_M4)
#include "pebble/qemu_cm4.h"
#elif defined(CONFIG_SOC_QEMU_PEBBLE_CORTEX_M33)
#include "pebble/qemu_cm33.h"
#elif defined(MICRO_FAMILY_STM32F4)
#include <stm32f4xx.h>
#elif defined(CONFIG_SOC_NRF52)
#include "pebble/nrf52.h"
#elif defined(CONFIG_SOC_SF32LB52)
#include <bf0_hal.h>
#else
#error "Unknown or missing MICRO_FAMILY_* define"
#endif