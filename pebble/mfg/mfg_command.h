/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <string.h>
// NB: Factory registry has 10 records statically allocated for it: see registry.c

void mfg_command_init(void);
void mfgdata_read_serial_number(char *serial_number, size_t serial_number_size);
