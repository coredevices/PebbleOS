/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#define MHZ_TO_HZ(hz) (((uint32_t)(hz)) * 1000000)

#define KiBYTES(k) ((k) * 1024) // Kibibytes to Bytes
#define MiBYTES(m) ((m) * 1024 * 1024) // Mebibytes to Bytes
#define EiBYTES(e) ((e) * 1024 * 1024 * 1024 * 1024 * 1024 * 1024) // Exbibytes to Bytes

#define MM_PER_METER 1000
#define METERS_PER_KM 1000
#define METERS_PER_MILE 1609

#define US_PER_MS (1000)
#define US_PER_S (1000000)
#define NS_PER_S (1000000000)
#define PS_PER_NS (1000)
#define PS_PER_US (1000000)
#define PS_PER_S (1000000000000ULL)
