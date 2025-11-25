/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

// These values need to multiples of subsectors (4k) to make them easy to erase.
//! Includes the header + the metrics
#define SECTION_HEADER_SIZE_BYTES 4096 // Contains both the file header and the stats
#define SECTION_LOGS_SIZE_BYTES (4096 * 7)

#define DEBUG_DB_NUM_FILES 4

void debug_db_determine_current_index(uint8_t* file_id, int* current_file_index, uint8_t* current_file_id);

void debug_db_init(void);

bool debug_db_is_generation_valid(int file_generation);
uint32_t debug_db_get_stats_base_address(int file_generation);
uint32_t debug_db_get_logs_base_address(int file_generation);

void debug_db_reformat_header_section(void);

uint32_t debug_db_get_stat_section_size(void);

