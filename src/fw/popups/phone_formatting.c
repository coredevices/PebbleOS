/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "phone_formatting.h"

#include "applib/graphics/utf8.h"
#include "util/math.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

// Turn every word after the first one into an initial.
// e.g. Katharine Claire Berry -> Katharine C. B.
void phone_format_caller_name(const char *full_name, char *destination, size_t length) {
  char *space = strchr(full_name, ' ');
  // If there are no spaces, just use the whole thing and bail.
  if (!space) {
    strncpy(destination, full_name, length);
    destination[length - 1] = '\0';
    return;
  }
  // Copy the first name to the destination, as long as it fits.
  size_t pos = MIN((size_t)(space - full_name), length - 1);
  strncpy(destination, full_name, pos);
  // Then keep adding the character after the space until we run out of spaces.
  do {
    space++;
    // Abort if it is impossible for us to fit a space, one-byte initial, period and null in the
    // buffer (= four bytes)
    // Also bail out if this space terminates the string; otherwise we append an unnecessary
    // space to the destination.
    if ((pos + 4 > length - 1) || (*space == '\0')) {
      break;
    }
    // Skip ahead if this is a space. This avoids stray dots on double spaces.
    if (*space == ' ') {
      continue;
    }

    destination[pos++] = ' ';
    size_t initial_size = utf8_copy_character((utf8_t *)&destination[pos], (utf8_t *)space,
                                              length - pos - 2); // 2 = ".\0"
    // If we couldn't fit anything, stop here.
    if (initial_size == 0) {
      pos--; // the space we previously added should be omitted from our string.
      break;
    }
    pos += initial_size;
    destination[pos++] = '.';
  } while ((space = strchr(space, ' ')));
  destination[pos] = '\0';
}

// based on https://en.wikipedia.org/wiki/National_conventions_for_writing_telephone_numbers
void phone_format_phone_number(const char *phone_number, char *formatted_phone_number,
                               size_t length) {
  const int phone_number_length = strlen(phone_number);

  // Only modify if phone number includes area code and correctly formatted
  const int long_distance_min_len = 12;  // 650-777-1234, +49 30 90260
  if (phone_number_length >= long_distance_min_len) {
    int local_number_length = 0;
    // Parse from the end of the string to identify the local portion of the phone number
    // After local_number_min_len, a separator delimits the regional or international portion
    const int local_number_min_len = 6;
    for (local_number_length = local_number_min_len; local_number_length < phone_number_length;
         local_number_length++) {
      const char key = phone_number[phone_number_length - local_number_length - 1];
      if (!isdigit(key)) {
        break;
      }
    }

    // Force the local part of the phone number to the second line using newline
    const int region_min_len = 3;
    if (local_number_length <= (phone_number_length - region_min_len)) {
      int region_length = phone_number_length - local_number_length;
      const char *local_number = &phone_number[region_length];
      // Remove dash, dot or space from region line
      if ((phone_number[region_length - 1] == '-') || (phone_number[region_length - 1] == '.') ||
          (phone_number[region_length - 1] == ' ')) {
        region_length--;
      }
      snprintf(formatted_phone_number, length, "%.*s\n%.*s",
               region_length, phone_number, local_number_length, local_number);
      return;
    }
  }

  // copy original to the output buffer for non-covered cases
  strncpy(formatted_phone_number, phone_number, length);
}
