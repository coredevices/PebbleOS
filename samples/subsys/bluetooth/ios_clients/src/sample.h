/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

/* Each starts its client on a connection whose link is encrypted, and stops it. */

void sample_ancs_start(uint16_t conn_handle);
void sample_ancs_stop(void);

void sample_ams_start(uint16_t conn_handle);
void sample_ams_stop(void);
