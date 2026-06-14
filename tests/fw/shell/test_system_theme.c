/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "applib/platform.h"
#include "applib/preferred_content_size.h"

// Stubs
///////////////

#include "stubs_analytics.h"
#include "stubs_fonts.h"
#include "stubs_logging.h"
#include "stubs_passert.h"
#include "stubs_process_manager.h"
#include "stubs_shell_prefs.h"

// Tests
///////////////

PreferredContentSize prv_convert_content_size_between_platforms(PreferredContentSize size,
                                                                PlatformType from_platform,
                                                                PlatformType to_platform);

void test_system_theme__convert_content_size_between_platforms(void) {
  // Converting between the same platform should be return the input
  cl_assert_equal_i(prv_convert_content_size_between_platforms(PreferredContentSizeMedium,
                                                               PlatformTypeBasalt,
                                                               PlatformTypeBasalt),
                    PreferredContentSizeMedium);

  // Converting between two platforms with the same default content size should return the input
  cl_assert_equal_i(prv_convert_content_size_between_platforms(PreferredContentSizeMedium,
                                                               PlatformTypeDiorite,
                                                               PlatformTypeBasalt),
                    PreferredContentSizeMedium);

  // Passing in an invalid from_platform or to_platform should assert
  cl_assert_passert(prv_convert_content_size_between_platforms(PreferredContentSizeSmall,
                                                               PlatformTypeEmery + 1,
                                                               PlatformTypeBasalt));
  cl_assert_passert(prv_convert_content_size_between_platforms(PreferredContentSizeSmall,
                                                               PlatformTypeBasalt,
                                                               PlatformTypeEmery + 1));

  // Converting from Emery to Basalt should return one size smaller
  cl_assert_equal_i(prv_convert_content_size_between_platforms(PreferredContentSizeLarge,
                                                               PlatformTypeEmery,
                                                               PlatformTypeBasalt),
                    PreferredContentSizeMedium);

  // Converting from Aplite to Emery should return one size larger
  cl_assert_equal_i(prv_convert_content_size_between_platforms(PreferredContentSizeLarge,
                                                               PlatformTypeAplite,
                                                               PlatformTypeEmery),
                    PreferredContentSizeExtraLarge);

  // Converting from Diorite to Emery (one size up) with the maximum size should be clipped
  cl_assert_equal_i(prv_convert_content_size_between_platforms(PreferredContentSizeExtraLarge,
                                                               PlatformTypeDiorite,
                                                               PlatformTypeEmery),
                    PreferredContentSizeExtraLarge);

  // Converting from Emery to Chalk (one size down) with the minimum size should be clipped
  cl_assert_equal_i(prv_convert_content_size_between_platforms(PreferredContentSizeSmall,
                                                               PlatformTypeEmery,
                                                               PlatformTypeChalk),
                    PreferredContentSizeSmall);
}
