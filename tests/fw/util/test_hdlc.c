/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "util/hdlc.h"

#include <string.h>

#include "stubs_passert.h"

// Setup

void test_hdlc__initialize(void) {
}

void test_hdlc__cleanup(void) {
}

// Tests

void test_hdlc__decode_no_special(void) {
  // without any special characters
  const char *str = "\x7eThis is a long string without any special characters to be escaped.\x7e";
  int len = strlen(str);
  HdlcStreamingContext ctx;
  hdlc_streaming_decode_reset(&ctx);
  for (int i = 0; i < len; i++) {
    char c = str[i];
    bool should_store, is_invalid;
    bool is_complete = hdlc_streaming_decode(&ctx, (uint8_t *)&c, &should_store, &is_invalid);
    cl_assert(is_invalid == false);
    if (i == 0 || i == len - 1) {
      cl_assert(is_complete == true);
      cl_assert(should_store == false);
    } else {
      cl_assert(is_complete == false);
      cl_assert(should_store == true);
      cl_assert(c == str[i]);
    }
  }
}

void test_hdlc__special_characters(void) {
  // make sure the escape characters haven't changed
  cl_assert(HDLC_FLAG == 0x7e);
  cl_assert(HDLC_ESCAPE == 0x7d);
  cl_assert(HDLC_ESCAPE_MASK == 0x20);
}

void test_hdlc__decode_empty(void) {
  // consecutive empty frames
  const uint8_t str[4] = {HDLC_FLAG, HDLC_FLAG, HDLC_FLAG, HDLC_FLAG};
  HdlcStreamingContext ctx;
  hdlc_streaming_decode_reset(&ctx);
  for (int i = 0; i < 4; i++) {
    char c = str[i];
    bool should_store, is_invalid;
    bool is_complete = hdlc_streaming_decode(&ctx, (uint8_t *)&c, &should_store, &is_invalid);
    cl_assert(is_complete == true);
    cl_assert(should_store == false);
    cl_assert(is_invalid == false);
  }
}

void test_hdlc__decode_invalid(void) {
  // invalid sequences
  uint8_t data;
  bool should_store, is_invalid, is_complete;
  HdlcStreamingContext ctx;

  // two consecutive escape characters
  hdlc_streaming_decode_reset(&ctx);
  data = HDLC_ESCAPE;
  is_complete = hdlc_streaming_decode(&ctx, &data, &should_store, &is_invalid);
  cl_assert(is_complete == false);
  cl_assert(should_store == false);
  cl_assert(is_invalid == false);
  data = HDLC_ESCAPE;
  is_complete = hdlc_streaming_decode(&ctx, &data, &should_store, &is_invalid);
  cl_assert(is_complete == false);
  cl_assert(should_store == false);
  cl_assert(is_invalid == true);

  // an escape character followed by a flag
  hdlc_streaming_decode_reset(&ctx);
  data = HDLC_ESCAPE;
  is_complete = hdlc_streaming_decode(&ctx, &data, &should_store, &is_invalid);
  cl_assert(is_complete == false);
  cl_assert(should_store == false);
  cl_assert(is_invalid == false);
  data = HDLC_FLAG;
  is_complete = hdlc_streaming_decode(&ctx, &data, &should_store, &is_invalid);
  cl_assert(is_complete == true);
  cl_assert(should_store == false);
  cl_assert(is_invalid == true);
}

void test_hdlc__decode_escaped_special(void) {
  // 2 escaped special characters
  uint8_t data;
  bool should_store, is_invalid, is_complete;
  HdlcStreamingContext ctx;
  hdlc_streaming_decode_reset(&ctx);

  // escaped escape character
  data = HDLC_ESCAPE;
  is_complete = hdlc_streaming_decode(&ctx, &data, &should_store, &is_invalid);
  cl_assert(is_complete == false);
  cl_assert(should_store == false);
  cl_assert(is_invalid == false);
  data = HDLC_ESCAPE ^ HDLC_ESCAPE_MASK;
  is_complete = hdlc_streaming_decode(&ctx, &data, &should_store, &is_invalid);
  cl_assert(is_complete == false);
  cl_assert(should_store == true);
  cl_assert(is_invalid == false);
  cl_assert(data == HDLC_ESCAPE);

  // escaped flag
  data = HDLC_ESCAPE;
  is_complete = hdlc_streaming_decode(&ctx, &data, &should_store, &is_invalid);
  cl_assert(is_complete == false);
  cl_assert(should_store == false);
  cl_assert(is_invalid == false);
  data = HDLC_FLAG ^ HDLC_ESCAPE_MASK;
  is_complete = hdlc_streaming_decode(&ctx, &data, &should_store, &is_invalid);
  cl_assert(is_complete == false);
  cl_assert(should_store == true);
  cl_assert(is_invalid == false);
  cl_assert(data == HDLC_FLAG);
}

void test_hdlc__encode_decode(void) {
  const char *str = "this is a string with the special \x7e \x7d \x7e\x7d \x7d\x7e characters";
  char buffer[100];
  int write_idx = 0;
  for (int i = 0; i < strlen(str); i++) {
    char c = str[i];
    if (hdlc_encode((uint8_t *)&c)) {
      buffer[write_idx++] = HDLC_ESCAPE;
    }
    buffer[write_idx++] = c;
  }
  buffer[write_idx++] = HDLC_FLAG;
  cl_assert(write_idx == strlen(str) + 7 /* 6 special characters to escape + 1 flag at end */);

  HdlcStreamingContext ctx;
  hdlc_streaming_decode_reset(&ctx);
  int read_idx = 0;
  int i = 0;
  while (true) {
    char c = buffer[i++];
    bool should_store, is_invalid;
    bool is_complete = hdlc_streaming_decode(&ctx, (uint8_t *)&c, &should_store, &is_invalid);
    cl_assert(is_invalid == false);
    if (should_store) {
      cl_assert(is_complete == false);
      cl_assert(c == str[read_idx++]);
    }
    if (is_complete) {
      cl_assert(should_store == false);
      cl_assert(i == write_idx);
      break;
    }
  }
  cl_assert(read_idx == strlen(str));
}
