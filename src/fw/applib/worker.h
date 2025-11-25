/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

//! @file worker.h
//!
//! @addtogroup Foundation
//! @{
//!   @addtogroup Worker
//!   @{

//! The event loop for workers, to be used in worker's main(). Will block until the worker is ready to exit.
//! @see \ref App
void worker_event_loop(void);

//! Launch the foreground app for this worker
void worker_launch_app(void);

//!   @} // end addtogroup Worker
//! @} // end addtogroup Foundation

