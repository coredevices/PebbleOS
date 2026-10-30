/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>

//! @file app_malloc.h
//! Allocations from the app heap, which lives in the app's own RAM and is reset between apps.
//! The _check variants never return NULL: running out of memory kills the app.

void *app_malloc(size_t bytes);
void *app_malloc_check(size_t bytes);
void *app_realloc(void *ptr, size_t bytes);
void *app_zalloc(size_t size);
void *app_zalloc_check(size_t size);
void *app_calloc(size_t count, size_t size);
void *app_calloc_check(size_t count, size_t size);
void app_free(void *ptr);
char *app_strdup(const char *s);
