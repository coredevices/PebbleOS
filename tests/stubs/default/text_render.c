/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <inttypes.h>

#include <pbl/kernel/compiler.h>

#include <applib/fonts/fonts_private.h>
#include <applib/graphics/gtypes.h>
#include <applib/graphics/text_resources.h>

PBL_WEAK void render_glyph(GContext *ctx, uint32_t codepoint, FontInfo *font, GRect cursor) {
}
