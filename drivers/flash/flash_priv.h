/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/flash.h>

int pbl_flash__access_begin(const struct pbl_flash *flash, uint32_t resume_ms);
void pbl_flash__access_end(const struct pbl_flash *flash);
