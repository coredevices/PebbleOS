/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "vibe_score.h"

#include "process_management/app_manager.h"
#include "syscall/syscall.h"

#include "system/passert.h"
#include "drivers/vibe.h"
#include "applib/applib_malloc.auto.h"
#include "util/net.h"

#define VIBE_SCORE_MAX_REPEAT_DELAY_MS (10000) // matches MAX_VIBE_DURATION_MS in vibe_pattern

static VibeNote *prv_vibe_score_get_note_list(GenericAttribute *notes_attribute) {
  return (VibeNote *)notes_attribute->data;
}

static VibeNoteIndex *prv_vibe_score_get_pattern_list(GenericAttribute *pattern_attribute) {
  return (VibeNoteIndex *)pattern_attribute->data;
}

static unsigned int prv_vibe_score_get_num_note_types(GenericAttribute *notes_attribute) {
  return notes_attribute->length / sizeof(VibeNote);
}

static unsigned int prv_vibe_score_get_pattern_length(GenericAttribute *pattern_attribute) {
  return pattern_attribute->length / sizeof(VibeNoteIndex);
}

static bool prv_vibe_score_resource_is_valid(ResAppNum app_num, uint32_t resource_id,
                                             uint32_t expected_signature, uint32_t *data_size) {
  // Load file signature, and check that it matches the expected_signature
  uint32_t data_signature;
  if (!(sys_resource_load_range(app_num, resource_id, 0, (uint8_t*)&data_signature,
        sizeof(data_signature)) == sizeof(data_signature) &&
        (ntohl(data_signature) == expected_signature))) {
    return false;
  }

  // Data is the second entry after the resource signature
  if (data_size) {
    size_t output_data_size = sys_resource_size(app_num, resource_id) - VIBE_DATA_OFFSET;
    *data_size = output_data_size;
  }
  return true;
}

bool vibe_score_validate(VibeScore *score, uint32_t data_size) {
  if (!score) {
    return false;
  }

  uint32_t total_size = sizeof(VibeScore);

  // check large enough to contain non-flexible parts
  if (data_size < total_size) {
    return false;
  }

  // check version number
  if (score->version > VIBE_SCORE_VERSION) {
    return false;
  }

  // check if attr_list_size is correct
  if (score->attr_list_size != (data_size - (sizeof(VibeScore) - sizeof(GenericAttributeList)))) {
    return false;
  }

  // check exact file size to contain all flexible-sized data
  for (unsigned int i = 0; i < score->attr_list.num_attributes; i++) {
    GenericAttribute * attribute = (GenericAttribute *)((uint8_t *)score + total_size);
    total_size += sizeof(GenericAttribute);
    if (data_size < total_size) {
      return false;
    }
    total_size += attribute->length;
    if (data_size < total_size) {
      return false;
    }
  }
  if (data_size != total_size) {
    return false;
  }

  // check to see all indices point to valid notes
  GenericAttribute *notes_attribute = generic_attribute_find_attribute(&score->attr_list,
                                                                       VibeAttributeId_Notes,
                                                                       score->attr_list_size);
  GenericAttribute *pattern_attribute = generic_attribute_find_attribute(&score->attr_list,
                                                                         VibeAttributeId_Pattern,
                                                                         score->attr_list_size);
  if (!notes_attribute || !pattern_attribute) {
    return false;
  }

  unsigned int num_note_types = prv_vibe_score_get_num_note_types(notes_attribute);
  if (notes_attribute->length != num_note_types * sizeof(VibeNote)) {
    return false;
  }

  VibeNoteIndex *pattern_list = prv_vibe_score_get_pattern_list(pattern_attribute);
  for (unsigned int i = 0; i < prv_vibe_score_get_pattern_length(pattern_attribute); i++) {
    if (pattern_list[i] >= num_note_types) {
      return false;
    }
  }

  GenericAttribute *repeat_delay_attribute =
      generic_attribute_find_attribute(&score->attr_list, VibeAttributeId_RepeatDelay,
                                       score->attr_list_size);
  if (repeat_delay_attribute) {
    if (repeat_delay_attribute->length != sizeof(uint16_t)) {
      return false;
    }
    uint16_t *delay = (uint16_t *)repeat_delay_attribute->data;
    if (*delay > VIBE_SCORE_MAX_REPEAT_DELAY_MS) {
      return false;
    }
  }
  return true;
}

VibeScore *vibe_score_create_with_resource_system(ResAppNum app_num,
                                                  uint32_t resource_id) {
  uint32_t data_size;
  if (!prv_vibe_score_resource_is_valid(app_num, resource_id, VIBE_SIGNATURE, &data_size)) {
    return NULL;
  }

  VibeScore *vibe_score = applib_zalloc(data_size);
  if (!vibe_score || sys_resource_load_range(app_num, resource_id, VIBE_DATA_OFFSET,
                                             (uint8_t*)vibe_score, data_size) != data_size) {
    applib_free(vibe_score);
    return NULL;
  }

  // Validate the loaded command sequence
  if (!vibe_score_validate(vibe_score, data_size)) {
    applib_free(vibe_score);
    return NULL;
  }

  return vibe_score;
}

unsigned int vibe_score_get_duration_ms(VibeScore *score) {
  if (!score) {
    return 0;
  }
  GenericAttribute *notes_attribute = generic_attribute_find_attribute(&score->attr_list,
                                                                       VibeAttributeId_Notes,
                                                                       score->attr_list_size);
  GenericAttribute *pattern_attribute = generic_attribute_find_attribute(&score->attr_list,
                                                                         VibeAttributeId_Pattern,
                                                                         score->attr_list_size);
  PBL_ASSERTN(notes_attribute && pattern_attribute);

  unsigned int duration_ms = 0;
  VibeNote *note_list = prv_vibe_score_get_note_list(notes_attribute);
  VibeNoteIndex *pattern_list = prv_vibe_score_get_pattern_list(pattern_attribute);
  unsigned int pattern_length = prv_vibe_score_get_pattern_length(pattern_attribute);

  for (unsigned int i = 0; i < pattern_length; i++) {
    VibeNote *note = &note_list[pattern_list[i]];
    duration_ms += (note->vibe_duration_ms + note->brake_duration_ms);
  }
  return duration_ms;
}

unsigned int vibe_score_get_repeat_delay_ms(VibeScore *score) {
  if (!score) {
    return 0;
  }
  GenericAttribute *repeat_delay_attribute =
      generic_attribute_find_attribute(&score->attr_list, VibeAttributeId_RepeatDelay,
                                       score->attr_list_size);
  if (repeat_delay_attribute) {
    uint16_t *repeat_delay = (uint16_t *) repeat_delay_attribute->data;
    return *repeat_delay;
  }
  return 0;
}

void vibe_score_do_vibe(VibeScore *score) {
  PBL_ASSERTN(score);
  GenericAttribute *notes_attribute = generic_attribute_find_attribute(&score->attr_list,
                                                                       VibeAttributeId_Notes,
                                                                       score->attr_list_size);
  GenericAttribute *pattern_attribute = generic_attribute_find_attribute(&score->attr_list,
                                                                         VibeAttributeId_Pattern,
                                                                         score->attr_list_size);
  PBL_ASSERTN(notes_attribute && pattern_attribute);

  VibeNote *note_list = prv_vibe_score_get_note_list(notes_attribute);
  VibeNoteIndex *pattern_list = prv_vibe_score_get_pattern_list(pattern_attribute);
  unsigned int pattern_length = prv_vibe_score_get_pattern_length(pattern_attribute);

  for (unsigned int i = 0; i < pattern_length; i++) {
    VibeNote *note = &note_list[pattern_list[i]];
    if (note->vibe_duration_ms > 0) {
      sys_vibe_pattern_enqueue_step_raw(note->vibe_duration_ms, note->strength);
    }
    if (note->brake_duration_ms > 0) {
      sys_vibe_pattern_enqueue_step_raw(note->brake_duration_ms, vibe_get_braking_strength());
    }
  }
  sys_vibe_pattern_trigger_start();
}

VibeScore *vibe_score_create_with_resource(uint32_t resource_id) {
  ResAppNum app_num = sys_get_current_resource_num();
  return vibe_score_create_with_resource_system(app_num, resource_id);
}

void vibe_score_destroy(VibeScore *score) {
  if (!score) {
    return;
  }

  applib_free(score);
}
