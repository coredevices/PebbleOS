/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "util/string.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

const char *string_strip_leading_whitespace(const char *string) {
  const char *result_string = string;
  while (*result_string != '\0') {
    if (*result_string != ' ' &&
        *result_string != '\n') {
      break;
    }
    result_string++;
  }

  return result_string;
}

void string_strip_trailing_whitespace(const char *string, char *string_out) {
  int string_len = strlen(string);
  bool trim = true;
  for (int i = string_len; i >= 0; i--) {
    if (trim &&
        (string[i] == ' ' || string[i] == '\n' || string[i] == '\0')) {
      string_out[i] = '\0';
    } else {
      trim = false;
      string_out[i] = string[i];
    }
  }
}

const char *bool_to_str(bool b) {
  if (b) {
    return "yes";
  } else {
    return "no";
  }
}

void itoa(uint32_t num, char *buffer, int buffer_length) {
  if (buffer_length < 11) {
    return;
  }
  *buffer++ = '0';
  *buffer++ = 'x';

  for (int i = 7; i >= 0; --i) {
    uint32_t digit = (num & (0xf << (i * 4))) >> (i * 4);

    char c;
    if (digit < 0xa) {
      c = '0' + digit;
    } else if (digit < 0x10) {
      c = 'a' + (digit - 0xa);
    } else {
      c = ' ';
    }

    *buffer++ = c;
  }
  *buffer = '\0';
}

void string_reverse(char *str) {
  uint8_t i = 0;
  int8_t j = strlen(str) - 1;
  for (i = 0; i < j; i++, j--) {
    char c = str[i];
    str[i] = str[j];
    str[j] = c;
  }
}

/* itoa:  convert n to characters in s */
void itoa_int(int n, char *str, int base) {
  bool neg;
  if ((neg = (n < 0))) {          /* record sign */
    n = -n;                       /* make n positive */
  }

  int i = 0;
  do {                            /* generate digits in reverse order */
    str[i++] = (n % base) + '0';    /* get next digit */
  } while ((n /= base) > 0);        /* delete it */

  if (neg) {
    str[i++] = '-';               /* append sign */
  }

  str[i] = '\0';
  string_reverse(str);
}

static int8_t ascii_hex_to_int(const uint8_t c) {
  if (isdigit(c)) return c - '0';
  if (isupper(c)) return (c - 'A') + 10;
  if (islower(c)) return (c - 'a') + 10;

  return -1;
}

static uint8_t ascii_hex_to_uint(const uint8_t msb, const uint8_t lsb) {
  return 16 * ascii_hex_to_int(msb) + ascii_hex_to_int(lsb);
}

uintptr_t str_to_address(const char *address_str) {
  char *endptr;
  uintptr_t address = strtoul(address_str, &endptr, 0);
  if (*endptr != '\0') { // A non-address character encountered
    return -1;
  }
  return address;
}

bool convert_bt_addr_hex_str_to_bd_addr(const char *hex_str, uint8_t *bd_addr, const unsigned int bd_addr_size) {
  const int len = strlen(hex_str);
  if (len != 12) {
    return false;
  }

  uint8_t* src = (uint8_t*) hex_str;
  uint8_t* dest = bd_addr + bd_addr_size - 1;

  for (unsigned int i = 0; i < bd_addr_size; ++i, src += 2, --dest) {
    *dest = ascii_hex_to_uint(src[0], src[1]);
  }

  return true;
}

void concat_str_int(const char *str, uint32_t num, char *buf, uint8_t buf_len) {
  uint8_t str_len = strlen(str);
  strncpy(buf, str, str_len);
  itoa_int(num, buf + str_len, 10);
}

void toupper_str(char *str) {
  int len = strlen(str);
  for (int i = 0; i < len; i++) {
    str[i] = toupper((unsigned char)str[i]);
  }
}

void byte_stream_to_hex_string(char *out_buf, size_t out_buf_len,
    const uint8_t *byte_stream, size_t byte_stream_len, bool print_backward) {
  size_t bytes_left = byte_stream_len;
  if (print_backward) {
    byte_stream += (byte_stream_len - 1); // addr of the last element
  }

  while (out_buf_len >= 3 /* 2 hex digits, plus '\0' */
         && bytes_left > 0) {
    snprintf(out_buf, out_buf_len, "%02x", *byte_stream);

    out_buf += 2;
    out_buf_len -= 2;

    byte_stream += (print_backward) ? -1 : 1;
    bytes_left -= 1;
  }
}

// -------------------------------------------------------------------------------
void safe_strcat(char* dst, const char* src, int dst_space) {
  int remaining = dst_space - strlen(dst);
  if (dst_space > 0) {
    strncat(dst, src, remaining);
  }
  dst[dst_space-1] = 0;
}
