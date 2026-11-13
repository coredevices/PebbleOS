/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/crypto/selftest.h>

/**
 * @brief Whether the accelerator of an algorithm may be used, running its self-test first.
 *
 * @param alg Algorithm.
 * @return False once its self-test failed.
 */
bool pbl_crypto__hw_usable(enum pbl_crypto_alg alg);
