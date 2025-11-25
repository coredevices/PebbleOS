/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "gdraw_command_sequence.h"
#include "gdraw_command_private.h"

#include "applib/applib_malloc.auto.h"
#include "applib/applib_resource_private.h"
#include "syscall/syscall.h"

#define GDRAW_COMMAND_SEQUENCE_PLAY_COUNT_INFINITE_STORED ((uint16_t) ~0)

static GDrawCommandFrame *prv_next_frame(GDrawCommandFrame *frame) {
  // Iterate to the end of the command list (next frame starts immediately afterwards)
  return gdraw_command_list_iterate_private(&frame->command_list, NULL, NULL);
}

GDrawCommandSequence *gdraw_command_sequence_create_with_resource(uint32_t resource_id) {
  ResAppNum app_num = sys_get_current_resource_num();
  return gdraw_command_sequence_create_with_resource_system(app_num, resource_id);
}

GDrawCommandSequence *gdraw_command_sequence_create_with_resource_system(ResAppNum app_num,
                                                                         uint32_t resource_id) {
  uint32_t data_size;
  if (!gdraw_command_resource_is_valid(app_num, resource_id, PDCS_SIGNATURE, &data_size)) {
    return NULL;
  }

  GDrawCommandSequence *draw_command_sequence = applib_resource_mmap_or_load(app_num, resource_id,
                                                                             PDCS_DATA_OFFSET,
                                                                             data_size, false);

  // Validate the loaded command sequence
  if (!gdraw_command_sequence_validate(draw_command_sequence, data_size)) {
    gdraw_command_sequence_destroy(draw_command_sequence);
    return NULL;
  }

  return draw_command_sequence;
}

GDrawCommandSequence *gdraw_command_sequence_clone(GDrawCommandSequence *sequence) {
  if (!sequence) {
    return NULL;
  }

  // potentially extracting into a generic task_ptrdup(void *, size_t)
  size_t size = gdraw_command_sequence_get_data_size(sequence);
  GDrawCommandSequence *result = applib_malloc(size);
  if (result) {
    memcpy(result, sequence, size);
  }

  return result;
}

void gdraw_command_sequence_destroy(GDrawCommandSequence *sequence) {
  applib_resource_munmap_or_free(sequence);
}

bool gdraw_command_sequence_validate(GDrawCommandSequence *sequence, size_t size) {
  if (!sequence ||
      (size < sizeof(GDrawCommandSequence)) ||
      (sequence->version > GDRAW_COMMAND_VERSION) ||
      (sequence->num_frames == 0)) {
    return false;
  }

  uint8_t *end = (uint8_t *)sequence + size;
  GDrawCommandFrame *frame = sequence->frames;
  for (uint32_t i = 0; i < sequence->num_frames; i++) {
    if (((uint8_t *) frame >= end) ||
        !gdraw_command_frame_validate(frame, (size_t)(end - (uint8_t *)frame))) {
      return false;
    }
    frame = prv_next_frame(frame);
  }

  return (end == (uint8_t *) frame);
}

static uint32_t prv_get_single_play_duration(GDrawCommandSequence *sequence) {
  uint32_t total = 0;
  GDrawCommandFrame *frame = sequence->frames;
  for (uint32_t i = 0; i < sequence->num_frames; i++) {
    total += gdraw_command_frame_get_duration(frame);
    frame = prv_next_frame(frame);
  }
  return total;
}

GDrawCommandFrame *gdraw_command_sequence_get_frame_by_elapsed(GDrawCommandSequence *sequence,
                                                               uint32_t elapsed) {
  if (!sequence) {
    return NULL;
  }

  if ((sequence->play_count != GDRAW_COMMAND_SEQUENCE_PLAY_COUNT_INFINITE_STORED) &&
      (elapsed >= gdraw_command_sequence_get_total_duration(sequence))) {
    // return the last frame if the elapsed time is longer than the total duration
    return gdraw_command_sequence_get_frame_by_index(sequence, sequence->num_frames - 1);
  }

  elapsed %= prv_get_single_play_duration(sequence);

  uint32_t total = 0;
  GDrawCommandFrame *frame = sequence->frames;
  for (uint32_t i = 0; i < sequence->num_frames; i++) {
    total += gdraw_command_frame_get_duration(frame);

    if (total > elapsed) {
      break;
    }

    frame = prv_next_frame(frame);
  }
  // return the last frame in the sequence if the elapsed time is longer than the total time of the
  // sequence
  return frame;
}

GDrawCommandFrame *gdraw_command_sequence_get_frame_by_index(GDrawCommandSequence *sequence,
                                                             uint32_t index) {
  if (!sequence || (index >= sequence->num_frames)) {
    return NULL;
  }

  GDrawCommandFrame *frame = sequence->frames;
  for (uint32_t i = 0; i < index; i++) {
    frame = prv_next_frame(frame);
  }
  return frame;
}

size_t gdraw_command_sequence_get_data_size(GDrawCommandSequence *sequence) {
  if (!sequence) {
    return 0;
  }

  size_t size = sizeof(GDrawCommandSequence);
  GDrawCommandFrame *frame = sequence->frames;
  for (uint32_t i = 0; i < sequence->num_frames; i++) {
    size += gdraw_command_frame_get_data_size(frame);
    frame = prv_next_frame(frame);
  }

  return size;
}

GSize gdraw_command_sequence_get_bounds_size(GDrawCommandSequence *sequence) {
  if (!sequence) {
    return GSizeZero;
  }

  return sequence->size;
}

void gdraw_command_sequence_set_bounds_size(GDrawCommandSequence *sequence, GSize size) {
  if (!sequence) {
    return;
  }

  sequence->size = size;
}

uint32_t gdraw_command_sequence_get_play_count(GDrawCommandSequence *sequence) {
  if (!sequence) {
    return 0;
  }

  if (sequence->play_count == GDRAW_COMMAND_SEQUENCE_PLAY_COUNT_INFINITE_STORED) {
    return PLAY_COUNT_INFINITE;
  }
  return sequence->play_count;
}

void gdraw_command_sequence_set_play_count(GDrawCommandSequence *sequence, uint32_t play_count) {
  if (!sequence) {
    return;
  }

  sequence->play_count = MIN(play_count, GDRAW_COMMAND_SEQUENCE_PLAY_COUNT_INFINITE_STORED);
}

uint32_t gdraw_command_sequence_get_total_duration(GDrawCommandSequence *sequence) {
  if (!sequence) {
    return 0;
  }

  if (sequence->play_count == GDRAW_COMMAND_SEQUENCE_PLAY_COUNT_INFINITE_STORED) {
    return PLAY_DURATION_INFINITE;
  }
  return prv_get_single_play_duration(sequence) * sequence->play_count;
}

uint32_t gdraw_command_sequence_get_num_frames(GDrawCommandSequence *sequence) {
  if (!sequence) {
    return 0;
  }

  return sequence->num_frames;
}
