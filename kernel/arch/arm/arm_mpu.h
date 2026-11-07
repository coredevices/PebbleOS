/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/kernel/mpu.h>

//! Encodes a region for a slot, into what the RBAR and RASR (ARMv7-M) or RLAR
//! (ARMv8-M) registers take.
void arm_mpu_region_encode(unsigned int slot, const struct pbl_mpu_region *region, uint32_t *rbar,
                           uint32_t *attr);
