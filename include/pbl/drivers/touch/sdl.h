/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/touch.h>

/**
 * @defgroup drivers_touch_sdl SDL
 * @ingroup drivers_touch
 * @brief Touchscreen of a native build, driven by the mouse through SDL.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_touch pbl_touch_sdl;
/** @endcond */

/** @brief The SDL touchscreen. */
#define SDL_TOUCH (&pbl_touch_sdl)

/** @} */
