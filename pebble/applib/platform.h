/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum PlatformType {
  PlatformTypeAplite,
  PlatformTypeBasalt,
  PlatformTypeChalk,
  PlatformTypeDiorite,
  PlatformTypeEmery,
  PlatformTypeFlint,
  PlatformTypeGabbro
} PlatformType;

//! Hardware a watch may have. An app that runs on more than one kind of watch asks whether the
//! one it is running on has a feature with \ref platform_has_feature.
typedef enum PlatformFeature {
  //! A microphone, for dictation
  PlatformFeatureMicrophone,
  //! A smartstrap connector
  PlatformFeatureSmartstrap,
  //! A smartstrap connector that can power an accessory
  PlatformFeatureSmartstrapPower,
  //! Step counting and sleep tracking
  PlatformFeatureHealth,
  //! A compass
  PlatformFeatureCompass,
  //! A touch screen
  PlatformFeatureTouch,
  //! A backlight that can change its colour
  PlatformFeatureRGBBacklight,
  //! A speaker
  PlatformFeatureSpeaker,
} PlatformFeature;

//! Whether the watch the app is running on has a feature.
//! @param feature The feature to ask for
//! @return true if the watch has the feature, false if it does not or the feature is not known
//! to its firmware
bool platform_has_feature(PlatformFeature feature);

//! @internal
//! The features of the platform, as the bits of PebblePlatformFeature.
uint32_t platform_get_features(void);

// Unit tests and the firmware don't define the SDK platform defines because reasons.
// Therefore, we need to switch on the platform for the platform type.
#if !defined(SDK)
#ifdef CONFIG_PLATFORM_COLOR
#define PBL_BW    0
#define PBL_COLOR 1
#else
#define PBL_BW    1
#define PBL_COLOR 0
#endif

#ifdef CONFIG_PLATFORM_ROUND
#define PBL_RECT  0
#define PBL_ROUND 1
#else
#define PBL_RECT  1
#define PBL_ROUND 0
#endif

#define PBL_DISPLAY_WIDTH  CONFIG_PLATFORM_DISPLAY_WIDTH
#define PBL_DISPLAY_HEIGHT CONFIG_PLATFORM_DISPLAY_HEIGHT

#define LEGACY_2X_DISP_COLS CONFIG_PLATFORM_LEGACY_2X_DISPLAY_WIDTH
#define LEGACY_2X_DISP_ROWS CONFIG_PLATFORM_LEGACY_2X_DISPLAY_HEIGHT
#define LEGACY_3X_DISP_COLS CONFIG_PLATFORM_LEGACY_3X_DISPLAY_WIDTH
#define LEGACY_3X_DISP_ROWS CONFIG_PLATFORM_LEGACY_3X_DISPLAY_HEIGHT

#if PBL_BW
#define DISPLAY_FRAMEBUFFER_BYTES (PBL_ROUNDUP(PBL_DISPLAY_WIDTH, 32) / 8 * PBL_DISPLAY_HEIGHT)
#else
#define DISPLAY_FRAMEBUFFER_BYTES (PBL_DISPLAY_WIDTH * PBL_DISPLAY_HEIGHT)
#endif

// A platform that is not one of these is the one it is most like: PlatformType only tells
// apart what the system draws differently, and that goes by shape and colour depth.
#ifdef CONFIG_PLATFORM_EMERY
#define PBL_PLATFORM_TYPE_CURRENT PlatformTypeEmery
#elifdef CONFIG_PLATFORM_FLINT
#define PBL_PLATFORM_TYPE_CURRENT PlatformTypeFlint
#elifdef CONFIG_PLATFORM_GABBRO
#define PBL_PLATFORM_TYPE_CURRENT PlatformTypeGabbro
#elifdef CONFIG_PLATFORM_ROUND
#define PBL_PLATFORM_TYPE_CURRENT PlatformTypeGabbro
#elifdef CONFIG_PLATFORM_COLOR
#define PBL_PLATFORM_TYPE_CURRENT PlatformTypeEmery
#else
#define PBL_PLATFORM_TYPE_CURRENT PlatformTypeFlint
#endif
#else
#if PBL_PLATFORM_APLITE
#define PBL_PLATFORM_TYPE_CURRENT PlatformTypeAplite
#elif PBL_PLATFORM_BASALT
#define PBL_PLATFORM_TYPE_CURRENT PlatformTypeBasalt
#elif PBL_PLATFORM_CHALK
#define PBL_PLATFORM_TYPE_CURRENT PlatformTypeChalk
#elif PBL_PLATFORM_DIORITE
#define PBL_PLATFORM_TYPE_CURRENT PlatformTypeDiorite
#elif PBL_PLATFORM_EMERY
#define PBL_PLATFORM_TYPE_CURRENT PlatformTypeEmery
#elif PBL_PLATFORM_FLINT
#define PBL_PLATFORM_TYPE_CURRENT PlatformTypeFlint
#elif PBL_PLATFORM_GABBRO
#define PBL_PLATFORM_TYPE_CURRENT PlatformTypeGabbro
#elif defined(PBL_ROUND)
#define PBL_PLATFORM_TYPE_CURRENT PlatformTypeGabbro
#elif defined(PBL_COLOR)
#define PBL_PLATFORM_TYPE_CURRENT PlatformTypeEmery
#elif defined(PBL_BW)
#define PBL_PLATFORM_TYPE_CURRENT PlatformTypeFlint
#else
#error "PBL_PLATFORM_TYPE_CURRENT couldn't be determined: No PBL_PLATFORM_* defined!"
#endif
#endif

#define PBL_PLATFORM_SWITCH_DEFAULT(PLAT, DEFAULT, APLITE, BASALT, CHALK, DIORITE, EMERY, FLINT, \
                                    GABBRO)                                                      \
  (((PLAT) == PlatformTypeFlint)     ? (FLINT)                                                   \
   : ((PLAT) == PlatformTypeEmery)   ? (EMERY)                                                   \
   : ((PLAT) == PlatformTypeDiorite) ? (DIORITE)                                                 \
   : ((PLAT) == PlatformTypeChalk)   ? (CHALK)                                                   \
   : ((PLAT) == PlatformTypeBasalt)  ? (BASALT)                                                  \
   : ((PLAT) == PlatformTypeAplite)  ? (APLITE)                                                  \
   : ((PLAT) == PlatformTypeGabbro)  ? (GABBRO)                                                  \
                                     : (DEFAULT))

// We fall back to Aplite because we need to fall back on _one_ of the given arguments.
// This prevents issues with sometimes using this for pointers/strings, and sometimes for ints.
//
// NOTE: Optimal use of this does _not_ call a function for the `PLAT` argument! If you do, it
//       will be _evaluated on every comparison_, which is unlikely to be what you want!
#define PBL_PLATFORM_SWITCH(PLAT, APLITE, BASALT, CHALK, DIORITE, EMERY, FLINT, GABBRO) \
  PBL_PLATFORM_SWITCH_DEFAULT(PLAT, APLITE, APLITE, BASALT, CHALK, DIORITE, EMERY, FLINT, GABBRO)

// INTERNAL
#define platform_type_get_name(plat)                                                         \
  PBL_PLATFORM_SWITCH_DEFAULT(plat, /*default*/ "unknown", /*aplite*/ "aplite",              \
                              /*basalt*/ "basalt", /*chalk*/ "chalk", /*diorite*/ "diorite", \
                              /*emery*/ "emery", /*flint*/ "flint", /*gabbro*/ "gabbro")
