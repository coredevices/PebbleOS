/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/kernel/mpu.h>

//! Indices in pbl_thread_attr::regions.
#define PROCESS_REGIONS_THREAD_APP    0
#define PROCESS_REGIONS_THREAD_WORKER 1

const struct pbl_mpu_region *process_region_app(void);
const struct pbl_mpu_region *process_region_worker(void);

//! Gives every thread that does not set its own the kernel view of App and Worker RAM.
void process_regions_thread_init(void);
