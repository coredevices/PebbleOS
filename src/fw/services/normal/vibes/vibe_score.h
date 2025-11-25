/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "resource/resource.h"
#include "util/attributes.h"
#include "util/generic_attribute.h"
#include "util/pack.h"

#define VIBE_SCORE_VERSION (1)
#define VIBE_SIGNATURE MAKE_WORD('V', 'I', 'B', 'E')
#define VIBE_DATA_OFFSET sizeof(VIBE_SIGNATURE)

typedef enum VibeAttributeId {
  VibeAttributeId_Invalid = 0x00,
  VibeAttributeId_Notes = 0x01,
  VibeAttributeId_Pattern = 0x02,
  VibeAttributeId_RepeatDelay = 0x03,
} VibeAttributeId;

typedef struct PACKED VibeScore {
  uint16_t version;
  uint8_t reserved[4];
  uint16_t attr_list_size;
  GenericAttributeList attr_list;
} VibeScore;

typedef struct PACKED VibeNote {
  uint16_t vibe_duration_ms;
  uint8_t brake_duration_ms;
  int8_t strength;
} VibeNote;

typedef uint8_t VibeNoteIndex;

// Fetches a vibe score from resources, specifying a ResAppNum
// Must be freed using vibe_score_destroy()
VibeScore *vibe_score_create_with_resource_system(ResAppNum app_num, uint32_t resource_id);

// Fetches a vibe score from resources, using the caller ResAppNum
// Must be freed using vibe_score_destroy()
VibeScore *vibe_score_create_with_resource(uint32_t resource_id);

// Checks whether a vibe score is valid, given its data size
bool vibe_score_validate(VibeScore *score, uint32_t data_size);

// Returns the duration in ms of the vibe pattern specified by the score
unsigned int vibe_score_get_duration_ms(VibeScore *score);

// Returns the value of the repeat_delay attribute, or 0 if it does not exist
unsigned int vibe_score_get_repeat_delay_ms(VibeScore *score);

// Queues the vibe pattern specified by the score and starts the vibe motor
// If there the system is already playing a vibe, this will do nothing
void vibe_score_do_vibe(VibeScore *score);

// Frees a vibe created with vibe_score_create_with_resource
void vibe_score_destroy(VibeScore *score);
