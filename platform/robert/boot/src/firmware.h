/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "stm32f7xx.h"

extern const uint32_t __BOOTLOADER_size__[];

#define BOOTLOADER_SIZE ((uint32_t) __BOOTLOADER_size__)
#define FIRMWARE_BASE (FLASH_BASE + BOOTLOADER_SIZE)
