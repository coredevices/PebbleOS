/*-
 * Copyright (c) 2001 Alexey Zelkin <phantom@FreeBSD.org>
 * Copyright (c) 1997 FreeBSD Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stddef.h>

#include "timelocal.h"

#include "services/common/i18n/i18n.h"

#define LCTIME_SIZE (sizeof(struct lc_time_T) / sizeof(char *))

static const struct lc_time_T _C_time_locale = {
  .mon = {
    i18n_noop("Jan"), i18n_noop("Feb"), i18n_noop("Mar"), i18n_noop("Apr"), i18n_noop("May"),
    i18n_noop("Jun"), i18n_noop("Jul"), i18n_noop("Aug"), i18n_noop("Sep"), i18n_noop("Oct"),
    i18n_noop("Nov"), i18n_noop("Dec")
  },
  .month = {
    i18n_noop("January"), i18n_noop("February"), i18n_noop("March"), i18n_noop("April"),
    i18n_noop("May"), i18n_noop("June"), i18n_noop("July"), i18n_noop("August"),
    i18n_noop("September"), i18n_noop("October"), i18n_noop("November"), i18n_noop("December")
  },
  .wday = {
    i18n_noop("Sun"), i18n_noop("Mon"), i18n_noop("Tue"), i18n_noop("Wed"),
    i18n_noop("Thu"), i18n_noop("Fri"), i18n_noop("Sat")
  },
  .weekday = {
    i18n_noop("Sunday"), i18n_noop("Monday"), i18n_noop("Tuesday"), i18n_noop("Wednesday"),
    i18n_noop("Thursday"), i18n_noop("Friday"), i18n_noop("Saturday")
  },

  .X_fmt = i18n_noop("%H:%M:%S"),
  .x_fmt = i18n_noop("%m/%d/%y"),
  .c_fmt = i18n_noop("%a %b %e %H:%M:%S %Y"),
  .r_fmt = i18n_noop("%I:%M:%S %p"),

  .am_pm_upcase   = { i18n_noop("AM"), i18n_noop("PM"), },
  .am_pm_downcase = { i18n_noop("am"), i18n_noop("pm"), },
};

const struct lc_time_T *time_locale_get(void) {
  return &_C_time_locale;
}
