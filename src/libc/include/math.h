/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-FileCopyrightText: 2025 Apache Software Foundation (ASF) */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#ifndef _HUGE_ENUF
#  define _HUGE_ENUF (1e+300)  /* _HUGE_ENUF*_HUGE_ENUF must overflow */
#endif

#define INFINITY   ((double)(_HUGE_ENUF * _HUGE_ENUF))
#define NAN        ((double)(INFINITY * 0.0F))

#define INFINITY_F ((float)INFINITY)
#define NAN_F      ((float)(INFINITY * 0.0F))

#define M_E        2.7182818284590452353602874713526625
#define M_LN10     2.3025850929940456840179914546843642
#define M_PI       3.1415926535897932384626433832795029
#define M_PI_2     1.5707963267948966192313216916397514

typedef float float_t;

typedef double double_t;

#define isinf(x)   (((x) == INFINITY) || ((x) == -INFINITY))
#define isinff(x)  (((x) == INFINITY_F) || ((x) == -INFINITY_F))
#define isnan(x)   ((x) != (x))
#define isnanf(x)  ((x) != (x))

double asin(double x);

double atan(double x);

double ceil(double x);

float ceilf(float x);

double cos(double x);

double copysign(double x, double y);

double exp(double x);

float expf(float x);

double fabs(double x);

float fabsf(float x);

float floorf(float x);

float fmaxf(float x, float y);

float fminf(float x, float y);

double fmod(double x, double div);

float logf(float x);

float log10f(float x);

double modf(double x, double *iptr);

float modff(float x, float *iptr);

double round(double d);

float roundf(float x);

double sin(double x);

double sqrt(double x);

float tanhf(float x);

#define signbit(x)           __builtin_signbit(x)