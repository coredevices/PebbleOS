/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "stm32f4xx_flash.h"

#define ADDR_FLASH_SECTOR_0     ((uint32_t)0x08000000) /* Base @ of Sector 0, 16 Kbytes */
#define ADDR_FLASH_SECTOR_1     ((uint32_t)0x08004000) /* Base @ of Sector 1, 16 Kbytes */
#define ADDR_FLASH_SECTOR_2     ((uint32_t)0x08008000) /* Base @ of Sector 2, 16 Kbytes */
#define ADDR_FLASH_SECTOR_3     ((uint32_t)0x0800C000) /* Base @ of Sector 3, 16 Kbytes */
#define ADDR_FLASH_SECTOR_4     ((uint32_t)0x08010000) /* Base @ of Sector 4, 64 Kbytes */
#define ADDR_FLASH_SECTOR_5     ((uint32_t)0x08020000) /* Base @ of Sector 5, 128 Kbytes */
#define ADDR_FLASH_SECTOR_6     ((uint32_t)0x08040000) /* Base @ of Sector 6, 128 Kbytes */
#define ADDR_FLASH_SECTOR_7     ((uint32_t)0x08060000) /* Base @ of Sector 7, 128 Kbytes */
#define ADDR_FLASH_SECTOR_8     ((uint32_t)0x08080000) /* Base @ of Sector 8, 128 Kbytes */
#define ADDR_FLASH_SECTOR_9     ((uint32_t)0x080A0000) /* Base @ of Sector 9, 128 Kbytes */
#define ADDR_FLASH_SECTOR_10    ((uint32_t)0x080C0000) /* Base @ of Sector 10, 128 Kbytes */
#define ADDR_FLASH_SECTOR_11    ((uint32_t)0x080E0000) /* Base @ of Sector 11, 128 Kbytes */

typedef void (*SystemFlashProgressCb)(
    uint32_t progress, uint32_t total, void *context);

// Erase the sectors of flash which lie within the given address range.
//
// If the address range overlaps even one single byte of a sector, the entire
// sector is erased.
//
// If progress_callback is not NULL, it is called at the beginning of the erase
// process and after each sector is erased. The rational number (progress/total)
// increases monotonically as the sector erasue procedure progresses.
//
// Returns true if successful, false if an error occurred.
bool system_flash_erase(
    uint32_t address, size_t length,
    SystemFlashProgressCb progress_callback, void *progress_context);

// Write data into flash. The flash must already be erased.
//
// If progress_callback is not NULL, it is called at the beginning of the
// writing process and periodically thereafter. The rational number
// (progress/total) increases monotonically as the data is written.
//
// Returns true if successful, false if an error occurred.
bool system_flash_write(
    uint32_t address, const void *data, size_t length,
    SystemFlashProgressCb progress_callback, void *progress_context);
