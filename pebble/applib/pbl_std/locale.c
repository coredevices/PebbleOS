/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "locale.h"

#include <limits.h>

#include <applib/i18n.h>
#include <process_state/app_state/app_state.h>

void locale_init_app_locale(LocaleInfo *info) {
  strncpy(info->app_locale_strings, "en_US", PBL_I18N_LOCALE_LEN);
  strncpy(info->app_locale_time, "en_US", PBL_I18N_LOCALE_LEN);
}

static void prv_update_locale(char *locale, const char *new) {
  if (new) {
    strncpy(locale, new, PBL_I18N_LOCALE_LEN);
  }
}

char *pbl_setlocale(int category, const char *locale) {
  LocaleInfo *info = app_state_get_locale_info();
  if (locale == nullptr) {
    switch (category) {
      case LC_ALL:
        return info->app_locale_strings;
      case LC_TIME:
        return info->app_locale_time;
      default:
        return nullptr;
    }
  } else if (*locale == '\0') {
    locale = app_get_system_locale();
  }

  switch (category) {
    case LC_ALL:
      prv_update_locale(info->app_locale_strings, locale);
      prv_update_locale(info->app_locale_time, locale);
      return info->app_locale_strings;
    case LC_TIME:
      prv_update_locale(info->app_locale_time, locale);
      return info->app_locale_time;
    default:
      /* not implemented */
      break;
  }

  return nullptr;
}

static const struct lconv pbl_lconv = {".",      "",       "",       "",       "",       "",
                                       "",       "",       "",       "",       CHAR_MAX, CHAR_MAX,
                                       CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX,
                                       CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX};

struct lconv *pbl_localeconv_r(struct _reent *data) {
  return (struct lconv *)&pbl_lconv;
}
