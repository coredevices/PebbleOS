/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include <inttypes.h>

// Test Data
///////////////////////////////////////////////////////////
//!   UTF-8 Decoding Test Case
//!
//! Arbitrary codepoints are selected from a variety of codepages. This is to
//! ensure codepoints can be decoded from a mix of codepages---even if our
//! characterset does not support such a wide range of codepages.
//!
//!   Codepoint    UTF-8          Glyph Description
//! ------------+--------------+--------------------
//! U+0061       \61            a
//! U+01F1       \c7\b1         DZ
//! U+0062       \62            b
//! U+0373       \cd\b3         T (greek)
//! U+0001D107   \f0\9d\84\87   music right repeat
//! U+1000       \e1\80\80      M (myanmar kha)
//! U+0001D12b   \f0\9d\84\ab   bb double flat
//! U+00F0       \c3\b0         d- (eth)
//! U+200E       \e2\80\8e      left-to-right mark
//! U+FEFF       \ef\bb\bf      byte order mark
//! ------------+--------------+--------------------
static const char* s_valid_test_string = "\x61\xc7\xb1\x62\xcd\xb3\xf0\x9d\x84\x87\xe1\x80\x80\xf0\x9d\x84\xab\xc3\xb0\xe2\x80\x8e\xef\xbb\xbf";

static const uint32_t s_valid_test_codepoints[] = {
  0x0061, 0x01F1, 0x0062, 0x0373, 0x0001D107, 0x1000, 0x0001D12b, 0x00F0, 0x200E, 0xFEFF,
};

//! Malformed UTF-8 test string
//!
//! This is the same as the test string, except U+0373 has been corrupted to
//! \xcd\xf3 (invalid UTF-8)
static const char* s_malformed_test_string = "\x61\xc7\xb1\x62\xcd\xf3\xf0\x9d\x84\x87\xe1\x80\x80\xf0\x9d\x84\xab\xc3\xb0";
static const int UTF8_TEST_MALFORMED_CODEPOINT_INDEX = 4;

static const char* s_valid_gothic_codepoints_string = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~ ¡¢£¤¥¦§¨©ª«¬®¯°±²³´µ¶·¸¹º»¼½¾¿ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏÐÑÒÓÔÕÖ×ØÙÚÛÜÝÞßàáâãäåæçèéêëìíîïðñòóôõö÷øùúûüýþÿıŁłŒœŠšŸŽžƒˆˇ˘˙˚˛˜˝π–—‘’‚“”„†‡•…‰‹›⁄€™Ω∂∆∏∑−√∞∫≈≠≤≥◊ﬁﬂ";

static const uint32_t s_valid_gothic_codepoints[] = {
  32, 33, 34, 35, 36, 37, 38, 39, 40, 41,
  42, 43, 44, 45, 46, 47, 48, 49, 50, 51,
  52, 53, 54, 55, 56, 57, 58, 59, 60, 61,
  62, 63, 64, 65, 66, 67, 68, 69, 70, 71,
  72, 73, 74, 75, 76, 77, 78, 79, 80, 81,
  82, 83, 84, 85, 86, 87, 88, 89, 90, 91,
  92, 93, 94, 95, 96, 97, 98, 99, 100, 101,
  102, 103, 104, 105, 106, 107, 108, 109, 110, 111,
  112, 113, 114, 115, 116, 117, 118, 119, 120, 121,
  122, 123, 124, 125, 126, 160, 161, 162, 163, 164,
  165, 166, 167, 168, 169, 170, 171, 172, 174, 175,
  176, 177, 178, 179, 180, 181, 182, 183, 184, 185,
  186, 187, 188, 189, 190, 191, 192, 193, 194, 195,
  196, 197, 198, 199, 200, 201, 202, 203, 204, 205,
  206, 207, 208, 209, 210, 211, 212, 213, 214, 215,
  216, 217, 218, 219, 220, 221, 222, 223, 224, 225,
  226, 227, 228, 229, 230, 231, 232, 233, 234, 235,
  236, 237, 238, 239, 240, 241, 242, 243, 244, 245,
  246, 247, 248, 249, 250, 251, 252, 253, 254, 255,
  305, 321, 322, 338, 339, 352, 353, 376, 381, 382,
  402, 710, 711, 728, 729, 730, 731, 732, 733, 960,
  8211, 8212, 8216, 8217, 8218, 8220, 8221, 8222, 8224, 8225,
  8226, 8230, 8240, 8249, 8250, 8260, 8364, 8482, 8486, 8706,
  8710, 8719, 8721, 8722, 8730, 8734, 8747, 8776, 8800, 8804,
  8805, 9674, 64257, 64258
};

// 你好, or "hello"
#define NIHAO "\xe4\xbd\xa0\xe5\xa5\xbd"
// WORD_JOIN codepoints added between each character to prevent
// this word from being broken up between lines
#define NIHAO_JOINED  "\xe4\xbd\xa0\xe2\x81\xa0\xe5\xa5\xbd"
// Only the first character from NIHAO
#define NIHAO_FIRST_CHARACTER "\xe4\xbd\xa0"
#define NIHAO_FIRST_CHARACTER_BYTES 3

// 你好吗 or "how are you"
#define NIHAOMA "\xe4\xbd\xa0\xe5\xa5\xbd\xe5\x90\x97"
// WORD_JOIN codepoints added between each character to prevent
// this word from being broken up between lines
#define NIHAOMA_JOINED "\xe4\xbd\xa0\xe2\x81\xa0\xe5\xa5\xbd\xe2\x81\xa0\xe5\x90\x97"

