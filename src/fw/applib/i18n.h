/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>

//! @file i18n.h
//! Wrapper function for i18n syscalls that make up the app's i18n APIs

//! @addtogroup Foundation
//! @{
//!   @addtogroup Internationalization
//! \brief Internationalization & Localization APIs
//!
//!   @{

//! Get the ISO locale name for the language currently set on the watch
//! @return A string containing the ISO locale name (e.g. "fr", "en_US", ...)
//! @note It is possible for the locale to change while your app is running.
//! And thus, two calls to i18n_get_system_locale may return different values.
const char *app_get_system_locale(void);

//! @internal
//! Get a translated version of a astring in a given locale
//! @param locale the ISO locale to translate to
//! @param string the english string to translate
//! @param buffer the buffer to copy the translation to
//! @param length the length of the buffer
void app_i18n_get(const char *locale, const char *string, char *buffer, size_t length);

//!   @} // end addtogroup Internationalization
//! @} // end addtogroup Foundation
