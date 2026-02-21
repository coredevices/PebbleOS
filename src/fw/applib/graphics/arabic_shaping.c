/* SPDX-FileCopyrightText: 2026 Ahmed Hussein */
/* SPDX-License-Identifier: Apache-2.0 */

#include "arabic_shaping.h"

#include <string.h>

// Maximum codepoints we can handle in a single shaping operation
// Keep small for stack safety on embedded systems (16 codepoints * 4 bytes = 64 bytes)
#define MAX_SHAPE_CODEPOINTS 16

// Connectivity flags for Arabic letters
#define JOIN_NONE       0x00  // Does not connect (space, punctuation)
#define JOIN_RIGHT_ONLY 0x01  // Connects only to the right (non-connecting letters)
#define JOIN_DUAL       0x03  // Connects both left and right (dual-joining letters)

// Shaping table entry - compact representation of Arabic letter forms
// Each entry maps a basic Arabic letter to its presentation forms
typedef struct __attribute__((__packed__)) {
  uint16_t base;      // Basic Arabic codepoint (e.g., 0x0628 for Ba)
  uint16_t isolated;  // Presentation form isolated
  uint8_t  offsets;   // Packed offsets: final(4) | initial(2) | medial(2) from isolated
  uint8_t  flags;     // Connectivity flags
} ArabicShapingEntry;

// Macro to pack offsets: final offset (0-15), initial offset (0-3), medial offset (0-3)
// Format: ffff_ii_mm where f=final, i=initial, m=medial
#define PACK_OFFSETS(final, initial, medial) \
  (((final) << 4) | (((initial) & 0x3) << 2) | ((medial) & 0x3))

// Standard dual-joining letter offsets pattern (most common)
// isolated+1=final, isolated+2=initial, isolated+3=medial
#define DUAL_JOIN_OFFSETS PACK_OFFSETS(1, 2, 3)

// Right-joining letter offsets pattern (non-connecting letters)
// isolated+1=final, no initial or medial forms
#define RIGHT_JOIN_OFFSETS PACK_OFFSETS(1, 0, 0)

// Shaping lookup table for Arabic letters
// Sorted by base codepoint for potential binary search optimization
// Total size: 36 entries * 6 bytes = 216 bytes ROM
static const ArabicShapingEntry s_shaping_table[] = {
  // Hamza (standalone, no connection)
  { 0x0621, 0xFE80, PACK_OFFSETS(0, 0, 0), JOIN_NONE },

  // Alef with Madda Above
  { 0x0622, 0xFE81, RIGHT_JOIN_OFFSETS, JOIN_RIGHT_ONLY },

  // Alef with Hamza Above
  { 0x0623, 0xFE83, RIGHT_JOIN_OFFSETS, JOIN_RIGHT_ONLY },

  // Waw with Hamza Above
  { 0x0624, 0xFE85, RIGHT_JOIN_OFFSETS, JOIN_RIGHT_ONLY },

  // Alef with Hamza Below
  { 0x0625, 0xFE87, RIGHT_JOIN_OFFSETS, JOIN_RIGHT_ONLY },

  // Yeh with Hamza Above
  { 0x0626, 0xFE89, DUAL_JOIN_OFFSETS, JOIN_DUAL },

  // Alef (non-connecting)
  { 0x0627, 0xFE8D, RIGHT_JOIN_OFFSETS, JOIN_RIGHT_ONLY },

  // Beh
  { 0x0628, 0xFE8F, DUAL_JOIN_OFFSETS, JOIN_DUAL },

  // Teh Marbuta
  { 0x0629, 0xFE93, RIGHT_JOIN_OFFSETS, JOIN_RIGHT_ONLY },

  // Teh
  { 0x062A, 0xFE95, DUAL_JOIN_OFFSETS, JOIN_DUAL },

  // Theh
  { 0x062B, 0xFE99, DUAL_JOIN_OFFSETS, JOIN_DUAL },

  // Jeem
  { 0x062C, 0xFE9D, DUAL_JOIN_OFFSETS, JOIN_DUAL },

  // Hah
  { 0x062D, 0xFEA1, DUAL_JOIN_OFFSETS, JOIN_DUAL },

  // Khah
  { 0x062E, 0xFEA5, DUAL_JOIN_OFFSETS, JOIN_DUAL },

  // Dal (non-connecting)
  { 0x062F, 0xFEA9, RIGHT_JOIN_OFFSETS, JOIN_RIGHT_ONLY },

  // Thal (non-connecting)
  { 0x0630, 0xFEAB, RIGHT_JOIN_OFFSETS, JOIN_RIGHT_ONLY },

  // Reh (non-connecting)
  { 0x0631, 0xFEAD, RIGHT_JOIN_OFFSETS, JOIN_RIGHT_ONLY },

  // Zain (non-connecting)
  { 0x0632, 0xFEAF, RIGHT_JOIN_OFFSETS, JOIN_RIGHT_ONLY },

  // Seen
  { 0x0633, 0xFEB1, DUAL_JOIN_OFFSETS, JOIN_DUAL },

  // Sheen
  { 0x0634, 0xFEB5, DUAL_JOIN_OFFSETS, JOIN_DUAL },

  // Sad
  { 0x0635, 0xFEB9, DUAL_JOIN_OFFSETS, JOIN_DUAL },

  // Dad
  { 0x0636, 0xFEBD, DUAL_JOIN_OFFSETS, JOIN_DUAL },

  // Tah
  { 0x0637, 0xFEC1, DUAL_JOIN_OFFSETS, JOIN_DUAL },

  // Zah
  { 0x0638, 0xFEC5, DUAL_JOIN_OFFSETS, JOIN_DUAL },

  // Ain
  { 0x0639, 0xFEC9, DUAL_JOIN_OFFSETS, JOIN_DUAL },

  // Ghain
  { 0x063A, 0xFECD, DUAL_JOIN_OFFSETS, JOIN_DUAL },

  // Tatweel (kashida - connector)
  { 0x0640, 0x0640, PACK_OFFSETS(0, 0, 0), JOIN_DUAL },

  // Feh
  { 0x0641, 0xFED1, DUAL_JOIN_OFFSETS, JOIN_DUAL },

  // Qaf
  { 0x0642, 0xFED5, DUAL_JOIN_OFFSETS, JOIN_DUAL },

  // Kaf
  { 0x0643, 0xFED9, DUAL_JOIN_OFFSETS, JOIN_DUAL },

  // Lam
  { 0x0644, 0xFEDD, DUAL_JOIN_OFFSETS, JOIN_DUAL },

  // Meem
  { 0x0645, 0xFEE1, DUAL_JOIN_OFFSETS, JOIN_DUAL },

  // Noon
  { 0x0646, 0xFEE5, DUAL_JOIN_OFFSETS, JOIN_DUAL },

  // Heh
  { 0x0647, 0xFEE9, DUAL_JOIN_OFFSETS, JOIN_DUAL },

  // Waw (non-connecting)
  { 0x0648, 0xFEED, RIGHT_JOIN_OFFSETS, JOIN_RIGHT_ONLY },

  // Alef Maksura
  { 0x0649, 0xFEEF, RIGHT_JOIN_OFFSETS, JOIN_RIGHT_ONLY },

  // Yeh
  { 0x064A, 0xFEF1, DUAL_JOIN_OFFSETS, JOIN_DUAL },
};

#define SHAPING_TABLE_SIZE (sizeof(s_shaping_table) / sizeof(s_shaping_table[0]))

// Find shaping entry for a base codepoint using linear search
// Returns NULL if not found
static const ArabicShapingEntry *prv_find_shaping_entry(Codepoint cp) {
  // Quick bounds check
  if (cp < 0x0621 || cp > 0x064A) {
    return NULL;
  }

  // Linear search (table is small, ~37 entries)
  for (size_t i = 0; i < SHAPING_TABLE_SIZE; i++) {
    if (s_shaping_table[i].base == cp) {
      return &s_shaping_table[i];
    }
  }

  return NULL;
}

bool arabic_is_shapeable(Codepoint cp) {
  return prv_find_shaping_entry(cp) != NULL;
}

// Check if an Arabic letter connects to the left (has initial/medial forms)
static bool prv_connects_left(const ArabicShapingEntry *entry) {
  if (entry == NULL) {
    return false;
  }
  return (entry->flags & JOIN_DUAL) == JOIN_DUAL;
}

// Check if an Arabic letter connects to the right (has final/medial forms)
static bool prv_connects_right(const ArabicShapingEntry *entry) {
  if (entry == NULL) {
    return false;
  }
  return (entry->flags & JOIN_RIGHT_ONLY) != 0;
}

// Get the presentation form codepoint for a letter based on its context
static Codepoint prv_get_shaped_codepoint(const ArabicShapingEntry *entry, ArabicForm form) {
  if (entry == NULL) {
    return 0;
  }

  // Tatweel (kashida) doesn't have presentation forms
  if (entry->base == 0x0640) {
    return entry->base;
  }

  uint16_t isolated = entry->isolated;
  uint8_t offsets = entry->offsets;

  switch (form) {
    case ARABIC_FORM_ISOLATED:
      return isolated;
    case ARABIC_FORM_FINAL:
      return isolated + ((offsets >> 4) & 0x0F);
    case ARABIC_FORM_INITIAL:
      return isolated + ((offsets >> 2) & 0x03);
    case ARABIC_FORM_MEDIAL:
      return isolated + (offsets & 0x03);
    default:
      return isolated;
  }
}

size_t arabic_shape_text(const utf8_t *src, size_t src_len,
                         utf8_t *dest, size_t dest_size) {
  if (src == NULL || dest == NULL || src_len == 0 || dest_size == 0) {
    return 0;
  }

  // First pass: collect all codepoints into an array
  Codepoint codepoints[MAX_SHAPE_CODEPOINTS];
  size_t num_codepoints = 0;

  utf8_t *ptr = (utf8_t *)src;
  const utf8_t *end = src + src_len;

  while (ptr < end && *ptr != '\0' && num_codepoints < MAX_SHAPE_CODEPOINTS) {
    utf8_t *next = NULL;
    Codepoint cp = utf8_peek_codepoint(ptr, &next);
    if (cp == 0 || next == NULL) {
      break;
    }
    codepoints[num_codepoints++] = cp;
    ptr = next;
  }

  if (num_codepoints == 0) {
    return 0;
  }

  // Second pass: shape Arabic letters and write to destination
  size_t dest_offset = 0;

  for (size_t i = 0; i < num_codepoints; i++) {
    Codepoint cp = codepoints[i];
    Codepoint shaped_cp = cp;

    // Check if this is a shapeable Arabic letter
    const ArabicShapingEntry *entry = prv_find_shaping_entry(cp);
    if (entry != NULL) {
      // Determine context: check previous and next letters
      const ArabicShapingEntry *prev_entry = NULL;
      const ArabicShapingEntry *next_entry = NULL;

      if (i > 0) {
        prev_entry = prv_find_shaping_entry(codepoints[i - 1]);
      }
      if (i + 1 < num_codepoints) {
        next_entry = prv_find_shaping_entry(codepoints[i + 1]);
      }

      // Check connectivity
      bool prev_connects = (prev_entry != NULL) && prv_connects_left(prev_entry);
      bool next_connects = (next_entry != NULL) && prv_connects_right(next_entry);

      // Also check if current letter can connect in that direction
      bool can_connect_right = prv_connects_right(entry);
      bool can_connect_left = prv_connects_left(entry);

      // Determine the form based on connections
      ArabicForm form;
      if (prev_connects && can_connect_right && next_connects && can_connect_left) {
        form = ARABIC_FORM_MEDIAL;
      } else if (prev_connects && can_connect_right) {
        form = ARABIC_FORM_FINAL;
      } else if (next_connects && can_connect_left) {
        form = ARABIC_FORM_INITIAL;
      } else {
        form = ARABIC_FORM_ISOLATED;
      }

      shaped_cp = prv_get_shaped_codepoint(entry, form);
    }

    // Encode the shaped codepoint to UTF-8
    // Ensure we have room for at least 4 bytes (max UTF-8 length)
    if (dest_offset + 4 >= dest_size) {
      break;
    }

    size_t bytes_written = utf8_encode_codepoint(shaped_cp, dest + dest_offset);
    if (bytes_written == 0) {
      continue;
    }
    dest_offset += bytes_written;
  }

  // Null-terminate if we have space
  if (dest_offset < dest_size) {
    dest[dest_offset] = '\0';
  }

  return dest_offset;
}
