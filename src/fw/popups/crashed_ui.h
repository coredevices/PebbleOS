/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "process_management/app_install_types.h"

//! Show the "Bug report captured" modal
void crashed_ui_show_forced_core_dump(void);

void crashed_ui_show_worker_crash(const AppInstallId install_id);
