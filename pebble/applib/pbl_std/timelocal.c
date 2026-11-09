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

#include "timelocal.h"

#include <stddef.h>

#include <pbl/i18n/i18n.h>

#define LCTIME_SIZE (sizeof(struct lc_time_T) / sizeof(char *))

static const struct lc_time_T _C_time_locale = {
  .mon =
      {pbl_i18n_ctx_noop("MonthAbbr", "Jan"), pbl_i18n_ctx_noop("MonthAbbr", "Feb"),
       pbl_i18n_ctx_noop("MonthAbbr", "Mar"), pbl_i18n_ctx_noop("MonthAbbr", "Apr"),
       pbl_i18n_ctx_noop("MonthAbbr", "May"), pbl_i18n_ctx_noop("MonthAbbr", "Jun"),
       pbl_i18n_ctx_noop("MonthAbbr", "Jul"), pbl_i18n_ctx_noop("MonthAbbr", "Aug"),
       pbl_i18n_ctx_noop("MonthAbbr", "Sep"), pbl_i18n_ctx_noop("MonthAbbr", "Oct"),
       pbl_i18n_ctx_noop("MonthAbbr", "Nov"), pbl_i18n_ctx_noop("MonthAbbr", "Dec")},
  .month =
      {pbl_i18n_noop("January"), pbl_i18n_noop("February"), pbl_i18n_noop("March"),
       pbl_i18n_noop("April"), pbl_i18n_noop("May"), pbl_i18n_noop("June"), pbl_i18n_noop("July"),
       pbl_i18n_noop("August"), pbl_i18n_noop("September"), pbl_i18n_noop("October"),
       pbl_i18n_noop("November"), pbl_i18n_noop("December")},
  .wday =
      {pbl_i18n_noop("Sun"), pbl_i18n_noop("Mon"), pbl_i18n_noop("Tue"), pbl_i18n_noop("Wed"),
       pbl_i18n_noop("Thu"), pbl_i18n_noop("Fri"), pbl_i18n_noop("Sat")},
  .weekday =
      {pbl_i18n_noop("Sunday"), pbl_i18n_noop("Monday"), pbl_i18n_noop("Tuesday"),
       pbl_i18n_noop("Wednesday"), pbl_i18n_noop("Thursday"), pbl_i18n_noop("Friday"),
       pbl_i18n_noop("Saturday")},

  .X_fmt = pbl_i18n_noop("%H:%M:%S"),
  .x_fmt = pbl_i18n_noop("%m/%d/%y"),
  .c_fmt = pbl_i18n_noop("%a %b %e %H:%M:%S %Y"),
  .r_fmt = pbl_i18n_noop("%I:%M:%S %p"),

  .am_pm_upcase =
      {
        pbl_i18n_noop("AM"),
        pbl_i18n_noop("PM"),
      },
  .am_pm_downcase = {
    pbl_i18n_noop("am"),
    pbl_i18n_noop("pm"),
  },
};

const struct lc_time_T *time_locale_get(void) {
  return &_C_time_locale;
}
