/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "services/normal/vibes/vibe_score.h"

#include "clar.h"
#include "string.h"

// stub
#include "stubs_logging.h"
#include "stubs_passert.h"

void sys_vibe_pattern_trigger_start(void) {}
ResAppNum sys_get_current_resource_num(void) {
  return 0;
}

const uint8_t *sys_resource_read_only_bytes(ResAppNum app_num, uint32_t resource_id,
                                            size_t *num_bytes_out) {
  return NULL;
}

int8_t vibe_get_braking_strength(void) {
  return -100;
}

// fake
typedef struct VibeStep {
  uint32_t duration_ms;
  int strength;
} VibeStep;

static VibeStep s_vibe_queue[256];
static int s_vibe_queue_index;
bool sys_vibe_pattern_enqueue_step_raw(uint32_t duration_ms, int32_t strength) {
  s_vibe_queue[s_vibe_queue_index] = (VibeStep){ .duration_ms = duration_ms, .strength = strength };
  s_vibe_queue_index++;
  return true;
}

static uint8_t *s_resource_buffer;
static size_t s_resource_buffer_size;
size_t sys_resource_load_range(ResAppNum app_num, uint32_t id, uint32_t start_bytes,
                               uint8_t *buffer, size_t num_bytes) {
  memcpy(buffer, s_resource_buffer + start_bytes, num_bytes);
  return num_bytes;
}
size_t sys_resource_size(ResAppNum app_num, uint32_t id) {
  return s_resource_buffer_size;
}

//helpers


//unit test code
void test_vibe_score__initialize(void) {
  s_vibe_queue_index = 0;
}

void test_vibe_score__cleanup(void) {
}

void test_vibe_score__double_pulse(void) {
  uint8_t buffer[] = {
      'V', 'I', 'B', 'E', // FourCC
      1, 0, // version
      0, 0, 0, 0, // reserved bytes
      18, 0, // attr_list_size
      2, // GenericAttributeList.num_attributes
      VibeAttributeId_Notes,
      8, 0, // GenericAttribute.length
      15, 0, // VibeNote.vibe_duration_ms
      9, // VibeNote.brake_duration_ms
      100, // VibeNote.strength
      100, 0, // VibeNote.vibe_duration_ms
      0, // VibeNote.brake_duration_ms
      0, // VibeNote.strength
      VibeAttributeId_Pattern,
      3, 0, // GenericAttribute.length
      0,
      1,
      0
  };
  s_resource_buffer = buffer;
  s_resource_buffer_size = sizeof(buffer);
  VibeScore *score = vibe_score_create_with_resource_system(0, 0);
  cl_assert(score);
  vibe_score_do_vibe(score);
  vibe_score_destroy(score);

  cl_assert_equal_i(s_vibe_queue_index, 5);
  cl_assert_equal_i(s_vibe_queue[0].duration_ms, 15);
  cl_assert_equal_i(s_vibe_queue[0].strength, 100);
  cl_assert_equal_i(s_vibe_queue[1].duration_ms, 9);
  cl_assert_equal_i(s_vibe_queue[1].strength, -100);
  cl_assert_equal_i(s_vibe_queue[2].duration_ms, 100);
  cl_assert_equal_i(s_vibe_queue[2].strength, 0);
  cl_assert_equal_i(s_vibe_queue[3].duration_ms, 15);
  cl_assert_equal_i(s_vibe_queue[3].strength, 100);
  cl_assert_equal_i(s_vibe_queue[4].duration_ms, 9);
  cl_assert_equal_i(s_vibe_queue[4].strength, -100);
}

void test_vibe_score__repeat_delay_is_valid(void) {
  uint8_t buffer[] = {
      'V', 'I', 'B', 'E', // FourCC
      1, 0, // version
      0, 0, 0, 0, // reserved bytes
      23, 0, // attr_list_size
      3, // GenericAttributeList.num_attributes
      VibeAttributeId_Notes,
      8, 0, // GenericAttribute.length
      15, 0, // VibeNote.vibe_duration_ms
      9, // VibeNote.brake_duration_ms
      100, // VibeNote.strength
      100, 0, // VibeNote.vibe_duration_ms
      0, // VibeNote.brake_duration_ms
      0, // VibeNote.strength
      VibeAttributeId_Pattern,
      3, 0, // GenericAttribute.length
      0,
      1,
      0,
      VibeAttributeId_RepeatDelay,
      2, 0, // GenericAttribute.length (2 bytes for a uint16)
      12, 12 // repeat_delay value
  };
  s_resource_buffer = buffer;
  s_resource_buffer_size = sizeof(buffer);
  VibeScore *score = vibe_score_create_with_resource_system(0, 0);
  cl_assert(score);
  vibe_score_destroy(score);
}

void test_vibe_score__test_get_duration_ms(void) {
  uint8_t buffer[] = {
      'V', 'I', 'B', 'E', // FourCC
      1, 0, // version
      0, 0, 0, 0, // reserved bytes
      18, 0, // attr_list_size
      2, // GenericAttributeList.num_attributes
      VibeAttributeId_Notes,
      8, 0, // GenericAttribute.length
      200, 0, // VibeNote.vibe_duration_ms
      1, // VibeNote.brake_duration_ms
      100, // VibeNote.strength
      150, 0, // VibeNote.vibe_duration_ms
      0, // VibeNote.brake_duration_ms
      0, // VibeNote.strength
      VibeAttributeId_Pattern,
      3, 0, // GenericAttribute.length
      0,
      1,
      0
  };
  s_resource_buffer = buffer;
  s_resource_buffer_size = sizeof(buffer);
  VibeScore *score = vibe_score_create_with_resource_system(0, 0);
  cl_assert(score);
  cl_assert_equal_i(vibe_score_get_duration_ms(score),(201 + 150 + 201));
  vibe_score_destroy(score);
}

void test_vibe_score__test_get_repeat_delay_ms_custom_delay(void) {
  uint8_t buffer[] = {
      'V', 'I', 'B', 'E', // FourCC
      1, 0, // version
      0, 0, 0, 0, // reserved bytes
      23, 0, // attr_list_size
      3, // GenericAttributeList.num_attributes
      VibeAttributeId_Notes,
      8, 0, // GenericAttribute.length
      200, 0, // VibeNote.vibe_duration_ms
      1, // VibeNote.brake_duration_ms
      100, // VibeNote.strength
      150, 0, // VibeNote.vibe_duration_ms
      0, // VibeNote.brake_duration_ms
      0, // VibeNote.strength
      VibeAttributeId_Pattern,
      3, 0, // GenericAttribute.length
      0,
      1,
      0,
      VibeAttributeId_RepeatDelay,
      2, 0, // length, in bytes (2 for a uint16)
      87, 4 // 1111 ms
  };
  s_resource_buffer = buffer;
  s_resource_buffer_size = sizeof(buffer);
  VibeScore *score = vibe_score_create_with_resource_system(0, 0);
  cl_assert(score);
  cl_assert_equal_i(vibe_score_get_repeat_delay_ms(score), 1111);
  vibe_score_destroy(score);
}

void test_vibe_score__test_get_repeat_delay_ms_default_delay(void) {
  uint8_t buffer[] = {
      'V', 'I', 'B', 'E', // FourCC
      1, 0, // version
      0, 0, 0, 0, // reserved bytes
      18, 0, // attr_list_size
      2, // GenericAttributeList.num_attributes
      VibeAttributeId_Notes,
      8, 0, // GenericAttribute.length
      200, 0, // VibeNote.vibe_duration_ms
      1, // VibeNote.brake_duration_ms
      100, // VibeNote.strength
      150, 0, // VibeNote.vibe_duration_ms
      0, // VibeNote.brake_duration_ms
      0, // VibeNote.strength
      VibeAttributeId_Pattern,
      3, 0, // GenericAttribute.length
      0,
      1,
      0
  };
  s_resource_buffer = buffer;
  s_resource_buffer_size = sizeof(buffer);
  VibeScore *score = vibe_score_create_with_resource_system(0, 0);
  cl_assert(score);
  cl_assert_equal_i(vibe_score_get_repeat_delay_ms(score), 0);
  vibe_score_destroy(score);
}

void test_vibe_score__test_bad_attr_size(void) {
  uint8_t buffer[] = {
      'V', 'I', 'B', 'E', // FourCC
      1, 0, // version
      0, 0, 0, 0, // reserved bytes
      11, 0, // attr_list_size (right value is 12)
      2, // GenericAttributeList.num_attributes
      VibeAttributeId_Notes,
      4, 0, // GenericAttribute.length
      1, 0, // VibeNote.vibe_duration_ms
      1, // VibeNote.brake_duration_ms
      100, // VibeNote.strength
      VibeAttributeId_Pattern,
      1, 0, // GenericAttribute.length
      0
  };
  s_resource_buffer = buffer;
  s_resource_buffer_size = sizeof(buffer);
  VibeScore *score = vibe_score_create_with_resource_system(0, 0);
  cl_assert(!score);
}

void test_vibe_score__get_duration_returns_zero_for_null_score(void) {
  cl_assert_equal_i(vibe_score_get_duration_ms(NULL), 0);
}

void test_vibe_score__get_repeat_delay_returns_zero_for_null_score(void) {
  cl_assert_equal_i(vibe_score_get_repeat_delay_ms(NULL), 0);
}
