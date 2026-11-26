/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/disk.h>

/**
 * @brief A disk in RAM, of @c CONFIG_DISK_RAM_SIZE KiB in 512-byte sectors, zeroed at boot.
 * @ingroup drivers_disk
 */
extern const struct pbl_disk pbl_disk_ram;
