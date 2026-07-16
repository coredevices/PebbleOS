/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

static void reverse(char *str, int length) {
  int start = 0;
  int end = length - 1;
  while (start < end) {
    char temp = str[start];
    str[start] = str[end];
    str[end] = temp;
    start++;
    end--;
  }
}

char *itoa(int value, char *str, int base) {
  int i = 0;
  int is_negative = 0;

  // Handle base validation
  if (base < 2 || base > 36) {
    str[0] = '\0';
    return str;
  }

  // Handle 0 explicitly
  if (value == 0) {
    str[i++] = '0';
    str[i] = '\0';
    return str;
  }

  // Handle negative numbers for base 10
  if (value < 0 && base == 10) {
    is_negative = 1;
    value = -value;
  }

  // Process individual digits
  while (value != 0) {
    int rem = value % base;
    str[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
    value = value / base;
  }

  // Append negative sign for negative numbers
  if (is_negative) {
    str[i++] = '-';
  }

  str[i] = '\0';

  // Reverse the string
  reverse(str, i);

  return str;
}
