/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/** @cond INTERNAL_HIDDEN */
#define PBL_DT_STRINGIFY_(x) #x
#define PBL_DT_STRINGIFY(x)  PBL_DT_STRINGIFY_(x)
/** @endcond */

/**
 * @brief The file with the devicetree instances of a driver, to include at the end of the driver.
 *
 * It is empty for a board without such devices.
 *
 * @param compatible Compatible of the driver, punctuation replaced by underscores.
 */
// clang-format off
#define PBL_DT_INSTANCES(compatible) PBL_DT_STRINGIFY(devicetree/instances/compatible.inc)
// clang-format on
