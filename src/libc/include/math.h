/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-FileCopyrightText: 2025 Apache Software Foundation (ASF) */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#ifndef _HUGE_ENUF
#  define _HUGE_ENUF (1e+300)  /* _HUGE_ENUF*_HUGE_ENUF must overflow */
#endif

#define INFINITY   ((double)(_HUGE_ENUF * _HUGE_ENUF))

#define INFINITY_F ((float)INFINITY)
#define NAN_F      ((float)(INFINITY * 0.0F))

#define M_E        2.7182818284590452353602874713526625
#define M_PI       3.1415926535897932384626433832795029

typedef float float_t;

typedef double double_t;

#define isinff(x)  (((x) == INFINITY_F) || ((x) == -INFINITY_F))
#define isnanf(x)  ((x) != (x))

float ceilf(float x);

float expf(float x);

float fabsf(float x);

float floorf(float x);

float fmaxf(float x, float y);

float fminf(float x, float y);

float logf(float x);

float modff(float x, float *iptr);

double round(double d);

float roundf(float x);