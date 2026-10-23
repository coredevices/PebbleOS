/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <applib/platform.h>
#include <applib/preferred_content_size.h>
#include <clar.h>
#include <shell/prefs.h>
#include <shell/system_theme.h>

// Stubs
///////////////

// Tests
///////////////

PreferredContentSize prv_convert_content_size_between_platforms(PreferredContentSize size,
                                                                PlatformType from_platform,
                                                                PlatformType to_platform);

void test_system_theme__convert_content_size_between_platforms(void) {
  // Converting between the same platform should be return the input
  cl_assert_equal_i(prv_convert_content_size_between_platforms(
                        PreferredContentSizeMedium, PlatformTypeBasalt, PlatformTypeBasalt),
                    PreferredContentSizeMedium);

  // Converting between two platforms with the same default content size should return the input
  cl_assert_equal_i(prv_convert_content_size_between_platforms(
                        PreferredContentSizeMedium, PlatformTypeDiorite, PlatformTypeBasalt),
                    PreferredContentSizeMedium);

  // Passing in an invalid from_platform or to_platform should assert
  cl_assert_panic(prv_convert_content_size_between_platforms(
      PreferredContentSizeSmall, PlatformTypeGabbro + 1, PlatformTypeBasalt));
  cl_assert_panic(prv_convert_content_size_between_platforms(
      PreferredContentSizeSmall, PlatformTypeBasalt, PlatformTypeGabbro + 1));

  // Converting from Emery to Basalt should return one size smaller
  cl_assert_equal_i(prv_convert_content_size_between_platforms(
                        PreferredContentSizeLarge, PlatformTypeEmery, PlatformTypeBasalt),
                    PreferredContentSizeMedium);

  // Converting from Aplite to Emery should return one size larger
  cl_assert_equal_i(prv_convert_content_size_between_platforms(
                        PreferredContentSizeLarge, PlatformTypeAplite, PlatformTypeEmery),
                    PreferredContentSizeExtraLarge);

  // Converting from Diorite to Emery (one size up) with the maximum size should be clipped
  cl_assert_equal_i(prv_convert_content_size_between_platforms(
                        PreferredContentSizeExtraLarge, PlatformTypeDiorite, PlatformTypeEmery),
                    PreferredContentSizeExtraLarge);

  // Converting from Emery to Chalk (one size down) with the minimum size should be clipped
  cl_assert_equal_i(prv_convert_content_size_between_platforms(
                        PreferredContentSizeSmall, PlatformTypeEmery, PlatformTypeChalk),
                    PreferredContentSizeSmall);
}

void test_system_theme__get_colors_and_app_color_mode(void) {
  shell_prefs_set_theme_highlight_color(GColorSunsetOrange);
  shell_prefs_set_theme_secondary_color(GColorLavenderIndigo);
  shell_prefs_set_theme_background_color(GColorBlack);
  shell_prefs_set_theme_text_color(GColorYellow);

  SystemThemeColors colors = {};
  system_theme_get_colors(&colors);
  cl_assert_equal_i(colors.primary_color.argb,
                    PBL_IF_COLOR_ELSE(GColorSunsetOrange, GColorBlack).argb);
  cl_assert_equal_i(colors.secondary_color.argb,
                    PBL_IF_COLOR_ELSE(GColorLavenderIndigo, GColorBlack).argb);
  cl_assert_equal_i(colors.background_color.argb, GColorBlack.argb);
  cl_assert_equal_i(colors.text_color.argb, GColorYellow.argb);

  // In ColorsAndBackground mode, default app colors are overridden by theme colors
  shell_prefs_set_theme_app_color_mode(ThemeAppColorMode_ColorsAndBackground);
  SystemThemeColors app_colors = {};
  system_theme_get_app_colors(GColorJaegerGreen, GColorDarkGreen, &app_colors);
  cl_assert_equal_i(app_colors.primary_color.argb,
                    PBL_IF_COLOR_ELSE(GColorSunsetOrange, GColorBlack).argb);
  cl_assert_equal_i(app_colors.secondary_color.argb,
                    PBL_IF_COLOR_ELSE(GColorLavenderIndigo, GColorBlack).argb);
  cl_assert_equal_i(app_colors.background_color.argb, GColorBlack.argb);

  // In BackgroundOnly mode, default app primary/secondary colors are preserved while background is
  // themed
  shell_prefs_set_theme_app_color_mode(ThemeAppColorMode_BackgroundOnly);
  system_theme_get_app_colors(GColorJaegerGreen, GColorDarkGreen, &app_colors);
  cl_assert_equal_i(app_colors.primary_color.argb, GColorJaegerGreen.argb);
  cl_assert_equal_i(app_colors.secondary_color.argb, GColorDarkGreen.argb);
  cl_assert_equal_i(app_colors.background_color.argb, GColorBlack.argb);
  cl_assert_equal_i(app_colors.text_color.argb, GColorYellow.argb);

  system_theme_reset_to_default(false);
  cl_assert_equal_i(shell_prefs_get_theme_background_color().argb, GColorWhite.argb);
}
