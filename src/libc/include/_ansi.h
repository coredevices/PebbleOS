/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// this is all magic necessary for pre-ansi compatible code

#define _PTR void *
#define _AND ,
#define _NOARGS void
#define _CONST const
#define _VOLATILE volatile
#define _SIGNED signed
#define _DOTS , ...
#define _VOID void

#define _EXFUN(name, proto) name proto
#define _EXPARM(name, proto) (* name) proto

#define _DEFUN(name, arglist, args) name(args)
#define _DEFUN_VOID(name) name(_NOARGS)
#define _CAST_VOID (void)
#ifndef _LONG_DOUBLE
#define _LONG_DOUBLE long double
#endif
#ifndef _PARAMS
#define _PARAMS(paramlist) paramlist
#endif
