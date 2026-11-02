/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// SDK functions the generator cannot marshal: the variadic ones.

#include "samsa_runtime.h"

#include <stdio.h>

#include <pbl/util/size.h>

#include <applib/app_logging.h>
#include <applib/dict.h>
#include <applib/ui/animation.h>
#include <applib/ui/animation_private.h>

//! Format with the app's variadic arguments, which start at word slot s.
static void prv_format(SamsaCpu *c, unsigned s, const char *fmt, char *out, size_t size) {
  size_t len = 0;
  out[0] = '\0';
  while (*fmt && len + 1 < size) {
    if (*fmt != '%') {
      out[len++] = *fmt++;
      out[len] = '\0';
      continue;
    }
    // One conversion at a time, handed to the host's snprintf.
    char spec[32];
    size_t n = 0;
    spec[n++] = *fmt++;
    int stars[2], num_stars = 0;
    while (*fmt && strchr("-+ #0123456789.*", *fmt) && n < sizeof(spec) - 4) {
      if (*fmt == '*' && num_stars < 2) {
        stars[num_stars++] = (int32_t)samsa_arg(c, s++);
      }
      spec[n++] = *fmt++;
    }
    int longs = 0;
    while (*fmt && strchr("hlLqjzt", *fmt)) {
      longs += (*fmt == 'l' || *fmt == 'q' || *fmt == 'L') ? 1 : 0;
      fmt++;
    }
    char conv = *fmt ? *fmt++ : '\0';
    char *dst = out + len;
    size_t room = size - len;
    int w;
    switch (conv) {
      case 'd':
      case 'i':
      case 'u':
      case 'x':
      case 'X':
      case 'o':
      case 'c': {
        spec[n++] = 'l';
        spec[n++] = 'l';
        spec[n++] = conv;
        spec[n] = '\0';
        long long v;
        if (longs >= 2) {
          s = (s + 1) & ~1u;
          v = (long long)samsa_arg64(c, s);
          s += 2;
        } else {
          uint32_t word = samsa_arg(c, s++);
          v = (conv == 'd' || conv == 'i') ? (long long)(int32_t)word : (long long)word;
        }
        if (conv == 'c') {
          spec[n - 3] = 'c';
          spec[n - 2] = '\0';
          int ch = (int)v;
          w = num_stars == 1 ? snprintf(dst, room, spec, stars[0], ch)
                             : snprintf(dst, room, spec, ch);
          break;
        }
        w = num_stars == 2   ? snprintf(dst, room, spec, stars[0], stars[1], v)
            : num_stars == 1 ? snprintf(dst, room, spec, stars[0], v)
                             : snprintf(dst, room, spec, v);
        break;
      }
      case 's':
      case 'p': {
        spec[n++] = conv;
        spec[n] = '\0';
        uint32_t word = samsa_arg(c, s++);
        const void *arg = conv == 's' ? samsa_g2h(word) : (const void *)(uintptr_t)word;
        if (conv == 's' && !arg) {
          arg = "(null)";
        }
        w = num_stars == 2   ? snprintf(dst, room, spec, stars[0], stars[1], arg)
            : num_stars == 1 ? snprintf(dst, room, spec, stars[0], arg)
                             : snprintf(dst, room, spec, arg);
        break;
      }
      case 'f':
      case 'F':
      case 'e':
      case 'E':
      case 'g':
      case 'G': {
        spec[n++] = conv;
        spec[n] = '\0';
        s = (s + 1) & ~1u;
        uint64_t bits = samsa_arg64(c, s);
        s += 2;
        double v;
        memcpy(&v, &bits, sizeof(v));
        w = num_stars == 2   ? snprintf(dst, room, spec, stars[0], stars[1], v)
            : num_stars == 1 ? snprintf(dst, room, spec, stars[0], v)
                             : snprintf(dst, room, spec, v);
        break;
      }
      case '%':
        w = snprintf(dst, room, "%%");
        break;
      default:
        w = 0;
        break;
    }
    if (w > 0) {
      len += ((size_t)w < room) ? (size_t)w : room - 1;
    }
  }
}

// int snprintf(char *str, size_t n, const char *fmt, ...)
void samsa_manual_snprintf(SamsaCpu *c) {
  char *str = samsa_g2h(samsa_arg(c, 0));
  uint32_t n = samsa_arg(c, 1);
  const char *fmt = samsa_g2h(samsa_arg(c, 2));
  char buf[512];
  prv_format(c, 3, fmt ?: "", buf, sizeof(buf));
  size_t len = strlen(buf);
  if (str && n) {
    size_t copy = len < n - 1 ? len : n - 1;
    memcpy(str, buf, copy);
    str[copy] = '\0';
  }
  samsa_return32(c, (uint32_t)len);
}

// void app_log(uint8_t level, const char *file, int line, const char *fmt, ...)
void samsa_manual_app_log(SamsaCpu *c) {
  uint8_t level = (uint8_t)samsa_arg(c, 0);
  const char *file = samsa_g2h(samsa_arg(c, 1));
  int line = (int32_t)samsa_arg(c, 2);
  const char *fmt = samsa_g2h(samsa_arg(c, 3));
  char buf[256];
  prv_format(c, 4, fmt ?: "", buf, sizeof(buf));
  app_log(level, file ?: "?", line, "%s", buf);
  samsa_return32(c, 0);
}

// uint32_t dict_calc_buffer_size(uint32_t count, ...), with size_t sizes
void samsa_manual_dict_calc_buffer_size(SamsaCpu *c) {
  uint32_t count = samsa_arg(c, 0);
  uint32_t total = sizeof(Dictionary);
  for (uint32_t i = 0; i < count; i++) {
    total += samsa_arg(c, 1 + i) + sizeof(Tuple);
  }
  samsa_return32(c, total);
}

// Animation *animation_{sequence,spawn}_create(Animation *a, Animation *b, Animation *c, ...),
// up to a NULL
static void prv_animation_create(SamsaCpu *c,
                                 Animation *(*from_array)(Animation **array, uint32_t len)) {
  Animation *array[ANIMATION_MAX_CREATE_VARGS];
  uint32_t len = 0;
  while (len < ARRAY_LENGTH(array)) {
    uint32_t g = samsa_arg(c, len);
    if (g == 0) {
      break;
    }
    array[len++] = samsa_g2h(g);
  }
  samsa_return32(c, len < 2 ? 0 : samsa_h2g(from_array(array, len)));
}

void samsa_manual_animation_sequence_create(SamsaCpu *c) {
  prv_animation_create(c, animation_sequence_create_from_array);
}

void samsa_manual_animation_spawn_create(SamsaCpu *c) {
  prv_animation_create(c, animation_spawn_create_from_array);
}
