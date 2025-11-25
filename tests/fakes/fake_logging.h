/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <clar.h>

#include <regex.h>

#include <stdio.h>
#include <stdarg.h>
#include <util/string.h>

// make sure we can unit-test log output
#define CUSTOM_LOG_INTERNAL

const char **s_log_internal__expected;
const char **s_log_internal__expected_regex;

static void log_internal(uint8_t log_level, const char* src_filename, int src_line_number,
                         const char* fmt, va_list args) {
  // this implementation for log_internal constructs the logged string inside of a static buffer
  // so we can compare it against some test expectation

  printf("%s:%d> ", GET_FILE_NAME(src_filename), src_line_number);

  static char buffer[256];
  const int written = vsprintf(buffer, fmt, args);
  cl_assert(written < sizeof(buffer)); // there's no vsnprintf()
  buffer[written] = '\0';

  // compare log message against array of expectations
  if (s_log_internal__expected) {
    if (*s_log_internal__expected == NULL) {
      cl_assert_equal_s("Did not expect another logged string, but got", buffer);
      cl_fail("Should only happen if the log statement exactly matches the message above.");
    }
    cl_assert_equal_s(*s_log_internal__expected, buffer);
    s_log_internal__expected++;
  } else if (s_log_internal__expected_regex) {
    if (*s_log_internal__expected_regex == NULL) {
      cl_assert_equal_s("Did not expect another logged string, but got", buffer);
      cl_fail("Should only happen if the log statement exactly matches the message above.");
    }
    regex_t regex = {};

    // Compile regex:
    cl_assert_equal_i(0, regcomp(&regex, *s_log_internal__expected_regex, REG_EXTENDED));

    // Match regex:
    const int rv = regexec(&regex, buffer, 0, NULL, 0);
    if (rv) {
      // Check REG_... #defines in regex.h for what these values mean.
      char msgbuf[256];
      char regexerr[128];
      regerror(rv, &regex, regexerr, sizeof(regexerr));
      sprintf(msgbuf, "Regex match failed (rv=%i): %s\n \"%s\" didn't match pattern \"%s\"",
              rv, regexerr, buffer, *s_log_internal__expected_regex);
      cl_fail(msgbuf);
    }
    regfree(&regex);

    s_log_internal__expected_regex++;
  }

  printf("%s", buffer);
  printf("\n");
}
