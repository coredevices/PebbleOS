/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

//! Parks the app briefly when the background system work queue starves, from its watchdog.
void app_throttle_init(void);
