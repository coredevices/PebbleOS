/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <locale.h>

#include <pbl/i18n/i18n.h>

typedef struct {
  char sys_locale[PBL_I18N_LOCALE_LEN];
  char app_locale_time[PBL_I18N_LOCALE_LEN];
  char app_locale_strings[PBL_I18N_LOCALE_LEN];
} LocaleInfo;

void locale_init_app_locale(LocaleInfo *info);

char *pbl_setlocale(int category, const char *locale);

struct _reent;

struct lconv *pbl_localeconv_r(struct _reent *data);
