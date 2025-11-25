/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "gdraw_command_frame.h"
#include "gdraw_command_private.h"

#include "system/passert.h"

bool gdraw_command_frame_validate(GDrawCommandFrame *frame, size_t size) {
  if (!frame || (size < sizeof(GDrawCommandFrame))) {
    return false;
  }
  return gdraw_command_list_validate(&frame->command_list, size - (sizeof(GDrawCommandFrame) -
          sizeof(GDrawCommandList)));
}

void gdraw_command_frame_draw_processed(GContext *ctx, GDrawCommandSequence *sequence,
                                        GDrawCommandFrame *frame, GPoint offset,
                                        GDrawCommandProcessor *processor) {
  if (!ctx || !frame) {
    return;
  }

  // Note: sequence is passed in here to enable version handling in the future (version field in
  // sequence struct will be used)

  // Offset graphics context drawing box origin by specified amount
  graphics_context_move_draw_box(ctx, offset);

  gdraw_command_list_draw_processed(ctx, &frame->command_list, processor);

  // Offset graphics context drawing box back to previous origin
  graphics_context_move_draw_box(ctx, GPoint(-offset.x, -offset.y));
}

void gdraw_command_frame_draw(GContext *ctx, GDrawCommandSequence *sequence,
                              GDrawCommandFrame *frame, GPoint offset) {
  gdraw_command_frame_draw_processed(ctx, sequence, frame, offset, NULL);
}

void gdraw_command_frame_set_duration(GDrawCommandFrame *frame, uint32_t duration) {
  if (!frame) {
    return;
  }

  frame->duration = duration;
}

uint32_t gdraw_command_frame_get_duration(GDrawCommandFrame *frame) {
  if (!frame) {
    return 0;
  }

  return frame->duration;
}

size_t gdraw_command_frame_get_data_size(GDrawCommandFrame *frame) {
  if (!frame) {
    return 0;
  }

  return sizeof(GDrawCommandFrame) - sizeof(GDrawCommandList) +
      gdraw_command_list_get_data_size(&frame->command_list);
}

GDrawCommandList *gdraw_command_frame_get_command_list(GDrawCommandFrame *frame) {
  if (!frame) {
    return NULL;
  }

  return &frame->command_list;
}
