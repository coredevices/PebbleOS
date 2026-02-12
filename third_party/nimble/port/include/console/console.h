/* SPDX-FileCopyrightText: 2025 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#ifndef __CONSOLE_H__
#define __CONSOLE_H__

#include "system/logging.h"

#define console_printf(_fmt, ...) PBL_LOG_D_INFO(LOG_DOMAIN_BT_STACK, _fmt, ##__VA_ARGS__)

#endif /* __CONSOLE_H__ */
