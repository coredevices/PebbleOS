/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "services/normal/voice/transcription.h"

#include "test_transcription_example.h"

static int s_count;

// setup and teardown
void test_transcription__initialize(void) {
}

void test_transcription__cleanup(void) {
}

static bool prv_cb_return_true(void *w, void *data) {
  return true;
}

static bool prv_cb_return_false(void *w, void *data) {
  return (s_count++ != (int)data);
}

void test_transcription__validate(void) {
  bool result;
  Transcription * validate_test = (Transcription *) s_test_transcription_example;
  size_t test_size = sizeof(s_test_transcription_example);
  result = transcription_validate(validate_test, test_size);
  cl_assert_equal_p(result, true);

  result = transcription_validate(NULL, test_size);
  cl_assert_equal_p(result, false);

  result = transcription_validate(validate_test, test_size - 1);
  cl_assert_equal_p(result, false);

  result = transcription_validate(validate_test, test_size + 1);
  cl_assert_equal_p(result, false);

  result = transcription_validate(validate_test, 0);
  cl_assert_equal_p(result, false);

  result = transcription_validate(validate_test, sizeof(Transcription));
  cl_assert_equal_p(result, false);

  result = transcription_validate(validate_test, sizeof(Transcription) - 1);
  cl_assert_equal_p(result, false);

  s_test_transcription_example[0] = 0;
  result = transcription_validate(validate_test, test_size);
  cl_assert_equal_p(result, false);
  s_test_transcription_example[0] = 1;

  s_test_transcription_example[2] = 3; // invalidate word count of the first sentence
  result = transcription_validate(validate_test, test_size);
  cl_assert_equal_p(result, false);
  s_test_transcription_example[2] = 2;   // restore word count of the first sentence

  s_test_transcription_example[5] = 4;   // invalidate word length of first word in first sentence
  result = transcription_validate(validate_test, test_size);
  cl_assert_equal_p(result, false);
  s_test_transcription_example[5] = 5;   // restore word length of first word in first

  s_test_transcription_example[23] = 2;  // invalidate Word count of the second sentence
  result = transcription_validate(validate_test, test_size);
  cl_assert_equal_p(result, false);
  s_test_transcription_example[23] = 3;  // restore word count of the second sentence

  s_test_transcription_example[26] = 0;  // invalidate word length of first word in second sentence
  result = transcription_validate(validate_test, test_size);
  cl_assert_equal_p(result, false);

  s_test_transcription_example[26] = 5;  // invalidate word length of first word in second sentence
  result = transcription_validate(validate_test, test_size);
  cl_assert_equal_p(result, false);
  s_test_transcription_example[26] = 4;  // restore word length of first word in second sentence

  s_test_transcription_example[8] = '\n';  // insert special character into the middle of a word
  result = transcription_validate(validate_test, test_size);
  cl_assert_equal_p(result, false);

  s_test_transcription_example[8] = '\0';  // insert null terminator into the middle of a word
  result = transcription_validate(validate_test, test_size);
  cl_assert_equal_p(result, false);
  s_test_transcription_example[8] = 'e';  // restore word

  s_test_transcription_example[11] = '\0';  // insert null terminator at the end of a word
  result = transcription_validate(validate_test, test_size);
  cl_assert_equal_p(result, false);

  s_test_transcription_example[9] = '\xe2';    // insert valid utf8 into a word
  s_test_transcription_example[10] = '\x9d';
  s_test_transcription_example[11] = '\xa4';
  result = transcription_validate(validate_test, test_size);
  cl_assert_equal_p(result, true);

  s_test_transcription_example[test_size - 1] = '\0'; // insert null terminator at the end of last word
  result = transcription_validate(validate_test, test_size);
  cl_assert_equal_p(result, false);
}

// tests
void test_transcription__iterate_words(void) {
  uint8_t words_test[] = {
    0,
    0x04, 0x00,
    't', 'e', 's', 't',

    51,
    0x05, 0x00,
    'h', 'e', 'l', 'l', 'o',

    101,
    0x03, 0x00,
    't', 'h', 'e'
  };

  uint8_t *end = words_test + sizeof(words_test);
  uint8_t *result;
  result = transcription_iterate_words((TranscriptionWord *)words_test, 3,
      (TranscriptionWordIterateCb)prv_cb_return_true, NULL);
  cl_assert_equal_p(result, end);

  result = transcription_iterate_words((TranscriptionWord *)words_test, 2,
      (TranscriptionWordIterateCb)prv_cb_return_true, NULL);
  cl_assert_equal_p(result, &words_test[15]);

  s_count = 0;
  result = transcription_iterate_words((TranscriptionWord *)words_test, 3,
      (TranscriptionWordIterateCb)prv_cb_return_false, (void *) 0);
  cl_assert_equal_p(result, words_test);

  s_count = 0;
  result = transcription_iterate_words((TranscriptionWord *)words_test, 3,
      (TranscriptionWordIterateCb)prv_cb_return_false, (void *) 1);
  cl_assert_equal_p(result, &words_test[7]);

  s_count = 0;
  result = transcription_iterate_words((TranscriptionWord *)words_test, 3,
      (TranscriptionWordIterateCb)prv_cb_return_false, (void *) 2);
  cl_assert_equal_p(result, &words_test[15]);

  result = transcription_iterate_words((TranscriptionWord *)words_test, 3, NULL, NULL);
  cl_assert_equal_p(result, end);
}

void test_transcription__iterate_sentences(void) {
  uint8_t sentence_test[] = {
    // Sentence #1
    0x02, 0x00,   // Word count

    // Word #1
    85,           // Confidence
    0x05, 0x00,   // Word length
    'H', 'e', 'l', 'l', 'o',

    // Word #2
    74,           // Confidence
    0x08, 0x00,   // Word length
    'c', 'o', 'm', 'p', 'u', 't', 'e', 'r',

    // Sentence #2
    0x03, 0x00,   // Word count

    // Word #1
    13,           // Confidence
    0x04, 0x00,   // Word length
    'h', 'e', 'l', 'l',

    // Word #1
    3,           // Confidence
    0x02, 0x00,   // Word length
    'o', 'h',

    // Word #2
    0,           // Confidence
    0x07, 0x00,   // Word length
    'c', 'o', 'm', 'p', 'u', 't', 'a',
  };

  uint8_t *end = sentence_test + sizeof(sentence_test);

  uint8_t *result;
  result = transcription_iterate_sentences((TranscriptionSentence *) sentence_test, 2,
      (TranscriptionSentenceIterateCb)prv_cb_return_true, NULL);
  cl_assert_equal_p(result, end);

  result = transcription_iterate_sentences((TranscriptionSentence *) sentence_test, 1,
      (TranscriptionSentenceIterateCb)prv_cb_return_true, NULL);
  cl_assert_equal_p(result, &sentence_test[21]);

  s_count = 0;
  result = transcription_iterate_sentences((TranscriptionSentence *) sentence_test, 2,
      (TranscriptionSentenceIterateCb)prv_cb_return_false, (void *) 0);
  cl_assert_equal_p(result, sentence_test);

  s_count = 0;
  result = transcription_iterate_sentences((TranscriptionSentence *) sentence_test, 2,
      (TranscriptionSentenceIterateCb)prv_cb_return_false, (void *) 1);
  cl_assert_equal_p(result, &sentence_test[21]);

  result = transcription_iterate_sentences((TranscriptionSentence *) sentence_test, 0,
      (TranscriptionSentenceIterateCb)prv_cb_return_false, NULL);
  cl_assert_equal_p(result, sentence_test);

  result = transcription_iterate_sentences((TranscriptionSentence *) sentence_test, 2,
      NULL, NULL);
  cl_assert_equal_p(result, end);

}



