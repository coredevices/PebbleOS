/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>

#include <pbl/fs/fs.h>

//! Mount PFS at / on the fake SPI flash, as on boot: a flash without PFS is formatted.
//! Mounting again simulates a reboot.
void fake_pfs_mount(void);

//! Open @p path for reading and writing, creating it with @p size bytes if it does not exist.
//! @return 0 or a negative errno.
int fake_pfs_create(struct pbl_fs_file *file, const char *path, size_t size);
