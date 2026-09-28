/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#if defined(CONFIG_SHELL) && defined(CONFIG_PERFORMANCE_TESTS)

#include <pbl/drivers/watchdog.h>
#include <pbl/kernel/thread.h>
#include <pbl/shell/shell.h>
#include <pbl/task_wdt/task_wdt.h>

#include "applib/fonts/fonts.h"
#include "applib/graphics/framebuffer.h"
#include "applib/graphics/graphics.h"
#include "applib/graphics/gtypes.h"
#include "kernel/event_loop.h"
#include "pbl/services/compositor/compositor.h"
#include "pbl/util/math.h"
#include "pbl/util/size.h"
#include "system/profiler.h"

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

// Average this many iterations of the text test for getting useful perf numbers.
#define PERFTEST_TEXT_ITERATIONS 5

static GContext s_perftest_ctx = {};

static GContext *prv_perftest_get_context(void) {
  GContext *ctx = &s_perftest_ctx;
  FrameBuffer *fb = compositor_get_framebuffer();
  memset(fb->buffer, 0xff, FRAMEBUFFER_SIZE_BYTES);
  graphics_context_init(ctx, fb, GContextInitializationMode_App);
  return ctx;
}

static int prv_perftest_line(const struct pbl_shell *sh, const char *do_aa, const char *width) {
  bool aa_enable;
  unsigned long stroke_width;

  if (strcmp(do_aa, "aa") == 0) {
    aa_enable = true;
  } else if (strcmp(do_aa, "noaa") == 0) {
    aa_enable = false;
  } else {
    pbl_shell_error(sh, "incorrect aa argument, must be 'aa' or 'noaa'");
    return -EINVAL;
  }

  if (pbl_shell_strtoul(width, &stroke_width) != 0 || stroke_width > UINT8_MAX) {
    pbl_shell_error(sh, "invalid width '%s'", width);
    return -EINVAL;
  }

  watchdog_feed();

  GContext *ctx = prv_perftest_get_context();

  GColor color = {.argb = (uint8_t)0x33};
  graphics_context_set_stroke_color(ctx, color);
  graphics_context_set_antialiased(ctx, aa_enable);
  graphics_context_set_stroke_width(ctx, stroke_width);

  profiler_start();
  // 45 degrees
  graphics_draw_line(ctx, GPoint(0, 0), GPoint(DISP_COLS, DISP_ROWS));
  // ~63 degrees
  graphics_draw_line(ctx, GPoint(DISP_COLS / 2, 0), GPoint(DISP_COLS, DISP_ROWS));
  // ~33 degrees
  graphics_draw_line(ctx, GPoint(0, DISP_ROWS / 3), GPoint(DISP_COLS, DISP_ROWS));
  // ~53 degrees
  graphics_draw_line(ctx, GPoint(DISP_COLS / 4, 0), GPoint(DISP_COLS, DISP_ROWS));
  // ~39 degrees
  graphics_draw_line(ctx, GPoint(0, DISP_ROWS / 5), GPoint(DISP_COLS, DISP_ROWS));
  profiler_stop();

  uint32_t total_time = profiler_get_total_duration(false);
  uint32_t us = profiler_get_total_duration(true);
  pbl_shell_print(sh, "%s, %s, %" PRIu32 ", %" PRIu32, do_aa, width, us, total_time);
  return 0;
}

static int prv_cmd_line(const struct pbl_shell *sh, size_t argc, char **argv) {
  return prv_perftest_line(sh, argv[1], argv[2]);
}

static int prv_cmd_line_all(const struct pbl_shell *sh, size_t argc, char **argv) {
  static const char *const aa[] = {"noaa", "aa"};
  static const char *const widths[] = {"8", "6", "5", "4", "3", "2", "1"};

  pbl_shell_print(sh, "Antialiasing?, Width, Total time (us), Total cycles");
  for (size_t i = 0; i < ARRAY_LENGTH(aa); i++) {
    for (size_t j = 0; j < ARRAY_LENGTH(widths); j++) {
      prv_perftest_line(sh, aa[i], widths[j]);
    }
  }

  return 0;
}

enum {
  TestString_Best,    // The best case
  TestString_Worst,   // Entirely unique characters, in order to miss the font cache every time
  TestString_Typical, // A very typical notification
  TestStringCount,
};

enum {
  TestStringFont_Gothic18,
  TestStringFont_Gothic24B,
  TestStringFont_Other,
  TestStringFontCount,
};

// A very big number
#define STRING_LENGTH_MAX 99999

typedef struct PerftestTextString {
  const char *string;
  size_t lengths[TestStringFontCount];
} PerftestTextString;

static const PerftestTextString s_perftest_text_strings[TestStringCount] = {
  [TestString_Best] =
      {
        .string = "MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM"
                  "MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM"
                  "MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM"
                  "MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM"
                  "MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM"
                  "MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM"
                  "MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM",
        .lengths =
            {
#if defined(CONFIG_BOARD_OBELIX) || defined(CONFIG_BOARD_GETAFIX)
              [TestStringFont_Gothic18] = 204,
              [TestStringFont_Gothic24B] = 144,
              [TestStringFont_Other] = STRING_LENGTH_MAX,
#endif
            },
      },
  [TestString_Worst] =
      {
        .string = "`1234567890-=qwertyuiop[]\\asdfghjkl;'zxcvbnm,./~!@#$%%^&*()_+QWERTYUIOP{}|A"
                  "SDFGHJKL:\"ZXCVBNM<>?èéêëēėęÿûüùúūîïíī"
                  "įìôöòóœøōõàáâäæãåāßśšłžźżçćčñń∑´®†¥¨ˆπ"
                  "∂ƒ©˙∆˚¬…Ω≈√∫˜µ≤≥÷¡™£¢∞§¶•ªº–≠`“‘"
                  "«ÈÉÊËĒĖĘŸÛÜÙÚŪÎÏÍĪĮÌÔÖÒÓŒØŌÕÀÁÂÄÆÃÅĀŚ"
                  "ŠŁŽŹŻÇĆČÑŃ∑ˇ∏”’»˝¸˛◊ı˜¯˘¿"
                  "あいうえおかきくけこさしすせそたちつてとなに"
                  "ぬねのはひふへほまみむめもやゆよらりるれろわ"
                  "をんアイウエオサシスセソタチツテトナニヌネノ"
                  "ハヒフヘホマミムメモヤユヨラリルレロワヲン",
        .lengths =
            {
#if defined(CONFIG_BOARD_OBELIX) || defined(CONFIG_BOARD_GETAFIX)
              [TestStringFont_Gothic18] = 579,
              [TestStringFont_Gothic24B] = 291,
              [TestStringFont_Other] = STRING_LENGTH_MAX,
#endif
            },
      },
  [TestString_Typical] = {
    .string = "Brian Gomberg\n"
              "Re: Robert stand-up 06/06 • "
              "y: - DDAD (enabling system apps to take advantage of memory mapped "
              "FLASH access on Robe"
              "\xe2\x80\xa6",
    .lengths = {
#if defined(CONFIG_BOARD_OBELIX) || defined(CONFIG_BOARD_GETAFIX)
      [TestStringFont_Gothic18] = 134,
      [TestStringFont_Gothic24B] = 134,
      [TestStringFont_Other] = STRING_LENGTH_MAX,
#endif
    },
  },
};

#define TEXT_ALIGNMENT (GTextAlignmentCenter)
#define TEXT_OVERFLOW  (GTextOverflowModeWordWrap)

typedef struct PerftestTextArguments {
  const struct pbl_shell *sh;
  const char *font_key;
  int text_index;
  int y_offset;
  const char *type_str;
  const char *y_offset_str;
} PerftestTextArguments;

static PerftestTextArguments s_perftest_text_arguments;
static volatile bool s_perftest_text_running;
static char s_text_test_str[1024];

static void prv_perftest_text_main(void *data) {
  const PerftestTextArguments *args = &s_perftest_text_arguments;

  profiler_init();
  GFont font = fonts_get_system_font(args->font_key);

  int font_index;
  if (strcmp(args->font_key, "RESOURCE_ID_GOTHIC_18") == 0) {
    font_index = TestStringFont_Gothic18;
  } else if (strcmp(args->font_key, "RESOURCE_ID_GOTHIC_24_BOLD") == 0) {
    font_index = TestStringFont_Gothic24B;
  } else {
    font_index = TestStringFont_Other;
  }

  size_t length = s_perftest_text_strings[args->text_index].lengths[font_index];
  length = MIN(length, sizeof(s_text_test_str) - 1);
  strncpy(s_text_test_str, s_perftest_text_strings[args->text_index].string, length);
  s_text_test_str[length] = '\0';

  GRect bounds = GRect(0, 0, DISP_COLS, DISP_ROWS);
  bounds.origin.y -= args->y_offset;
  if (args->y_offset > 0) {
    bounds.size.h = DISP_ROWS + args->y_offset;
  }

  uint32_t avg = 0;

  for (int i = 0; i < PERFTEST_TEXT_ITERATIONS; i++) {
    // Sometimes this loop takes long enough that we end up watchdogging
    watchdog_feed();
    pbl_task_wdt_feed_all();

    GContext *ctx = prv_perftest_get_context();
    graphics_context_set_text_color(ctx, GColorBlack);

    profiler_start();
    graphics_draw_text(ctx, s_text_test_str, font, bounds, TEXT_OVERFLOW, TEXT_ALIGNMENT, NULL);
    profiler_stop();
    avg += profiler_get_total_duration(true);
  }

  avg /= PERFTEST_TEXT_ITERATIONS;
  uint32_t flash_us_avg = PROFILER_NODE_GET_TOTAL_US(text_render_flash) / PERFTEST_TEXT_ITERATIONS;
  pbl_shell_print(args->sh, "%s, %s, %s, %" PRIu32 ", %" PRIu32, args->font_key, args->type_str,
                  args->y_offset_str, avg, flash_us_avg);

  s_perftest_text_running = false;
}

static int prv_perftest_text(const struct pbl_shell *sh, const char *string_type,
                             const char *font_key, const char *y_offset) {
  PerftestTextArguments *args = &s_perftest_text_arguments;
  long offset;

  if (strcmp(string_type, "best") == 0) {
    args->text_index = TestString_Best;
  } else if (strcmp(string_type, "worst") == 0) {
    args->text_index = TestString_Worst;
  } else if (strcmp(string_type, "typical") == 0) {
    args->text_index = TestString_Typical;
  } else {
    pbl_shell_error(sh, "incorrect type argument, must be 'best', 'typical', or 'worst'");
    return -EINVAL;
  }

  if (pbl_shell_strtol(y_offset, &offset) != 0) {
    pbl_shell_error(sh, "invalid offset '%s'", y_offset);
    return -EINVAL;
  }

  args->sh = sh;
  args->font_key = font_key;
  args->y_offset = offset;
  args->type_str = string_type;
  args->y_offset_str = y_offset;

  s_perftest_text_running = true;
  launcher_task_add_callback(prv_perftest_text_main, NULL);
  while (s_perftest_text_running) {
    pbl_thread_yield();
    watchdog_feed();
    pbl_task_wdt_feed_all();
  }

  return 0;
}

static int prv_cmd_text(const struct pbl_shell *sh, size_t argc, char **argv) {
  return prv_perftest_text(sh, argv[1], argv[2], argv[3]);
}

static int prv_cmd_text_all(const struct pbl_shell *sh, size_t argc, char **argv) {
  static const char *const fonts[] = {
    "RESOURCE_ID_GOTHIC_28",      "RESOURCE_ID_GOTHIC_24",      "RESOURCE_ID_GOTHIC_18",
    "RESOURCE_ID_GOTHIC_28_BOLD", "RESOURCE_ID_GOTHIC_24_BOLD", "RESOURCE_ID_GOTHIC_18_BOLD",
  };
  static const char *const types[] = {"best", "worst", "typical"};
  static const char *const offsets[] = {"0", "2000"};

  pbl_shell_print(sh, "Font, Type, Offset, Total avg us, Flash avg us");
  for (size_t type = 0; type < ARRAY_LENGTH(types); type++) {
    for (size_t font = 0; font < ARRAY_LENGTH(fonts); font++) {
      for (size_t offset = 0; offset < ARRAY_LENGTH(offsets); offset++) {
        prv_perftest_text(sh, types[type], fonts[font], offsets[offset]);
      }
    }
  }

  return 0;
}

static const struct pbl_shell_cmd sub_perftest[] = {
  PBL_SHELL_CMD_ARG(line, NULL, "Time line drawing <aa|noaa> <width>", prv_cmd_line, 3, 0),
  PBL_SHELL_CMD_ARG(text, NULL, "Time text drawing <best|worst|typical> <font_key> <y_offset>",
                    prv_cmd_text, 4, 0),
  PBL_SHELL_CMD(line_all, NULL, "Time line drawing for all variants", prv_cmd_line_all),
  PBL_SHELL_CMD(text_all, NULL, "Time text drawing for all variants", prv_cmd_text_all),
  PBL_SHELL_SUBCMD_SET_END,
};

PBL_SHELL_CMD_REGISTER(perftest, sub_perftest, "Drawing performance tests", NULL);

#endif
