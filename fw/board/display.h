/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

// The SDK exporter can't crawl the framebuffer headers.
#if !defined(SDK)

#include <board/display_platform.h>

#endif // !SDK
