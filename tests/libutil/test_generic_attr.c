/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <clar.h>

#include <pbl/util/generic_attr.h>

#include <string.h>

enum {
  AttrIdTranscription = 0x02,
  AttrIdAppUuid = 0x03,
};

#define UUID_SIZE 16

// setup and teardown
void test_generic_attr__initialize(void) {
}

void test_generic_attr__cleanup(void) {
}

// tests

void test_generic_attr__find_attribute(void) {
  uint8_t data1[] = {
    0x02, // attribute list - num attributes

    0x02, // attribute type - transcription
    0x2F,
    0x00, // attribute length

    // Transcription
    0x01, // Transcription type
    0x02, // Sentence count

    // Sentence #1
    0x02,
    0x00, // Word count

    // Word #1
    85, // Confidence
    0x05,
    0x00, // Word length
    'H',
    'e',
    'l',
    'l',
    'o',

    // Word #2
    74, // Confidence
    0x08,
    0x00, // Word length
    'c',
    'o',
    'm',
    'p',
    'u',
    't',
    'e',
    'r',

    // Sentence #2
    0x03,
    0x00, // Word count

    // Word #1
    13, // Confidence
    0x04,
    0x00, // Word length
    'h',
    'e',
    'l',
    'l',

    // Word #1
    3, // Confidence
    0x02,
    0x00, // Word length
    'o',
    'h',

    // Word #2
    0, // Confidence
    0x07,
    0x00, // Word length
    'c',
    'o',
    'm',
    'p',
    'u',
    't',
    'a',

    0x03, // attribute type - App UUID
    0x10,
    0x00, // attribute length

    0xa8,
    0xc5,
    0x63,
    0x17,
    0xa2,
    0x89,
    0x46,
    0x5c,
    0xbe,
    0xf1,
    0x5b,
    0x98,
    0x0d,
    0xfd,
    0xb0,
    0x8a,
  };

  // same as data1, but with the attribute order swapped
  uint8_t data2[] = {
    0x02, // attribute list - num attributes

    0x03, // attribute type - App UUID
    0x10,
    0x00, // attribute length

    0xa8,
    0xc5,
    0x63,
    0x17,
    0xa2,
    0x89,
    0x46,
    0x5c,
    0xbe,
    0xf1,
    0x5b,
    0x98,
    0x0d,
    0xfd,
    0xb0,
    0x8a,

    0x02, // attribute type - transcription
    0x2F,
    0x00, // attribute length

    // Transcription
    0x01, // Transcription type
    0x02, // Sentence count

    // Sentence #1
    0x02,
    0x00, // Word count

    // Word #1
    85, // Confidence
    0x05,
    0x00, // Word length
    'H',
    'e',
    'l',
    'l',
    'o',

    // Word #2
    74, // Confidence
    0x08,
    0x00, // Word length
    'c',
    'o',
    'm',
    'p',
    'u',
    't',
    'e',
    'r',

    // Sentence #2
    0x03,
    0x00, // Word count

    // Word #1
    13, // Confidence
    0x04,
    0x00, // Word length
    'h',
    'e',
    'l',
    'l',

    // Word #1
    3, // Confidence
    0x02,
    0x00, // Word length
    'o',
    'h',

    // Word #2
    0, // Confidence
    0x07,
    0x00, // Word length
    'c',
    'o',
    'm',
    'p',
    'u',
    't',
    'a',
  };

  struct pbl_generic_attr_list *attr_list1 = (struct pbl_generic_attr_list *)data1;
  struct pbl_generic_attr_list *attr_list2 = (struct pbl_generic_attr_list *)data2;

  struct pbl_generic_attr *attr1 =
      pbl_generic_attr_find(attr_list1, AttrIdTranscription, sizeof(data1));
  cl_assert(attr1);
  cl_assert_equal_i(attr1->id, AttrIdTranscription);
  cl_assert_equal_i(attr1->length, 0x2F);
  size_t offset = sizeof(struct pbl_generic_attr_list) + sizeof(struct pbl_generic_attr);
  cl_assert_equal_p(attr1->data, &data1[offset]);

  struct pbl_generic_attr *attr2 = pbl_generic_attr_find(attr_list1, AttrIdAppUuid, sizeof(data1));
  cl_assert(attr2);
  cl_assert_equal_i(attr2->id, AttrIdAppUuid);
  cl_assert_equal_i(attr2->length, 16);
  offset = sizeof(struct pbl_generic_attr_list) + sizeof(struct pbl_generic_attr) + attr1->length +
           sizeof(struct pbl_generic_attr);
  cl_assert_equal_p(attr2->data, &data1[offset]);

  attr1 = pbl_generic_attr_find(attr_list2, AttrIdAppUuid, sizeof(data2));
  cl_assert(attr1);
  cl_assert_equal_i(attr1->id, AttrIdAppUuid);
  cl_assert_equal_i(attr1->length, 16);
  offset = sizeof(struct pbl_generic_attr_list) + sizeof(struct pbl_generic_attr);
  cl_assert_equal_p(attr1->data, &data2[offset]);

  attr2 = pbl_generic_attr_find(attr_list2, AttrIdTranscription, sizeof(data2));
  cl_assert(attr2);
  cl_assert_equal_i(attr2->id, AttrIdTranscription);
  cl_assert_equal_i(attr2->length, 0x2F);
  offset = sizeof(struct pbl_generic_attr_list) + sizeof(struct pbl_generic_attr) + attr1->length +
           sizeof(struct pbl_generic_attr);
  cl_assert_equal_p(attr2->data, &data2[offset]);

  struct pbl_generic_attr *attr3 =
      pbl_generic_attr_find(attr_list1, AttrIdAppUuid, sizeof(data1) - 1);
  cl_assert(!attr3);

  attr3 = pbl_generic_attr_find(attr_list1, AttrIdAppUuid, sizeof(data1) - UUID_SIZE);
  cl_assert(!attr3);

  attr3 = pbl_generic_attr_find(attr_list1, AttrIdAppUuid, sizeof(data1) - UUID_SIZE - 1);
  cl_assert(!attr3);
}

void test_generic_attr__add_attribute(void) {
  uint8_t data[] = {0x01, 0x55, 0x77, 0x54, 0x47};
  uint8_t data_out[(2 * sizeof(struct pbl_generic_attr)) + sizeof(data) + UUID_SIZE];
  struct pbl_generic_attr *next = (struct pbl_generic_attr *)data_out;
  next = pbl_generic_attr_add(next, AttrIdTranscription, data, sizeof(data));
  size_t offset = sizeof(struct pbl_generic_attr) + sizeof(data);
  cl_assert_equal_p((uint8_t *)next, &data_out[offset]);
  struct pbl_generic_attr expected = {.id = AttrIdTranscription, .length = sizeof(data)};
  cl_assert_equal_m(&expected, data_out, sizeof(struct pbl_generic_attr));
  cl_assert_equal_m(&data_out[sizeof(struct pbl_generic_attr)], data, sizeof(data));

  uint8_t uuid[UUID_SIZE];
  for (size_t i = 0; i < sizeof(uuid); i++) {
    uuid[i] = (uint8_t)(0xa5 ^ i);
  }
  next = pbl_generic_attr_add(next, AttrIdAppUuid, uuid, sizeof(uuid));
  cl_assert_equal_p((uint8_t *)next, data_out + sizeof(data_out));
  expected = (struct pbl_generic_attr){.id = AttrIdAppUuid, .length = UUID_SIZE};
  cl_assert_equal_m(&expected, &data_out[offset], sizeof(struct pbl_generic_attr));
  offset += sizeof(struct pbl_generic_attr);
  cl_assert_equal_m(uuid, &data_out[offset], sizeof(uuid));
}

void test_generic_attr__empty_attribute_at_end(void) {
  const uint8_t data[] = {0x02, 0x01, 0x01, 0x00, 0xaa, 0x02, 0x00, 0x00};
  struct pbl_generic_attr_list *list = (struct pbl_generic_attr_list *)data;

  struct pbl_generic_attr *attr = pbl_generic_attr_find(list, 0x02, sizeof(data));
  cl_assert(attr);
  cl_assert_equal_i(attr->length, 0);
  cl_assert(!pbl_generic_attr_find(list, 0x02, sizeof(data) - 1));
}
