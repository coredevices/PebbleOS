/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/runlevel.h"

void services_normal_early_init(void);

void services_normal_init(void);

void services_normal_set_runlevel(RunLevel runlevel);
