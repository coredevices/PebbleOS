/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

/**
 * @defgroup boot_version Bootloader version
 * @ingroup subsys
 * @brief Version of the bootloader that started the firmware.
 * @{
 */

/**
 * @brief Get the bootloader version.
 *
 * PBLBOOT encodes it as major, minor, patch and tweak, one byte each from the most significant
 * one. Older bootloaders report a build timestamp.
 *
 * @return Bootloader version, or 0 if unknown.
 */
uint32_t pbl_boot_version_get(void);

/** @} */
