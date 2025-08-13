/*
 * Copyright 2024 Google LLC
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

//! PreferredContentSize represents the display scale of all the app's UI components. The enum
//! contains all sizes that all platforms as a whole are capable of displaying, but each individual
//! platform may not be able to display all sizes.
//! @note As of version 4.1, platforms other than Emery cannot display extra large and Emery itself
//! cannot display small.
typedef enum PreferredContentSize {
  PreferredContentSizeSmall,
  PreferredContentSizeMedium,
  PreferredContentSizeLarge,
  PreferredContentSizeExtraLarge,
  NumPreferredContentSizes,
} PreferredContentSize;

#if !PUBLIC_SDK
//! TODO PBL-41920: This belongs in a platform specific location
#if PLATFORM_ROBERT || PLATFORM_OBELIX
#define PreferredContentSizeDefault PreferredContentSizeLarge
#else
#define PreferredContentSizeDefault PreferredContentSizeMedium
#endif
#endif

//! @internal
//! Switch on a PreferredContentSize. Defaults to SMALL value.
//! @note Optimal use of this does _not_ call a function for the `SIZE` argument! If you do, it
//! will be _evaluated on every comparison_, which is unlikely to be what you want!
#define PREFERRED_CONTENT_SIZE_SWITCH(SIZE, SMALL, MEDIUM, LARGE, EXTRALARGE) \
    (                                                                         \
      (SIZE == PreferredContentSizeMedium) ? (MEDIUM) :                       \
      (SIZE == PreferredContentSizeLarge) ? (LARGE) :                         \
      (SIZE == PreferredContentSizeExtraLarge) ? (EXTRALARGE) :               \
      (SMALL)                                                                 \
    )

//! Returns the user's preferred content size representing the scale of all the app's UI components
//! should use for display.
//! @return The user's \ref PreferredContentSize setting.
PreferredContentSize preferred_content_size(void);
