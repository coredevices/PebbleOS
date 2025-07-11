/*
 * Copyright 2024 Google LLC
 * Copyright 2025 Apache Software Foundation (ASF)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#ifndef _HUGE_ENUF
#  define _HUGE_ENUF (1e+300)  /* _HUGE_ENUF*_HUGE_ENUF must overflow */
#endif

#define INFINITY   ((double)(_HUGE_ENUF * _HUGE_ENUF))

#define INFINITY_F ((float)INFINITY)
#define NAN_F      ((float)(INFINITY * 0.0F))

#define M_E        2.7182818284590452353602874713526625

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
