/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "services/normal/voice_endpoint_private.h"

#include "fake_pebble_tasks.h"

#include "stubs_logging.h"
#include "stubs_passert.h"
#include "stubs_rand_ptr.h"

// setup and teardown
void test_generic_attribute__initialize(void) {
}

void test_generic_attribute__cleanup(void) {
}

// tests

void test_generic_attribute__find_attribute(void) {
  uint8_t data1[] = {
    0x02,         // attribute list - num attributes

    0x02,         // attribute type - transcription
    0x2F, 0x00,   // attribute length

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

    0x03,         // attribute type - App UUID
    0x10, 0x00,   // attribute length

    0xa8, 0xc5, 0x63, 0x17, 0xa2, 0x89, 0x46, 0x5c,
    0xbe, 0xf1, 0x5b, 0x98, 0x0d, 0xfd, 0xb0, 0x8a,
  };

  // same as data1, but with the attribute order swapped
  uint8_t data2[] = {
    0x02,         // attribute list - num attributes

    0x03,         // attribute type - App UUID
    0x10, 0x00,   // attribute length

    0xa8, 0xc5, 0x63, 0x17, 0xa2, 0x89, 0x46, 0x5c,
    0xbe, 0xf1, 0x5b, 0x98, 0x0d, 0xfd, 0xb0, 0x8a,

    0x02,         // attribute type - transcription
    0x2F, 0x00,   // attribute length

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

  GenericAttributeList *attr_list1 = (GenericAttributeList *)data1;
  GenericAttributeList *attr_list2 = (GenericAttributeList *)data2;

  GenericAttribute *attr1 = generic_attribute_find_attribute(attr_list1, VEAttributeIdTranscription,
                                                             sizeof(data1));
  cl_assert(attr1);
  cl_assert_equal_i(attr1->id, VEAttributeIdTranscription);
  cl_assert_equal_i(attr1->length, 0x2F);
  size_t offset = sizeof(GenericAttributeList) + sizeof(GenericAttribute);
  cl_assert_equal_p(attr1->data, &data1[offset]);

  GenericAttribute *attr2 = generic_attribute_find_attribute(attr_list1, VEAttributeIdAppUuid,
                                                             sizeof(data1));
  cl_assert(attr2);
  cl_assert_equal_i(attr2->id, VEAttributeIdAppUuid);
  cl_assert_equal_i(attr2->length, 16);
  offset = sizeof(GenericAttributeList) + sizeof(GenericAttribute) +
      attr1->length + sizeof(GenericAttribute);
  cl_assert_equal_p(attr2->data, &data1[offset]);

  attr1 = generic_attribute_find_attribute(attr_list2, VEAttributeIdAppUuid, sizeof(data2));
  cl_assert(attr1);
  cl_assert_equal_i(attr1->id, VEAttributeIdAppUuid);
  cl_assert_equal_i(attr1->length, 16);
  offset = sizeof(GenericAttributeList) + sizeof(GenericAttribute);
  cl_assert_equal_p(attr1->data, &data2[offset]);

  attr2 = generic_attribute_find_attribute(attr_list2, VEAttributeIdTranscription, sizeof(data2));
  cl_assert(attr2);
  cl_assert_equal_i(attr2->id, VEAttributeIdTranscription);
  cl_assert_equal_i(attr2->length, 0x2F);
  offset = sizeof(GenericAttributeList) + sizeof(GenericAttribute) +
      attr1->length + sizeof(GenericAttribute);
  cl_assert_equal_p(attr2->data, &data2[offset]);

  GenericAttribute *attr3 = generic_attribute_find_attribute(attr_list1, VEAttributeIdAppUuid,
                                                             sizeof(data1) - 1);
  cl_assert(!attr3);

  attr3 = generic_attribute_find_attribute(attr_list1, VEAttributeIdAppUuid,
                                           sizeof(data1) - sizeof(Uuid));
  cl_assert(!attr3);

  attr3 = generic_attribute_find_attribute(attr_list1, VEAttributeIdAppUuid,
                                           sizeof(data1) - sizeof(Uuid) - 1);
  cl_assert(!attr3);
}

void test_generic_attribute__add_attribute(void) {
  uint8_t data[] = {
    0x01, 0x55, 0x77, 0x54, 0x47
  };
  uint8_t data_out[(2 * sizeof(GenericAttribute)) + sizeof(data) + sizeof(Uuid)];
  GenericAttribute *next = (GenericAttribute *)data_out;
  next = generic_attribute_add_attribute(next, VEAttributeIdTranscription, data, sizeof(data));
  size_t offset = sizeof(GenericAttribute) + sizeof(data);
  cl_assert_equal_p((uint8_t*)next, &data_out[offset]);
  GenericAttribute expected = {
    .id = VEAttributeIdTranscription,
    .length = sizeof(data)
  };
  cl_assert_equal_m(&expected, data_out, sizeof(GenericAttribute));
  cl_assert_equal_m(&data_out[sizeof(GenericAttribute)], data, sizeof(data));

  Uuid uuid;
  uuid_generate(&uuid);
  next = generic_attribute_add_attribute(next, VEAttributeIdAppUuid, &uuid, sizeof(uuid));
  cl_assert_equal_p((uint8_t*)next, data_out + sizeof(data_out));
  expected = (GenericAttribute) {
    .id = VEAttributeIdAppUuid,
    .length = sizeof(Uuid)
  };
  cl_assert_equal_m(&expected, &data_out[offset], sizeof(GenericAttribute));
  offset += sizeof(GenericAttribute);
  cl_assert_equal_m(&uuid, &data_out[offset], sizeof(uuid));
}
