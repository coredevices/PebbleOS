/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/common/i18n/i18n.h"
#include <locale.h>

typedef struct {
  char sys_locale[ISO_LOCALE_LENGTH];
  char app_locale_time[ISO_LOCALE_LENGTH];
  char app_locale_strings[ISO_LOCALE_LENGTH];
} LocaleInfo;

void locale_init_app_locale(LocaleInfo *info);

char *pbl_setlocale(int category, const char *locale);

struct _reent;

struct lconv *pbl_localeconv_r(struct _reent *data);

