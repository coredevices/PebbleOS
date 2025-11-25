/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "gdraw_command_list.h"
#include "gdraw_command_private.h"

#include "applib/applib_malloc.auto.h"
#include "system/passert.h"

bool gdraw_command_list_copy(void *buffer, size_t buffer_length, GDrawCommandList *src) {
  size_t src_size = gdraw_command_list_get_data_size(src);
  if (buffer_length < src_size) {
    return false;
  }

  memcpy(buffer, src, src_size);
  return true;
}

GDrawCommandList *gdraw_command_list_clone(GDrawCommandList *list) {
  if (!list) {
    return NULL;
  }

  size_t size = gdraw_command_list_get_data_size(list);
  GDrawCommandList *result = applib_malloc(size);
  if (result) {
    memcpy(result, list, size);
  }

  return result;
}

void gdraw_command_list_destroy(GDrawCommandList *list) {
  if (list) {
    applib_free(list);
  }
}

static GDrawCommand *prv_next_command(GDrawCommand *command) {
  return (GDrawCommand *) (command->points + command->num_points);
}

bool gdraw_command_list_validate(GDrawCommandList *command_list, size_t size) {
  if (!command_list ||
      (size < sizeof(GDrawCommandList)) ||
      (command_list->num_commands == 0)) {
    return false;
  }

  uint8_t *end = (uint8_t *)command_list + size;
  GDrawCommand *command = command_list->commands;
  for (uint32_t i = 0; i < command_list->num_commands; i++) {
    if ((end <= (uint8_t *) command) ||
        !gdraw_command_validate(command, end - (uint8_t *) command)) {
      return false;
    }
    command = prv_next_command(command);
  }

  return ((uint8_t *) command <= end);
}

void *gdraw_command_list_iterate_private(GDrawCommandList *command_list,
                                 GDrawCommandListIteratorCb handle_command,
                                 void *callback_context) {
  if (!command_list) {
    return NULL;
  }

  GDrawCommand *command = command_list->commands;
  for (uint32_t i = 0; i < command_list->num_commands; i++) {
    if ((handle_command) && (!handle_command(command, i, callback_context))) {
        break;
    }
    command = prv_next_command(command);
  }
  return command;
}

void gdraw_command_list_iterate(GDrawCommandList *command_list,
                                 GDrawCommandListIteratorCb handle_command,
                                 void *callback_context) {
  gdraw_command_list_iterate_private(command_list, handle_command, callback_context);
}

GDrawCommand *gdraw_command_list_get_command(GDrawCommandList *command_list, uint16_t command_idx) {
  if (!command_list || (command_idx >= command_list->num_commands)) {
    return NULL;
  }

  GDrawCommand *command = command_list->commands;
  for (uint32_t i = 0; i < command_idx; i++) {
    command = prv_next_command(command);
  }
  return command;
}

static bool prv_draw_command(GDrawCommand *command, uint32_t idx, void *ctx) {
  gdraw_command_draw(ctx, command);
  return true;
}

typedef struct {
  GContext *ctx;
  const GDrawCommandList *list;
  GDrawCommandProcessor *processor;
  GDrawCommand *processed_draw_command;
} GDrawCommandDrawProcessedCBData;

static bool prv_draw_command_processed(GDrawCommand *draw_command, uint32_t idx, void *ctx) {
  GDrawCommandDrawProcessedCBData *data = ctx;

  size_t size = gdraw_command_get_data_size(draw_command);

  memset(data->processed_draw_command, 0, size);
  memcpy(data->processed_draw_command, draw_command, size);
  if (data->processor->command) {
    data->processor->command(data->processor, data->processed_draw_command, size, data->list,
                             draw_command);
  }
  gdraw_command_draw(data->ctx, data->processed_draw_command);
  return true;
}

void gdraw_command_list_draw(GContext *ctx, GDrawCommandList *command_list) {
  gdraw_command_list_draw_processed(ctx, command_list, NULL);
}

static bool prv_iterate_max_command_size(GDrawCommand *command, uint32_t idx, void *ctx) {
  size_t *size = ctx;
  const size_t command_size = gdraw_command_get_data_size(command);
  if (command_size > *size) {
    *size = command_size;
  }
  return true;
}

T_STATIC size_t prv_get_list_max_command_size(GDrawCommandList *command_list) {
  if (!command_list) {
    return 0;
  }

  size_t size = 0;
  gdraw_command_list_iterate(command_list, prv_iterate_max_command_size, &size);
  return size;
}

void gdraw_command_list_draw_processed(GContext *ctx, GDrawCommandList *command_list,
                                       GDrawCommandProcessor *processor) {
  if (!ctx || !command_list) {
    return;
  }

  if (!processor) {
    gdraw_command_list_iterate(command_list, prv_draw_command, ctx);
  } else {
    const size_t max_size = prv_get_list_max_command_size(command_list);

    GDrawCommandDrawProcessedCBData data = {
      .ctx = ctx,
      .list = command_list,
      .processor = processor,
      // malloc because we clear the memory within each iteration of `prv_draw_command_processed`
      .processed_draw_command = applib_malloc(max_size)
    };

    if (data.processed_draw_command) {
      gdraw_command_list_iterate(command_list, prv_draw_command_processed, &data);
      applib_free(data.processed_draw_command);
    }
  }
}

static bool prv_calc_size(GDrawCommand *command, uint32_t idx, void *ctx) {
  size_t *size = ctx;
  *size += gdraw_command_get_data_size(command);

  return true;
}

uint32_t gdraw_command_list_get_num_commands(GDrawCommandList *command_list) {
  if (!command_list) {
    return 0;
  }

  return command_list->num_commands;
}

size_t gdraw_command_list_get_data_size(GDrawCommandList *command_list) {
  if (!command_list) {
    return 0;
  }

  size_t size = sizeof(GDrawCommandList);
  gdraw_command_list_iterate(command_list, prv_calc_size, &size);
  return size;
}

static bool prv_get_num_points(GDrawCommand *command, uint32_t idx, void *ctx) {
  size_t *num_gpoints = ctx;
  *num_gpoints += gdraw_command_get_num_points(command);

  return true;
}

size_t gdraw_command_list_get_num_points(GDrawCommandList *command_list) {
  size_t num_gpoints = 0;
  gdraw_command_list_iterate(command_list, prv_get_num_points, &num_gpoints);
  return num_gpoints;
}

typedef struct {
  const struct {
    GPoint *points;
    bool is_precise;
  } values;
  struct {
    uint32_t current_index;
    size_t bytes_left;
  } iter;
} CollectPointsCBContext;

_Static_assert((sizeof(GPoint) == sizeof(GPointPrecise)),
    "GPointPrecise cannot be convert to GPoint in-place because of its size difference.");

_Static_assert((offsetof(GPoint, y) == offsetof(GPointPrecise, y)),
    "GPointPrecise cannot be convert to GPoint in-place because of its member size difference.");

static bool prv_collect_points(GDrawCommand *command, uint32_t idx, void *ctx) {
  CollectPointsCBContext *collect = ctx;
  const size_t bytes_copied = gdraw_command_copy_points(command,
      &collect->values.points[collect->iter.current_index], collect->iter.bytes_left);
  const uint16_t num_copied = bytes_copied / sizeof(GPoint);

  // convert to regular GPoint
  if (command->type == GDrawCommandTypePrecisePath && !collect->values.is_precise) {
    for (uint16_t i = 0; i < num_copied; i++) {
      GPoint *point_buffer = &collect->values.points[collect->iter.current_index + i];
      GPointPrecise point = *(GPointPrecise *)point_buffer;
      *point_buffer = GPointFromGPointPrecise(point);
    }
  }
  // convert to GPointPrecise
  else if (command->type == GDrawCommandTypePath && collect->values.is_precise) {
    for (uint16_t i = 0; i < num_copied; i++) {
      GPoint *point_buffer = &collect->values.points[collect->iter.current_index + i];
      GPoint point = *point_buffer;
      *(GPointPrecise *)point_buffer = GPointPreciseFromGPoint(point);
    }
  }

  collect->iter.current_index += num_copied;
  collect->iter.bytes_left -= bytes_copied;

  return true;
}

GPoint *gdraw_command_list_collect_points(GDrawCommandList *command_list, bool is_precise,
    uint16_t *num_points_out) {

  const uint16_t num_points = gdraw_command_list_get_num_points(command_list);
  const size_t max_bytes = num_points * sizeof(GPoint);
  GPoint *points = applib_malloc(num_points * sizeof(GPoint));
  if (!points) {
    return NULL;
  }

  CollectPointsCBContext ctx = {
    .values = {
      .points = points,
      .is_precise = is_precise,
    },
    .iter.bytes_left = max_bytes,
  };
  gdraw_command_list_iterate(command_list, prv_collect_points, &ctx);

  if (num_points_out) {
    *num_points_out = num_points;
  }

  return points;
}
