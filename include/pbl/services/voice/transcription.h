/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/graphics/utf8.h"
#include "util/attributes.h"

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

//! The transcription module validates and manipulates the serialized format of the transcription
//! structure received from the phone over the voice endpoint.

//! Transcription types supported. Only the sentence list transcription is currently supported, but
//! this allows for different formats in future
typedef enum {
  TranscriptionTypeSentenceList = 0x01
} TranscriptionType;

//! A word string with associated confidence value and length. The string is not zero terminated
typedef struct PACKED {
  uint8_t confidence; //!< Word confidence value (1 - 100%) or 0 if confidence value is not valid
  uint16_t length;    //!< Length of word
  utf8_t data[];      //!< UTF-8 encoded text
} TranscriptionWord;

//! A serialized list of words making up a sentence.
typedef struct PACKED {
  uint16_t word_count;
  TranscriptionWord words[];
} TranscriptionSentence;

//! A transcription consists of one of more sentences, each of which is broken up into a list of
//! words with a confidence value for each word. Not all recognizers support multiple sentences and
//! not all support confidence per word. The simplest representation of a string would be a single
//! list of words (with their confidence values set to zero) making up a single sentence.
//! The list of objects is serialized in memory as it would be received over the endpoint.
typedef struct PACKED {
  TranscriptionType type:8;
  uint8_t sentence_count;
  TranscriptionSentence sentences[];
} Transcription;

//! Callback for iterating over a list of word sentences
//! @param sentence   Current sentence in iteration
//! @param data       Context data pointer
//! @return true to continue iteration, false to end iteration
typedef bool (*TranscriptionSentenceIterateCb)(const TranscriptionSentence *sentence,
    void *data);

//! Callback for iterating over a list of words
//! @param word   Current word in iteration
//! @param data   Context data pointer
//! @return true to continue iteration, false to end iteration
typedef bool (*TranscriptionWordIterateCb)(const TranscriptionWord *word, void *data);

//! Check that a transcription object is valid (called to check transcriptions received from phone)
bool transcription_validate(const Transcription *transcription, size_t size);

//! Iterate over a list of serialized TranscriptionSentence objects
//! @param sentences        Beginning of serialized list
//! @param count            Number of TranscriptionSentence objects in list
//! @param handle_sentence  Callback for handling each sentence
//! @param data             Context data pointer - passed into handler callback
//! @return a pointer to the end of the serialized list
void *transcription_iterate_sentences(const TranscriptionSentence *sentences, size_t count,
    TranscriptionSentenceIterateCb handle_sentence, void *data);

//! Iterate over list of serialized TranscriptionWord objects
//! @param words        Beginning of serialized list
//! @param count        Number of words in list
//! @param handle_word  Callback for handling each word
//! @param data         Context data pointer - passed into handler callback
//! @return a pointer to the end of the serialized list
void *transcription_iterate_words(const TranscriptionWord *words, size_t count,
    TranscriptionWordIterateCb handle_word, void *data);
