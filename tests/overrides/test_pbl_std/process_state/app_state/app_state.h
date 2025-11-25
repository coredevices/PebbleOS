/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/pbl_std/locale.h"

#include <time.h>

struct tm *app_state_get_gmtime_tm(void);
struct tm *app_state_get_localtime_tm(void);
char *app_state_get_localtime_zone(void);

LocaleInfo *app_state_get_locale_info(void);

typedef int AppStateInitParams;
typedef struct {char _unused;} TextRenderState;
