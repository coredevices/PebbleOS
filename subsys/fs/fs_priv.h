/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/fs/fs.h>

struct pbl_fs_mount *pbl_fs__mount_get(const char *path, const char **name);
