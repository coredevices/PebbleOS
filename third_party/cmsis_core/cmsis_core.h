/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#if defined(MICRO_FAMILY_STM32F4)
#include <stm32f4xx.h>
#elif defined(MICRO_FAMILY_NRF52)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-declarations"
#include <nrf52840.h>
#pragma GCC diagnostic pop
#elif defined(MICRO_FAMILY_SF32LB52)
#include <bf0_hal.h>
#else
#error "Unknown or missing MICRO_FAMILY_* define"
#endif