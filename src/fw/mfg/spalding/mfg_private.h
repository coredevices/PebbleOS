/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

void mfg_info_write_boot_fpga_bitstream(void);
bool mfg_info_is_boot_fpga_bitstream_written(void);
