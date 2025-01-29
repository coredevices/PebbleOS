#pragma once

#include "applib/fonts/codepoint.h"
#include "util/iterator.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>

typedef uint8_t utf8_t;

#define UTF8_ELLIPSIS_STRING ("\xe2\x80\xa6")

////////////////////////////////////////////////////////////
// UTF-8 Internal API

//! Validate a UTF-8 encoded c-string.
//! @param string A null-terminated UTF-8 c-string.
//! @return True if the string is valid UTF-8, false otherwise
bool utf8_is_valid_string(const char *string);

//! Move past the current codepoint to the start of the next codepoint.
//! @param start A null-terminated UTF-8 c-string.
//! @return pointer to the next codepoint if one can be found, NULL otherwise
utf8_t *utf8_get_next(utf8_t *start);

//! Move before the current codepoint to the start of the previous codepoint.
//! @param start The start of the utf-8 string.
//! @param current The current utf-8 codepoint in the string
//! @note: we assume utf8_get_next was used previously and thus the utf8 is well formed
utf8_t *utf8_get_previous(utf8_t *start, utf8_t *current);

//! Peek at the string and return the next codepoint
//! @return next codepoint if one can be found, GRAPHICS_INVALID_STREAM otherwise
uint32_t utf8_peek_codepoint(utf8_t *string, utf8_t **next_ptr);

//! Copies the UTF-8 character at origin to dest, given there is a valid character and it fits.
//! Does nothing and returns zero if not.
//! @param dest Pointer to the buffer to copy a character into.
//! @param origin Pointer to a utf-8 character to copy.
//! @param length Maximum number of bytes to copy.
//! @return The number of bytes copied.
size_t utf8_copy_character(utf8_t *dest, utf8_t *origin, size_t length);

//! Returns the length of the string if this length is less than \ref max_size bytes. Otherwise, it
//! returns the length of the string up until the end of the last valid codepoint that fits into
//! \ref max_size bytes and \ref truncated is set to true (it is set to false if the string is not
//! truncated)
//! @param text A null-terminated UTF-8 c-string.
//! @param max_size maximum allowable size, in bytes, of the string (including null terminator)
//! @return length of string in bytes (will always be less than \ref max_size)
size_t utf8_get_size_truncate(const char *text, size_t max_size);

//! Truncates \ref in_string to at most \ref max_length bytes (including the null
//! terminator) with ellipsis.
//! @param in_string A null-terminated UTF-8 c-string.
//! @param[out] out_buffer A buffer where the truncated string will be output,
//!                        must have length at least max_length.
//! @param max_length Max allowable size bytes of the output string (including null terminator).
//! @return Length of output string in bytes (always less than or equal to max_length).
size_t utf8_truncate_with_ellipsis(const char *in_string, char *out_buffer, size_t max_length);

////////////////////////////////////////////////////////////
// UTF-8 Iterator API

typedef struct {
  utf8_t *start;
  utf8_t *end; //<! Points to first un-decodable codepoint
} Utf8Bounds;

typedef struct {
  Utf8Bounds const  *bounds;
  utf8_t *current; //<! Must be within bounds, inclusive; advancing past trips assert
  utf8_t *next;
  uint32_t codepoint; //! Cached current codepoint
} Utf8IterState;

Utf8Bounds utf8_get_bounds(bool *const success, char const *text);

void utf8_iter_init(Iterator *utf8_iter, Utf8IterState *utf8_iter_state, Utf8Bounds const  *bounds, utf8_t *start);

bool utf8_iter_next(IteratorState state);

bool utf8_iter_prev(IteratorState state);

//! A Codepoint callback will be called for each codepoint
//! @param index int of the current codepoint index
//! @param codepoint the current Codepoint of the iteration
//! @param context user context that is passed for each iteration
//! @return true to continue the iterator, otherwise false to break the iteration
typedef bool (*Utf8EachCodepoint)(int index, Codepoint codepoint, void *context);

//! Calls a user given Utf8EachCodepoint callback for each codepoint given a valid UTF-8 c-string
//! @param str a null-terminated UTF-8 c-string
//! @param callback Utf8EachCodepoint callback
//! @param context user context to be passed to the callback
//! @return true if the string was a valid UTF-8 c-string, false otherwise
bool utf8_each_codepoint(const char *str, Utf8EachCodepoint callback, void *context);
