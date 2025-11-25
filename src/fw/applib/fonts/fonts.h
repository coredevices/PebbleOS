/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/fonts/fonts_private.h"
#include "resource/resource.h"

#if !defined(SDK)
#include "font_resource_keys.auto.h"
#endif

//! @addtogroup Graphics
//! @{
//!   @addtogroup Fonts
//! @see \ref TextLayer
//! @see \ref TextDrawing
//! @see \ref text_layer_set_font
//! @see \ref graphics_draw_text
//!   @{

//! Pointer to opaque font data structure.
//! @see \ref fonts_load_custom_font()
//! @see \ref text_layer_set_font()
//! @see \ref graphics_draw_text()
typedef FontInfo* GFont;

//! @internal
//! Gets the fallback system font (14pt Raster Gothic)
GFont fonts_get_fallback_font(void);

//! Loads a system font corresponding to the specified font key.
//! @param font_key The string key of the font to load. See
//! <a href="https://developer.pebble.com/guides/app-resources/system-fonts/">System
//! Fonts</a> guide for a list of system fonts.
//! @return An opaque pointer to the loaded font, or, a pointer to the default
//! (fallback) font if the specified font cannot be loaded.
//! @note This may load a font from the flash peripheral into RAM.
GFont fonts_get_system_font(const char *font_key);

GFont fonts_get_system_emoji_font_for_size(unsigned int font_size);

//! Loads a custom font.
//! @param handle The resource handle of the font to load. See resource_ids.auto.h
//! for a list of resource IDs, and use \ref resource_get_handle() to obtain the resource handle.
//! @return An opaque pointer to the loaded font, or a pointer to the default
//! (fallback) font if the specified font cannot be loaded.
//! @see Read the <a href="http://developer.getpebble.com/guides/pebble-apps/resources/">App
//! Resources</a> guide on how to embed a font into your app.
//! @note this may load a font from the flash peripheral into RAM.
GFont fonts_load_custom_font(ResHandle handle);

//! @internal
//! firmware-only access version of fonts_load_custom_font
GFont fonts_load_custom_font_system(ResAppNum app_num, uint32_t resource_id);

//! Unloads the specified custom font and frees the memory that is occupied by
//! it.
//! @note When an application exits, the system automatically unloads all fonts
//! that have been loaded.
//! @param font The font to unload.
void fonts_unload_custom_font(GFont font);

//! @internal
uint8_t fonts_get_font_height(GFont font);

//! @internal
// Get the vertical offset of the top of the font's caps from the origin of a text frame
// Currently only an approximation, see PBL-25709
int16_t fonts_get_font_cap_offset(GFont font);

//!   @} // end addtogroup Fonts
//! @} // end addtogroup Graphics
