/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

//! The address given on the command line, least significant octet first. False if none was.
bool id_addr_posix_bottom_get(uint8_t octets[6]);
