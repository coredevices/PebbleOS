/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

//! The system UI's style sheet: every value that changes with the user's preferred content size.
//! This file holds data only, so tests can link the real values without the rest of the theme.

#include "system_theme_style.h"

#include "applib/fonts/fonts.h"
#include "applib/platform.h"

static const SystemThemeStyle s_styles[NumPreferredContentSizes] = {
  [PreferredContentSizeSmall] =
      {
        .fonts =
            {
              [TextStyleFont_Header] = FONT_KEY_GOTHIC_18_BOLD,
#if !defined(CONFIG_RECOVERY_FW)
              [TextStyleFont_Title] = FONT_KEY_GOTHIC_18_BOLD,
              [TextStyleFont_Body] = FONT_KEY_GOTHIC_18,
#endif
              [TextStyleFont_Subtitle] = FONT_KEY_GOTHIC_18_BOLD,
              [TextStyleFont_Caption] = FONT_KEY_GOTHIC_14,
              [TextStyleFont_Footer] = FONT_KEY_GOTHIC_14,
              //! @note this is the same as the Title key (as that's what it's cloned from) until
              //! Small
              //!       is designed
              [TextStyleFont_MenuCellTitle] = FONT_KEY_GOTHIC_18_BOLD,
              //! @note this is the same as Medium until Small is designed
              [TextStyleFont_MenuCellSubtitle] = FONT_KEY_GOTHIC_18,
#if !defined(CONFIG_RECOVERY_FW)
              //! @note this is the same as Medium until Small is designed
              [TextStyleFont_TimeHeaderNumbers] = FONT_KEY_LECO_20_BOLD_NUMBERS,
#endif
              //! @note this is the same as Medium until Small is designed
              [TextStyleFont_TimeHeaderWords] = FONT_KEY_GOTHIC_14_BOLD,
              //! @note this is the same as Medium until Small is designed
              [TextStyleFont_PinSubtitle] = FONT_KEY_GOTHIC_18,
              //! @note this is the same as Medium until Small is designed
              [TextStyleFont_ParagraphHeader] = FONT_KEY_GOTHIC_14,
            },
      },
  [PreferredContentSizeMedium] =
      {
        .fonts =
            {
              [TextStyleFont_Header] = FONT_KEY_GOTHIC_18_BOLD,
#if !defined(CONFIG_RECOVERY_FW)
              [TextStyleFont_Title] = FONT_KEY_GOTHIC_24_BOLD,
              [TextStyleFont_Body] = FONT_KEY_GOTHIC_24_BOLD,
#endif
              [TextStyleFont_Subtitle] = FONT_KEY_GOTHIC_24_BOLD,
              [TextStyleFont_Caption] = FONT_KEY_GOTHIC_14,
              [TextStyleFont_Footer] = FONT_KEY_GOTHIC_18,
              [TextStyleFont_MenuCellTitle] = FONT_KEY_GOTHIC_24_BOLD,
              [TextStyleFont_MenuCellSubtitle] = FONT_KEY_GOTHIC_18,
#if !defined(CONFIG_RECOVERY_FW)
              [TextStyleFont_TimeHeaderNumbers] = FONT_KEY_LECO_20_BOLD_NUMBERS,
#endif
              [TextStyleFont_TimeHeaderWords] = FONT_KEY_GOTHIC_14_BOLD,
              [TextStyleFont_PinSubtitle] = FONT_KEY_GOTHIC_18,
              [TextStyleFont_ParagraphHeader] = FONT_KEY_GOTHIC_14,
            },
      },
  [PreferredContentSizeLarge] =
      {
        .fonts =
            {
              [TextStyleFont_Header] = FONT_KEY_GOTHIC_24_BOLD,
#if !defined(CONFIG_RECOVERY_FW)
              [TextStyleFont_Title] = FONT_KEY_GOTHIC_28_BOLD,
              [TextStyleFont_Body] = FONT_KEY_GOTHIC_28,
#endif
              [TextStyleFont_Subtitle] = FONT_KEY_GOTHIC_28,
              [TextStyleFont_Caption] = FONT_KEY_GOTHIC_18,
              [TextStyleFont_Footer] = FONT_KEY_GOTHIC_18,
              [TextStyleFont_MenuCellTitle] = FONT_KEY_GOTHIC_24_BOLD,
              [TextStyleFont_MenuCellSubtitle] = FONT_KEY_GOTHIC_24,
#if !defined(CONFIG_RECOVERY_FW)
              [TextStyleFont_TimeHeaderNumbers] = FONT_KEY_LECO_26_BOLD_NUMBERS_AM_PM,
#endif
              [TextStyleFont_TimeHeaderWords] = FONT_KEY_GOTHIC_18_BOLD,
              [TextStyleFont_PinSubtitle] = FONT_KEY_GOTHIC_24,
              [TextStyleFont_ParagraphHeader] = FONT_KEY_GOTHIC_18_BOLD,
            },
      },
  [PreferredContentSizeExtraLarge] = {
    .fonts = {
      [TextStyleFont_Header] = FONT_KEY_GOTHIC_28_BOLD,
#if !defined(CONFIG_RECOVERY_FW)
      [TextStyleFont_Title] = FONT_KEY_GOTHIC_36_BOLD,
      [TextStyleFont_Body] = FONT_KEY_GOTHIC_36,
#endif
      //! @note this is the same as Large until ExtraLarge is designed
      [TextStyleFont_Subtitle] = FONT_KEY_GOTHIC_28,
      [TextStyleFont_Caption] = FONT_KEY_GOTHIC_24,
      [TextStyleFont_Footer] = FONT_KEY_GOTHIC_24,
      [TextStyleFont_MenuCellTitle] = FONT_KEY_GOTHIC_28_BOLD,
      [TextStyleFont_MenuCellSubtitle] = FONT_KEY_GOTHIC_28,
#if !defined(CONFIG_RECOVERY_FW)
      //! @note this is the same as Large until ExtraLarge is designed
      [TextStyleFont_TimeHeaderNumbers] = FONT_KEY_LECO_26_BOLD_NUMBERS_AM_PM,
#endif
      //! @note this is the same as Large until ExtraLarge is designed
      [TextStyleFont_TimeHeaderWords] = FONT_KEY_GOTHIC_18_BOLD,
      //! @note this is the same as Large until ExtraLarge is designed
      [TextStyleFont_PinSubtitle] = FONT_KEY_GOTHIC_24,
      [TextStyleFont_ParagraphHeader] = FONT_KEY_GOTHIC_18_BOLD,
    },
  },
};

const SystemThemeStyle *system_theme_get_style_for_size(PreferredContentSize size) {
  return &s_styles[(size < NumPreferredContentSizes) ? size : PreferredContentSizeDefault];
}

const SystemThemeStyle *system_theme_get_style(void) {
  return system_theme_get_style_for_size(system_theme_get_content_size());
}
