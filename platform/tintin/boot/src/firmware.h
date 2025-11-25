/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "stm32f2xx.h"

#define FW_OLDWORLD_OFFSET (0x10000)
#define FW_NEWWORLD_OFFSET (0x04000)
#define FW_WORLD_DIFFERENCE (FW_OLDWORLD_OFFSET - FW_NEWWORLD_OFFSET)

#define FIRMWARE_OLDWORLD_BASE (FLASH_BASE + FW_OLDWORLD_OFFSET)
#define FIRMWARE_NEWWORLD_BASE (FLASH_BASE + FW_NEWWORLD_OFFSET)

// Byte offset of NeWo in firmware
#define FW_IDENTIFIER_OFFSET (28)

bool firmware_is_new_world(void* base);
