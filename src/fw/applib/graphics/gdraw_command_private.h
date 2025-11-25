/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "gdraw_command.h"
#include "gdraw_command_list.h"
#include "gdraw_command_image.h"
#include "gdraw_command_frame.h"
#include "gdraw_command_sequence.h"

#include "applib/graphics/gtypes.h"
#include "util/pack.h"

#include <stdint.h>
#include <stdbool.h>

#define GDRAW_COMMAND_VERSION (1)

#define PDCS_SIGNATURE MAKE_WORD('P', 'D', 'C', 'S')
#define PDCS_SIZE_OFFSET sizeof(PDCS_SIGNATURE)
#define PDCS_DATA_OFFSET (PDCS_SIZE_OFFSET + sizeof(uint32_t))

#define PDCI_SIGNATURE MAKE_WORD('P', 'D', 'C', 'I')
#define PDCI_SIZE_OFFSET sizeof(PDCI_SIGNATURE)
#define PDCI_DATA_OFFSET (PDCI_SIZE_OFFSET + sizeof(uint32_t))

struct __attribute__((__packed__)) GDrawCommand {
  GDrawCommandType type:8;
  struct {
    uint8_t hidden:1;
    uint8_t reserved:7;
  };
  GColor stroke_color;
  uint8_t  stroke_width;
  GColor fill_color;
  union {
    struct { // path
      bool path_open;
    };
    struct { // circle
      uint16_t radius;
    };
  };
  union {
    struct {
      uint16_t num_points;
      GPoint points[];
    };
    struct {
      uint16_t num_precise_points;
      GPointPrecise precise_points[];
    };
  };
};

struct __attribute__((__packed__)) GDrawCommandList {
  uint16_t num_commands;
  GDrawCommand commands[];
};

struct __attribute__((__packed__)) GDrawCommandImage {
  uint8_t version;
  uint8_t reserved;
  GSize size;
  GDrawCommandList command_list;
};

struct __attribute__((__packed__)) GDrawCommandFrame {
  uint16_t duration;
  GDrawCommandList command_list;
};

struct __attribute__((__packed__)) GDrawCommandSequence {
  uint8_t version;
  uint8_t reserved;
  GSize size;
  uint16_t play_count;
  uint16_t num_frames;
  GDrawCommandFrame frames[];
};
