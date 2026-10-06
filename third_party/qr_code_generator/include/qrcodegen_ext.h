/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>

#include <qrcodegen.h>

int qrcodegen_getMinFitVersion(enum qrcodegen_Ecc ecl, size_t dataLen);
