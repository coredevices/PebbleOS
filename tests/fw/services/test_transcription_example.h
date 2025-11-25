/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */


static uint8_t s_test_transcription_example[] = {

  // Transcription
  0x01,         // Transcription type
  0x02,         // Sentence count

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

