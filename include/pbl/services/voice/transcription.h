/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <pbl/kernel/compiler.h>

#include <applib/graphics/utf8.h>

/**
 * @defgroup services_voice_transcription Transcription
 * @ingroup services_voice
 * @brief Validation and traversal of serialized transcriptions received from the phone.
 *
 * A transcription is kept in memory exactly as received over the voice endpoint: packed,
 * variable-length sentences, each a list of variable-length words.
 *
 * @code{.c}
 * static bool count_word(const TranscriptionWord *word, void *data) {
 *   (*(size_t *)data)++;
 *   return true;
 * }
 *
 * if (transcription_validate(t, size)) {
 *   size_t words = 0;
 *   const TranscriptionSentence *s = &t->sentences[0];
 *   transcription_iterate_words(s->words, s->word_count, count_word, &words);
 * }
 * @endcode
 * @{
 */

/** @brief Transcription format. */
typedef enum {
  /** List of sentences, the only supported format. */
  TranscriptionTypeSentenceList = 0x01
} TranscriptionType;

/** @brief Word with its confidence value. */
typedef struct PBL_PACKED {
  /** Confidence in percent (1-100), or 0 if not available. */
  uint8_t confidence;
  /** Length of @ref data in bytes. */
  uint16_t length;
  /** UTF-8 text, not zero terminated. */
  utf8_t data[];
} TranscriptionWord;

/** @brief Sentence, a serialized list of words. */
typedef struct PBL_PACKED {
  /** Number of words. */
  uint16_t word_count;
  /** Serialized words; use transcription_iterate_words() as they have variable length. */
  TranscriptionWord words[];
} TranscriptionSentence;

/**
 * @brief Transcription: one or more sentences of words.
 *
 * Not all recognizers produce several sentences or per-word confidence; the simplest
 * transcription is a single sentence with all confidence values set to 0.
 */
typedef struct PBL_PACKED {
  /** Format of the transcription. */
  TranscriptionType type : 8;
  /** Number of sentences. */
  uint8_t sentence_count;
  /** Serialized sentences; use transcription_iterate_sentences() as they have variable length. */
  TranscriptionSentence sentences[];
} Transcription;

/**
 * @brief Sentence iteration callback.
 *
 * @param sentence Current sentence.
 * @param data Context pointer.
 * @return true to continue, false to stop the iteration.
 */
typedef bool (*TranscriptionSentenceIterateCb)(const TranscriptionSentence *sentence, void *data);

/**
 * @brief Word iteration callback.
 *
 * @param word Current word.
 * @param data Context pointer.
 * @return true to continue, false to stop the iteration.
 */
typedef bool (*TranscriptionWordIterateCb)(const TranscriptionWord *word, void *data);

/**
 * @brief Check that a transcription received from the phone is well formed.
 *
 * The format must be supported, every sentence and word non-empty, words free of control
 * characters other than backspace, and the content must exactly fill @p size bytes.
 *
 * @param transcription Transcription to check, may be NULL.
 * @param size Size of the received buffer in bytes.
 * @return true if the transcription is valid.
 */
bool transcription_validate(const Transcription *transcription, size_t size);

/**
 * @brief Iterate over a serialized list of sentences.
 *
 * @param sentences First sentence.
 * @param count Number of sentences.
 * @param handle_sentence Callback for each sentence, may be NULL to only skip over the list.
 * @param data Context pointer passed to @p handle_sentence.
 * @return End of the list, or the sentence at which the callback stopped the iteration.
 */
void *transcription_iterate_sentences(const TranscriptionSentence *sentences, size_t count,
                                      TranscriptionSentenceIterateCb handle_sentence, void *data);

/**
 * @brief Iterate over a serialized list of words.
 *
 * @param words First word.
 * @param count Number of words.
 * @param handle_word Callback for each word, may be NULL to only skip over the list.
 * @param data Context pointer passed to @p handle_word.
 * @return End of the list, or the word at which the callback stopped the iteration.
 */
void *transcription_iterate_words(const TranscriptionWord *words, size_t count,
                                  TranscriptionWordIterateCb handle_word, void *data);

/** @} */
