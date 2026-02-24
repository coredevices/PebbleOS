/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/*
 * chip_id.h
 *
 * This file specifies IDs for the different processors on our multi-processor devices.
 * The IDs are used to differenetiate the source of system logs, core dumps, etc.
 *
 * The IDs must be unique within a platform and must fit in 2 bits.
 * If we build a device with more than 4 log/core dump producing processors, this will need to be
 * addressed.
 */

#define CORE_ID_MAIN_MCU 0
#define CORE_ID_BLE 1
