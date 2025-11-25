/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

void periph_config_init(void);
void periph_config_acquire_lock(void);
void periph_config_release_lock(void);
void periph_config_enable(void *periph, uint32_t rcc_bit);
void periph_config_disable(void *periph, uint32_t rcc_bit);
