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
} PlatformType;

// Unit tests and the firmware don't define the SDK platform defines because reasons.
// Therefore, we need to switch on the platform for the platform type.
#if !defined(SDK)
  #if PLATFORM_TINTIN
    #define PBL_PLATFORM_TYPE_CURRENT PlatformTypeAplite
  #elif PLATFORM_SNOWY
    #define PBL_PLATFORM_TYPE_CURRENT PlatformTypeBasalt
  #elif PLATFORM_SPALDING
    #define PBL_PLATFORM_TYPE_CURRENT PlatformTypeChalk
  #elif PLATFORM_SILK
    #define PBL_PLATFORM_TYPE_CURRENT PlatformTypeDiorite
  #elif PLATFORM_ROBERT || PLATFORM_CALCULUS || PLATFORM_OBELIX
    #define PBL_PLATFORM_TYPE_CURRENT PlatformTypeEmery
  #elif PLATFORM_ASTERIX
    #define PBL_PLATFORM_TYPE_CURRENT PlatformTypeFlint
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
  #else
    #error "PBL_PLATFORM_TYPE_CURRENT couldn't be determined: No PBL_PLATFORM_* defined!"
  #endif
#endif

#define PBL_PLATFORM_SWITCH_DEFAULT(PLAT, DEFAULT, APLITE, BASALT, CHALK, DIORITE, EMERY, FLINT) (\
  ((PLAT) == PlatformTypeFlint) ? (FLINT) : \
  ((PLAT) == PlatformTypeEmery) ? (EMERY) : \
  ((PLAT) == PlatformTypeDiorite) ? (DIORITE) : \
  ((PLAT) == PlatformTypeChalk) ? (CHALK) : \
  ((PLAT) == PlatformTypeBasalt) ? (BASALT) : \
  ((PLAT) == PlatformTypeBasalt) ? (APLITE) : \
  (DEFAULT) \
)

// We fall back to Aplite because we need to fall back on _one_ of the given arguments.
// This prevents issues with sometimes using this for pointers/strings, and sometimes for ints.
//
// NOTE: Optimal use of this does _not_ call a function for the `PLAT` argument! If you do, it
//       will be _evaluated on every comparison_, which is unlikely to be what you want!
#define PBL_PLATFORM_SWITCH(PLAT, APLITE, BASALT, CHALK, DIORITE, EMERY, FLINT) \
  PBL_PLATFORM_SWITCH_DEFAULT(PLAT, APLITE, APLITE, BASALT, CHALK, DIORITE, EMERY, FLINT)


// INTERNAL
#define platform_type_get_name(plat) PBL_PLATFORM_SWITCH_DEFAULT(plat, \
  /*default*/ "unknown", \
  /*aplite*/ "aplite", \
  /*basalt*/ "basalt", \
  /*chalk*/ "chalk", \
  /*diorite*/ "diorite", \
  /*emery*/ "emery", \
  /*flint*/ "flint")
