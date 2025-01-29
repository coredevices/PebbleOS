#include "utf8.h"

#include "system/passert.h"
#include "system/logging.h"

#include "util/iterator.h"
#include "util/math.h"
#include "util/size.h"
#include "util/string.h"

#include <inttypes.h>
#include <stdbool.h>

////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2008-2009 Bjoern Hoehrmann <bjoern@hoehrmann.de>
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
////////////////////////////////////////////////////////////////////////////////
static const unsigned int VALID_UTF8 = 0;

static const uint8_t utf8d[] = {
  // The first part of the table maps bytes to character classes that
  // to reduce the size of the transition table and create bitmasks.
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
  8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,  2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
  10,3,3,3,3,3,3,3,3,3,3,3,3,4,3,3, 11,6,6,6,5,8,8,8,8,8,8,8,8,8,8,8,
  
  // The second part is a transition table that maps a combination
  // of a state of the automaton and a character class to a state.
  0,12,24,36,60,96,84,12,12,12,48,72, 12,12,12,12,12,12,12,12,12,12,12,12,
  12, 0,12,12,12,12,12, 0,12, 0,12,12, 12,24,12,12,12,12,12,24,12,24,12,12,
  12,12,12,12,12,12,12,24,12,12,12,12, 12,24,12,12,12,12,12,12,12,24,12,12,
  12,12,12,12,12,12,12,36,12,36,12,12, 12,36,12,12,12,12,12,36,12,36,12,12,
  12,36,12,12,12,12,12,12,12,12,12,12, 
};

static uint32_t utf8_decode(uint8_t *state, uint32_t *codepoint, uint32_t byte) {
  uint32_t type = utf8d[byte];

  *codepoint = (*state != VALID_UTF8) ?
    (byte & 0x3fu) | (*codepoint << 6) :
    (0xff >> type) & (byte);

  *state = utf8d[256 + *state + type];
  return *state;
}

//! Print all code points in a c-string (debugging)
//! @param s A null-terminated c-string
void utf8_print_code_points(utf8_t *s) {
  uint32_t codepoint;
  uint8_t state = 0;

  for (; *s; ++s) {
    if (!utf8_decode(&state, &codepoint, *s)) {
      PBL_LOG(LOG_LEVEL_ALWAYS, "U+%04"PRIX32, codepoint);
    }
  }

  if (state != VALID_UTF8) {
    PBL_LOG(LOG_LEVEL_ALWAYS, "String is not well-formed");
  }
}




////////////////////////////////////////////////////////////
// Private API

//! Peek at the string and return the next codepoint
uint32_t utf8_peek_codepoint(utf8_t *stream, utf8_t **next_ptr) {
  uint32_t codepoint = 0;
  uint8_t state = 0;

  if (stream == NULL) {
    return 0;
  }

  for (; *stream; stream++) {
    if (utf8_decode(&state, &codepoint, *stream)) {
      // not done, loop again
      continue;
    }
    if (next_ptr) {
      *next_ptr = ++stream;
    }
    return codepoint;
  }

  if (next_ptr) {
    *next_ptr = NULL;
  }

  return 0;
}

utf8_t *utf8_get_next(utf8_t *stream) {
  uint32_t codepoint = 0;
  uint8_t state = 0;

  if (stream == NULL) {
    return stream;
  }

  for (; *stream; stream++) {
    if (!utf8_decode(&state, &codepoint, *stream)) {
      // Valid codepoint found; advance to start of next code point
      return ++stream;
    }
  }

  // No valid codepoint found
  return NULL;
}

// see http://stackoverflow.com/questions/22257486/iterate-backwards-through-a-utf8-multibyte-string
utf8_t *utf8_get_previous(utf8_t *start, utf8_t *stream) {
  do {
    if (stream <= start) {
      return NULL;
    }
    --stream;
  } while ((*stream & 0xc0) == 0x80);

  return stream;
}

////////////////////////////////////////////////////////////
// Public API

//! Return NULL if not successful in decoding text
utf8_t *utf8_get_end(const char *text) {
  if (text == NULL) {
    return (utf8_t *) text;
  }

  uint8_t *stream = (uint8_t *) text;
  uint32_t codepoint = 0;
  uint8_t state = 0;

  while (*stream) {
    utf8_decode(&state, &codepoint, *stream);
    stream++;
  }

  bool success = (state == VALID_UTF8);
  if (!success) {
    return NULL;
  }

  return (utf8_t *) stream;
}


bool utf8_is_valid_string(const char *char_stream) {
  return (utf8_get_end(char_stream) != NULL);
}

Utf8Bounds utf8_get_bounds(bool *const success, char const  *text) {
  Utf8Bounds bounds;
  bounds.start = (utf8_t *) text;
  bounds.end = bounds.start;

  utf8_t *end = utf8_get_end(text);

  if (NULL == end) {
    *success = false;
    return bounds;
  }

  bounds.end = end;
  *success = true;
  return bounds;
}

bool utf8_bounds_init(Utf8Bounds *bounds, const char *text) {
  bounds->start = (utf8_t *) text;
  bounds->end = bounds->start;

  utf8_t *end = utf8_get_end(text);

  if (end == NULL) {
    return false;
  }

  bounds->end = end;
  return true;
}

bool utf8_iter_next(IteratorState state) {
  Utf8IterState *utf8_iter_state = (Utf8IterState *) state;
  PBL_ASSERTN(utf8_iter_state);

  utf8_iter_state->codepoint = 0; // Invalidate the cached codepoint

  if (utf8_iter_state->current >= utf8_iter_state->bounds->end) {
    return false;
  }

  utf8_iter_state->current = utf8_iter_state->next;

  if (utf8_iter_state->current == NULL) {
    return false;
  }

  if (*utf8_iter_state->current == '\0') {
    return false;
  }

  utf8_iter_state->codepoint = utf8_peek_codepoint(utf8_iter_state->current, &utf8_iter_state->next);
  return true;
}

bool utf8_iter_prev(IteratorState state) {
  Utf8IterState *utf8_iter_state = (Utf8IterState *) state;
  PBL_ASSERTN(utf8_iter_state);

  utf8_iter_state->codepoint = 0;

  if (utf8_iter_state->current <= utf8_iter_state->bounds->start) {
    return false;
  }

  utf8_iter_state->current = utf8_get_previous(utf8_iter_state->bounds->start,
      utf8_iter_state->current);
  utf8_iter_state->codepoint = utf8_peek_codepoint(utf8_iter_state->current, &utf8_iter_state->next);
  return true;

}

void utf8_iter_init(Iterator *utf8_iter, Utf8IterState *utf8_iter_state, Utf8Bounds const *bounds, utf8_t *start) {
  PBL_ASSERTN(utf8_iter_state);
  PBL_ASSERTN(bounds);

  utf8_iter_state->bounds = bounds;
  PBL_ASSERTN(start >= bounds->start);
  PBL_ASSERTN(start <= bounds->end);
  utf8_iter_state->current = start;
  utf8_iter_state->codepoint = utf8_peek_codepoint(start, &utf8_iter_state->next);

  iter_init(utf8_iter, (IteratorCallback) utf8_iter_next, utf8_iter_prev, (IteratorState) utf8_iter_state);
}

size_t utf8_copy_character(utf8_t *dest, utf8_t *origin, size_t length) {
  utf8_t *next_char = utf8_get_next(origin);
  // If next_char is NULL, we were asked to copy the last character, so just take the end of the
  // string.
  if (next_char == NULL) {
    next_char = utf8_get_end((char *)origin);
    // If we can't get the end, bail out.
    if (next_char == NULL) {
      return 0;
    }
  }
  size_t len = next_char - origin;
  // Never copy a partial character; if it won't fit, do nothing.
  if (len > length) {
    return 0;
  }
  memcpy(dest, origin, len);
  return len;
}

size_t utf8_get_size_truncate(const char *text, size_t max_size) {
  PBL_ASSERTN(text);
  PBL_ASSERTN(max_size > 0);

  size_t len = strnlen(text, max_size);
  if (len == 0) {
    return len;
  }

  // get the start of the previous character if the string is too long
  if (max_size == len) {
    // src[len] is be valid because strnlen indicated that the source string is at least len
    // characters, therefore len can, at worst, only be the end of the string
    utf8_t *end = utf8_get_previous((utf8_t *)text, (utf8_t *)&text[len]);
    len = end - (utf8_t *)text;
  }

  return len;
}

size_t utf8_truncate_with_ellipsis(const char *in_string, char *out_buffer, size_t max_length) {
  const char ellipsis[] = UTF8_ELLIPSIS_STRING;
  const size_t ellipsis_length = ARRAY_LENGTH(ellipsis);
  if (max_length < ellipsis_length) {
    return 0;
  }
  const size_t in_length_bytes = strlen(in_string) + 1;
  const size_t clamped_in_length_bytes = MIN(in_length_bytes, max_length - (ellipsis_length - 1));
  if (in_length_bytes > max_length) {
    // finds where the ellipsis should start, by asking utf8_get_size_truncate
    const size_t ellipsis_start_offset = utf8_get_size_truncate(in_string, clamped_in_length_bytes);
    strncpy(out_buffer, in_string, ellipsis_start_offset);
    strncpy(&out_buffer[ellipsis_start_offset], ellipsis, ellipsis_length);
    return ellipsis_start_offset + ellipsis_length;
  } else {
    strncpy(out_buffer, in_string, in_length_bytes);
  }
  return in_length_bytes;
}

bool utf8_each_codepoint(const char *str, Utf8EachCodepoint callback, void *context) {
  Iterator utf8_iter;
  Utf8IterState utf8_iter_state;

  bool success = false;
  const Utf8Bounds utf8_bounds = utf8_get_bounds(&success, str);
  if (!success) {
    return false;
  }

  utf8_iter_init(&utf8_iter, &utf8_iter_state, &utf8_bounds, utf8_bounds.start);

  int i = 0;
  while (utf8_iter_state.codepoint &&
         callback(i++, utf8_iter_state.codepoint, context) &&
         iter_next(&utf8_iter)) {}
  return true;
}
