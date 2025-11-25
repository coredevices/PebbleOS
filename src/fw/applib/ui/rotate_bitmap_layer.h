/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/ui/layer.h"
#include "applib/graphics/gtypes.h"

//! @file rotate_bitmap_layer.h
//! @addtogroup UI
//! @{
//!   @addtogroup Layer Layers
//!   @{
//!     @addtogroup RotBitmapLayer
//! \brief Layer that displays a rotated bitmap image.
//!
//! A RotBitmapLayer is like a \ref BitmapLayer but has the ability to be rotated (by default, around its center). The amount of rotation
//! is specified using \ref rot_bitmap_layer_set_angle() or \ref rot_bitmap_layer_increment_angle(). The rotation argument
//! to those functions is specified as an amount of clockwise rotation, where the value 0x10000 represents a full 360 degree
//! rotation and 0 represent no rotation, and it scales linearly between those values, just like \ref sin_lookup.
//!
//! The center of rotation in the source bitmap is always placed at the center of the RotBitmapLayer and the size of the RotBitmapLayer
//! is automatically calculated so that the entire Bitmap can fit in at all rotation angles.
//!
//! For example, if the image is 10px wide and high, the RotBitmapLayer will be 14px wide ( sqrt(10^2+10^2) ).
//!
//! By default, the center of rotation in the source bitmap is the center of the bitmap but you can call \ref rot_bitmap_set_src_ic() to change the
//! center of rotation.
//!
//! @note RotBitmapLayer has performance limitations that can degrade user
//! experience (see \ref graphics_draw_rotated_bitmap). Use sparingly.
//!     @{

//! The data structure of a RotBitmapLayer, containing a Layer data structure, a pointer to
//! the GBitmap, and all necessary state to draw itself (the clip color, the rotation, center of rotation and
//! the compositing mode).
//! @note a `RotBitmapLayer *` can safely be casted to a `Layer *` and can thus be used
//! with all other functions that take a `Layer *` as an argument.
//! <br/>For example, the following is legal:
//! \code{.c}
//! RotBitmapLayer bitmap_layer;
//! ...
//! layer_set_hidden((Layer *)&bitmap_layer, true);
//! \endcode

typedef struct {
  Layer layer;
  GBitmap *bitmap;

  //! the color to use in the regions covered by the dest rect, but not by the rotated src rect
  GColor8 corner_clip_color;

  int32_t rotation; //! angle to rotate this by when drawing
  GPoint src_ic; //! the instance center (pivot) of the src bitmap, relative to the src bitmap bounds
  GPoint dest_ic; //! the instance center (pivot) of the dest bitmap, kept in the center of the frame
  // TODO: better name than "instance center"???

  GCompOp compositing_mode;
} RotBitmapLayer;

/**
initializes the bitmap to render with clear background and corner clip,
and as a square layer with dimensions of the diagonal for the bitmap,
with the IC's situated in the center of the bitmap and layer, respectively
**/
void rot_bitmap_layer_init(RotBitmapLayer *image, GBitmap *bitmap);

//! Creates a new RotBitmapLayer on the heap and initializes it with the default values:
//!  * Angle: 0
//!  * Compositing mode: \ref GCompOpAssign
//!  * Corner clip color: \ref GColorClear
//!
//! @param bitmap The bitmap to display in this RotBitmapLayer
//! @return A pointer to the RotBitmapLayer. `NULL` if the RotBitmapLayer could not
//! be created
RotBitmapLayer* rot_bitmap_layer_create(GBitmap *bitmap);

void rot_bitmap_layer_deinit(RotBitmapLayer *bitmap);

//! Destroys a RotBitmapLayer and frees all associated memory.
//! @note It is the developer responsibility to free the \ref GBitmap.
//! @param bitmap The RotBitmapLayer to destroy.
void rot_bitmap_layer_destroy(RotBitmapLayer *bitmap);

//! Defines what color to use in areas that are not covered by the source bitmap.
//! By default this is \ref GColorClear.
//! @param bitmap The RotBitmapLayer on which to change the corner clip color
//! @param color The corner clip color
void rot_bitmap_layer_set_corner_clip_color(RotBitmapLayer *bitmap, GColor color);
void rot_bitmap_layer_set_corner_clip_color_2bit(RotBitmapLayer *bitmap, GColor2 color);

//! Sets the rotation angle of this RotBitmapLayer
//! @param bitmap The RotBitmapLayer on which to change the rotation
//! @param angle Rotation is an integer between 0 (no rotation) and 0x10000 (360 degree rotation). @see sin_lookup()
void rot_bitmap_layer_set_angle(RotBitmapLayer *bitmap, int32_t angle);

//! Change the rotation angle of this RotBitmapLayer
//! @param bitmap The RotBitmapLayer on which to change the rotation
//! @param angle_change The rotation angle change
void rot_bitmap_layer_increment_angle(RotBitmapLayer *bitmap, int32_t angle_change);

//! Defines the only point that will not be affected by the rotation in the source bitmap.
//!
//! For example, if you pass GPoint(0, 0), the image will rotate around the top-left corner.
//!
//! This point is always projected at the center of the RotBitmapLayer. Calling this function
//! automatically adjusts the width and height of the RotBitmapLayer so that
//! the entire bitmap can fit inside the layer at all rotation angles.
//!
//! @param bitmap The RotBitmapLayer on which to change the rotation
//! @param ic The only point in the original image that will not be affected by the rotation.
void rot_bitmap_set_src_ic(RotBitmapLayer *bitmap, GPoint ic);

//! Sets the compositing mode of how the bitmap image is composited onto what has been drawn beneath the
//! RotBitmapLayer.
//! By default this is \ref GCompOpAssign.
//! The RotBitmapLayer is automatically marked dirty after this operation.
//! @param bitmap The RotBitmapLayer on which to change the rotation
//! @param mode The compositing mode to set
//! @see \ref GCompOp for visual examples of the different compositing modes.
void rot_bitmap_set_compositing_mode(RotBitmapLayer *bitmap, GCompOp mode);

//!     @} // end addtogroup RotBitmapLayer
//!   @} // end addtogroup Layer
//! @} // end addtogroup UI
