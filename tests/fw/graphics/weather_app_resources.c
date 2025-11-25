/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "weather_app_resources.h"
#include "kernel/pbl_malloc.h"
#include "applib/graphics/gdraw_command_private.h"
#include "applib/ui/animation.h"
#include "util/size.h"

#include <string.h>

#if !TINTIN_FORCE_FIT

#if defined(UNITTEST)
#include <stdio.h>
#endif

GDrawCommandImage *weather_app_resource_create_cloud() {
  GPoint c0_points[] =
      {{22, 6}, {28, 1}, {39, 1}, {46, 10}, {46, 23}, {4, 23}, {4, 14}, {12, 6}};
  GPoint c1_points[] =
      {{17, 12}, {28, 1}, {39, 1}, {46, 10}, {46, 23}, {4, 23}, {4, 14}, {12, 6}, {22, 6}};

  GPoint c2_points[] = {{ 5, 35}, {17, 23}};
  GPoint c3_points[] = {{ 9, 43}, {29, 23}};
  GPoint c4_points[] = {{23, 41}, {41, 23}};

  const uint16_t num_commands = 5;

  GDrawCommandImage *image = task_malloc(
      sizeof(GDrawCommandImage) +
          sizeof(GDrawCommand) * num_commands +
          sizeof(c0_points) + sizeof(c1_points) + sizeof(c2_points)+ sizeof(c3_points) +
          sizeof(c4_points));

  *image = (GDrawCommandImage){
      .version = 1,
      .size = GSize(48, 48),
      .command_list = (GDrawCommandList) {
          .num_commands = num_commands,
      },
  };

  GDrawCommandList *list = &image->command_list;

  GDrawCommand *c0 = gdraw_command_list_get_command(list, 0);
  *c0 = (GDrawCommand){
      .type = GDrawCommandTypePath,
      .fill_color = GColorWhite,
      .num_points = ARRAY_LENGTH(c0_points),
  };
  memcpy(c0->points, c0_points, sizeof(c0_points));

  GDrawCommand *c1 = gdraw_command_list_get_command(list, 1);
  *c1 = (GDrawCommand){
      .type = GDrawCommandTypePath,
      .path_open = true,
      .stroke_color = GColorBlack,
      .stroke_width = 3,
      .num_points = ARRAY_LENGTH(c1_points),
  };
  memcpy(c1->points, c1_points, sizeof(c1_points));

  GDrawCommand *c2 = gdraw_command_list_get_command(list, 2);
  *c2 = (GDrawCommand){
      .type = GDrawCommandTypePath,
      .hidden = false,
      .stroke_color = GColorBlack,
      .stroke_width = 3,
      .path_open = true,
      .num_points = ARRAY_LENGTH(c2_points),
  };
  memcpy(c2->points, c2_points, sizeof(c2_points));

  GDrawCommand *c3 = gdraw_command_list_get_command(list, 3);
  *c3 = *c2;
  memcpy(c3->points, c3_points, sizeof(c3_points));

  GDrawCommand *c4 = gdraw_command_list_get_command(list, 4);
  *c4 = *c2;
  memcpy(c4->points, c4_points, sizeof(c4_points));

  return image;
}

#define COPY_POINTS(target, source) \
  for (size_t i=0; i < ARRAY_LENGTH(source); i++) { \
    (target)[i] = (source)[i]; \
  }

GDrawCommandImage *weather_app_resource_create_cloud_25px(void) {
  GDrawCommandImage *result = weather_app_resource_create_cloud();
  result->size = GSize(25, 25);

  GPoint c0_points[] =
      {        {10, 4}, {13, 1}, {19, 1}, {23, 6}, {23, 13}, {1, 13}, {1, 8}, {4, 4}};
  GPoint c1_points[] =
      {{7, 7}, {13, 1}, {19, 1}, {23, 6}, {23, 13}, {1, 13}, {1, 8}, {4, 4}, {10, 4}};
  GPoint c2_points[] = {{1, 19}, {7, 13}};
  GPoint c3_points[] = {{3, 24}, {14, 13}};
  GPoint c4_points[] = {{11, 23}, {21, 13}};
  COPY_POINTS(gdraw_command_list_get_command(&result->command_list, 0)->points, c0_points)
  COPY_POINTS(gdraw_command_list_get_command(&result->command_list, 1)->points, c1_points)
  COPY_POINTS(gdraw_command_list_get_command(&result->command_list, 2)->points, c2_points)
  COPY_POINTS(gdraw_command_list_get_command(&result->command_list, 3)->points, c3_points)
  COPY_POINTS(gdraw_command_list_get_command(&result->command_list, 4)->points, c4_points)
  return result;
}

GDrawCommandImage *weather_app_resource_create_sun(void) {
  GPoint c0_points[] = {{47, 23}, {5, 23}};
  GPoint c1_points[] = {{13, 10}, {39, 36}};
  GPoint c2_points[] = {{26,  2}, {26, 44}};
  GPoint c3_points[] = {{39, 10}, {13, 36}};
  GPoint c4_points[] = {{21, 11}, {31, 11}, {39, 18}, {39, 28}, {31, 36}, {21, 36}, {13, 28},
      {13, 18}};
  const uint16_t num_commands = 5;

  GDrawCommandImage *image = task_malloc(
      sizeof(GDrawCommandImage) +
          sizeof(GDrawCommand) * num_commands +
          sizeof(c0_points) + sizeof(c1_points) + sizeof(c2_points) + sizeof(c3_points) +
          sizeof(c4_points));

  *image = (GDrawCommandImage){
      .version = 1,
      .size = GSize(48, 48),
      .command_list = (GDrawCommandList) {
          .num_commands = num_commands,
      },
  };


  GDrawCommandList *list = &image->command_list;
  GDrawCommand *c0 = gdraw_command_list_get_command(list, 0);
  *c0 = (GDrawCommand){
      .type = GDrawCommandTypePath,
      .stroke_color = GColorBlack,
      .stroke_width = 3,
      .path_open = true,
      .num_points = ARRAY_LENGTH(c0_points),
  };
  memcpy(c0->points, c0_points, sizeof(c0_points));

  GDrawCommand *c1 = gdraw_command_list_get_command(list, 1);
  *c1 = *c0;
  memcpy(c1->points, c1_points, sizeof(c1_points));

  GDrawCommand *c2 = gdraw_command_list_get_command(list, 2);
  *c2 = *c0;
  memcpy(c2->points, c2_points, sizeof(c2_points));

  GDrawCommand *c3 = gdraw_command_list_get_command(list, 3);
  *c3 = *c0;
  memcpy(c3->points, c3_points, sizeof(c3_points));

  GDrawCommand *c4 = gdraw_command_list_get_command(list, 4);
  *c4 = (GDrawCommand){
      .type = GDrawCommandTypePath,
      .stroke_color = GColorBlack,
      .stroke_width = 3,
      .fill_color = GColorWhite,
      .num_points = ARRAY_LENGTH(c4_points),
  };
  memcpy(c4->points, c4_points, sizeof(c4_points));

  return image;
}

GDrawCommandImage *weather_app_resource_create_sun_25px(void) {
  GDrawCommandImage *result = weather_app_resource_create_sun();
  result->size = GSize(25, 25);

  GPoint c0_points[] = {{0, 12}, {24, 12}};
  GPoint c1_points[] = {{12, 0}, {12, 24}};
  GPoint c2_points[] = {{3, 3}, {21, 21}};
  GPoint c3_points[] = {{3, 21}, {21, 3}};
  GPoint c4_points[] = {{9, 4}, {15, 4}, {20, 9}, {20, 15}, {15, 20}, {9, 20}, {4, 15}, {4, 9}};
  COPY_POINTS(gdraw_command_list_get_command(&result->command_list, 0)->points, c0_points)
  COPY_POINTS(gdraw_command_list_get_command(&result->command_list, 1)->points, c1_points)
  COPY_POINTS(gdraw_command_list_get_command(&result->command_list, 2)->points, c2_points)
  COPY_POINTS(gdraw_command_list_get_command(&result->command_list, 3)->points, c3_points)
  COPY_POINTS(gdraw_command_list_get_command(&result->command_list, 4)->points, c4_points)
  return result;
}
#endif
