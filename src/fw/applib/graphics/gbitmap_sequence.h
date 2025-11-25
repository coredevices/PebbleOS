/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "upng.h"
#include "gtypes.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

typedef struct GBitmapSequencePNGDecoderData {
  upng_t *upng;
  size_t read_cursor; // relative to file start, advanced to the control chunk of the next frame
  GColor8 *palette;   // required for palettized images (rgba)
  uint8_t palette_entries;
  apng_dispose_ops last_dispose_op;
  uint32_t previous_xoffset;
  uint32_t previous_yoffset;
  uint32_t previous_width;
  uint32_t previous_height;
} GBitmapSequencePNGDecoderData;

typedef struct {
  uint32_t resource_id;
  union {
    uint32_t flags;
    struct {
      bool header_loaded : 1;
      bool data_is_loaded_from_flash : 1;
    };
  };
  GSize bitmap_size; // Width & Height
  uint32_t play_count;  // Total number of times to play the sequence
  uint32_t play_index;  // Current number of times sequence was played
  uint32_t total_duration_ms; // Duration of the animation in ms
  uint32_t total_frames;  // Total number of frames for the sequence
  uint32_t current_frame;  // Current frame in the sequence
  uint32_t current_frame_delay_ms;  // Amount of time to display the current frame
  uint32_t elapsed_ms;  // Total elapsed time for the sequence

  // Stores internal decoder data
  union {
    GBitmapSequencePNGDecoderData png_decoder_data;
    // potential decoder data for future formats
  };
} GBitmapSequence;

//! Creates a GBitmapSequence from the specified resource (APNG/PNG files)
//! @param resource_id Resource to load and create GBitmapSequence from.
//! @return GBitmapSequence pointer if the resource was loaded, NULL otherwise
GBitmapSequence *gbitmap_sequence_create_with_resource(uint32_t resource_id);

//! @internal
GBitmapSequence *gbitmap_sequence_create_with_resource_system(ResAppNum app_num,
                                                              uint32_t resource_id);

//! Deletes the GBitmapSequence structure and frees any allocated memory/decoder_data
//! @param bitmap_sequence Pointer to the bitmap sequence to free (delete)
void gbitmap_sequence_destroy(GBitmapSequence *bitmap_sequence);

//! Restarts the GBitmapSequence to the first frame \ref gbitmap_sequence_update_bitmap_next_frame
//! @param bitmap_sequence Pointer to loaded bitmap sequence
//! @return True if sequence was restarted, false otherwise
bool gbitmap_sequence_restart(GBitmapSequence *bitmap_sequence);

//! Updates the contents of the bitmap sequence to the next frame
//! and optionally returns the delay in milliseconds until the next frame.
//! @param bitmap_sequence Pointer to loaded bitmap sequence
//! @param bitmap Pointer to the initialized GBitmap in which to render the bitmap sequence
//! @param[out] delay_ms If not NULL, returns the delay in milliseconds until the next frame.
//! @return True if frame was rendered.  False if all frames (and loops) have been rendered
//! for the sequence.  Will also return false if frame could not be rendered
//! (includes out of memory errors).
//! @note GBitmap must be large enough to accommodate the bitmap_sequence image
//! \ref gbitmap_sequence_get_bitmap_size
bool gbitmap_sequence_update_bitmap_next_frame(GBitmapSequence *bitmap_sequence,
    GBitmap *bitmap, uint32_t *delay_ms);

//! Updates the contents of the bitmap sequence to the frame at elapsed in the sequence.
//! For looping animations this accounts for the loop, for example an animation of 1 second that
//! is configured to loop 2 times updated to 1500 ms elapsed time will display the sequence
//! frame at 500 ms.  Elapsed time is the time from the start of the animation, and will
//! be ignored if it is for a time earlier than the last rendered frame.
//! @param bitmap_sequence Pointer to loaded bitmap sequence
//! @param bitmap Pointer to the initialized GBitmap in which to render the bitmap sequence
//! @param elapsed_ms Elapsed time in milliseconds in the sequence relative to start
//! @return True if a frame was rendered.  False if all frames (and loops) have already
//! been rendered for the sequence.  Will also return false if frame could not be rendered
//! (includes out of memory errors).
//! @note GBitmap must be large enough to accommodate the bitmap_sequence image
//! \ref gbitmap_sequence_get_bitmap_size
//! @note This function is disabled for play_count 0
bool gbitmap_sequence_update_bitmap_by_elapsed(GBitmapSequence *bitmap_sequence,
    GBitmap *bitmap, uint32_t elapsed_ms);

//! This function gets the current frame number for the bitmap sequence
//! @param bitmap_sequence Pointer to loaded bitmap sequence
//! @return index of current frame in the current loop of the bitmap sequence
int32_t gbitmap_sequence_get_current_frame_idx(GBitmapSequence *bitmap_sequence);

//! This function gets the current frame's delay in milliseconds
//! @param bitmap_sequence Pointer to loaded bitmap sequence
//! @return delay for current frame to be shown in milliseconds
uint32_t gbitmap_sequence_get_current_frame_delay_ms(GBitmapSequence *bitmap_sequence);

//! This function sets the total number of frames for the bitmap sequence
//! @param bitmap_sequence Pointer to loaded bitmap sequence
//! @return number of frames contained in a single loop of the bitmap sequence
uint32_t gbitmap_sequence_get_total_num_frames(GBitmapSequence *bitmap_sequence);

//! This function gets the play count (number of times to repeat) the bitmap sequence
//! @note This value is initialized by the bitmap sequence data, and is modified by
//! \ref gbitmap_sequence_set_play_count
//! @param bitmap_sequence Pointer to loaded bitmap sequence
//! @return Play count of bitmap sequence, PLAY_COUNT_INFINITE for infinite looping
uint32_t gbitmap_sequence_get_play_count(GBitmapSequence *bitmap_sequence);

//! This function sets the play count (number of times to repeat) the bitmap sequence
//! @param bitmap_sequence Pointer to loaded bitmap sequence
//! @param play_count Number of times to repeat the bitmap sequence
//! with 0 disabling update_by_elapsed and update_next_frame, and
//! PLAY_COUNT_INFINITE for infinite looping of the animation
void gbitmap_sequence_set_play_count(GBitmapSequence *bitmap_sequence, uint32_t play_count);

//! This function gets the minimum required size (dimensions) necessary
//! to render the bitmap sequence to a GBitmap
//! using the /ref gbitmap_sequence_update_bitmap_next_frame
//! @param bitmap_sequence Pointer to loaded bitmap sequence
//! @return Dimensions required to render the bitmap sequence to a GBitmap
GSize gbitmap_sequence_get_bitmap_size(GBitmapSequence *bitmap_sequence);

//! @internal
//! This function gets the total duration in milliseconds of the \ref GBitmapSequence. This does
//! not include the play count, it only refers to the duration of playing one sequence.
//! @param bitmap_sequence Pointer to loaded bitmap sequence
//! @return The total duration in milliseconds of the \ref GBitmapSequence
uint32_t gbitmap_sequence_get_total_duration(GBitmapSequence *bitmap_sequence);
