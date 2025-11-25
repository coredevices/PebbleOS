/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

struct GContext;
typedef struct GContext GContext;

//! @addtogroup Foundation
//! @{
//!   @addtogroup App
//!   @{

//! Gets the graphics context that belongs to the caller of the function.
//! @note Only use the returned GContext inside a drawing callback (`update_proc`)!
//! @return The current graphics context
//! @see \ref Drawing
//! @see \ref LayerUpdateProc

GContext* app_get_current_graphics_context(void);

//!   @} // end addtogroup App
//! @} // end addtogroup Foundation

