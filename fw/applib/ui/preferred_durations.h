/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

//! @file preferred_durations.h
//! @addtogroup UI
//! @{
//!   @addtogroup Preferences
//!
//! \brief Values recommended by the system
//!
//!   @{

//! Get the recommended amount of milliseconds a result window should be visible before it should
//! automatically close.
//! @note It is the application developer's responsibility to automatically close a result window.
//! @return The recommended result window timeout duration in milliseconds
uint32_t preferred_result_display_duration(void);

//!   @} // end addtogroup Preferences
//! @} // end addtogroup UI
