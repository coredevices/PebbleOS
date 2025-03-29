#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

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
