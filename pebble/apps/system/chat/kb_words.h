// On-watch English word engine for the touch keyboard: completions, autocorrect, swipe typing.
// Emery and gabbro only; the dictionary is the raw resource KB_WORDS (see tools/build-kb-words.py).
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define KB_WORD_MAX 24

#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)

void kb_words_init(void);    // cheap; the dictionary index is read lazily on first use
void kb_words_deinit(void);  // frees everything

// Completions for the word being typed (lowercase ASCII prefix, may be ""), best first.
// Words equal to the prefix are not returned. Returns count (<= max, at most 12).
int kb_words_complete(const char *prefix, char out[][KB_WORD_MAX], int max);

// For a word the user just finished typing: true and writes a correction if it is
// almost certainly a typo of a common word (input/output lowercase).
bool kb_words_correct(const char *word, char *out, size_t out_size);

// Swipe typing: `keys` is the lowercase sequence of letter keys the finger passed over in order
// (consecutive duplicates removed; first and last are where the finger went down/up).
// xs/ys (may be NULL) are npts points of the actual finger path in keyboard units: x = 8 per
// half key across the 20-unit row (0..160), y = 16 per letter row from the top row (0..47).
// Best candidates first. Returns count (<= max, at most 12).
int kb_words_swipe(const char *keys, const uint8_t *xs, const uint8_t *ys, int npts,
                   char out[][KB_WORD_MAX], int max);

#else

// Other platforms: no word engine (and no code; these are never emitted unless called).
static inline void kb_words_init(void) {}
static inline void kb_words_deinit(void) {}
static inline int kb_words_complete(const char *p, char out[][KB_WORD_MAX], int m) { (void)p; (void)out; (void)m; return 0; }
static inline bool kb_words_correct(const char *w, char *o, size_t s) { (void)w; (void)o; (void)s; return false; }
static inline int kb_words_swipe(const char *k, const uint8_t *x, const uint8_t *y, int n,
                                 char out[][KB_WORD_MAX], int m) {
  (void)k; (void)x; (void)y; (void)n; (void)out; (void)m; return 0;
}

#endif
