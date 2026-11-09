/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

typedef enum PlatformType {
  PlatformTypeAplite,
  PlatformTypeBasalt,
  PlatformTypeChalk,
  PlatformTypeDiorite,
  PlatformTypeEmery,
  PlatformTypeFlint,
  PlatformTypeGabbro
} PlatformType;

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
#define DISPLAY_FRAMEBUFFER_BYTES \
  (ROUND_TO_MOD_CEIL(PBL_DISPLAY_WIDTH, 32) / 8 * PBL_DISPLAY_HEIGHT)
#else
#define DISPLAY_FRAMEBUFFER_BYTES (PBL_DISPLAY_WIDTH * PBL_DISPLAY_HEIGHT)
#endif

// Variant boards should go before their base platforms, as they define the base platform too.
#ifdef CONFIG_PLATFORM_EMERY
#define PBL_PLATFORM_TYPE_CURRENT PlatformTypeEmery
#elifdef CONFIG_PLATFORM_FLINT
#define PBL_PLATFORM_TYPE_CURRENT PlatformTypeFlint
#elifdef CONFIG_PLATFORM_GABBRO
#define PBL_PLATFORM_TYPE_CURRENT PlatformTypeGabbro
#else
#error "PBL_PLATFORM_TYPE_CURRENT couldn't be determined: No PLATFORM_* defined!"
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
