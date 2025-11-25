/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

// When compiling test, the host OS might have conflicting defines for this:
#undef ntohs
#undef htons
#undef ntohl
#undef htonl
#undef ltohs
#undef ltohl

static inline uint16_t ntohs(uint16_t v) {
  // return ((v & 0x00ff) << 8) | ((v & 0xff00) >> 8);
  return __builtin_bswap16(v);
}

static inline uint16_t htons(uint16_t v) {
  return ntohs(v);
}

static inline uint32_t ntohl(uint32_t v) {
  // return ((v & 0x000000ff) << 24) |
  //        ((v & 0x0000ff00) << 8) |
  //        ((v & 0x00ff0000) >> 8) |
  //        ((v & 0xff000000) >> 24);
  return __builtin_bswap32(v);
}

static inline uint32_t htonl(uint32_t v) {
  return ntohl(v);
}

#define ltohs(v) (v)
#define ltohl(v) (v)

// Types for values in network byte-order. They are wrapped in structs so that
// the compiler will disallow implicit casting of these types to or from
// integral types. This way it is a compile error to try using variables of
// these types without first performing a byte-order conversion.
// There is no overhead for wrapping the values in structs.
typedef struct net16 {
  uint16_t v;
} net16;

typedef struct net32 {
  uint32_t v;
} net32;

static inline uint16_t ntoh16(net16 net) {
  return ntohs(net.v);
}

static inline net16 hton16(uint16_t v) {
  return (net16){ htons(v) };
}

static inline uint32_t ntoh32(net32 net) {
  return ntohl(net.v);
}

static inline net32 hton32(uint32_t v) {
  return (net32){ htonl(v) };
}
