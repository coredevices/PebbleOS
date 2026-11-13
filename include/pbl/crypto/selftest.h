/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup crypto_selftest Self-test
 * @ingroup crypto
 * @brief Known-answer tests of the hardware accelerators.
 *
 * Feeds the FIPS 180-4 and FIPS 197 examples through the hardware hooks (see @ref drivers_crypto).
 * An algorithm that fails stays in software from then on. Built with @c CONFIG_CRYPTO_SELFTEST,
 * which also checks each algorithm before its first hardware use.
 * @{
 */

/** @brief Algorithm. */
enum pbl_crypto_alg {
  /** SHA-256. */
  PBL_CRYPTO_ALG_SHA256,
  /** AES, every key size, both ways. */
  PBL_CRYPTO_ALG_AES,
};

/**
 * @brief Check the accelerator of an algorithm.
 *
 * @param alg Algorithm.
 * @retval 0 if the accelerator returned the expected results.
 * @retval -ENODEV if there is no accelerator for @p alg, or it is not ready.
 * @retval -EIO if it returned a wrong result; the algorithm stays in software from then on.
 */
int pbl_crypto_selftest(enum pbl_crypto_alg alg);

/** @} */
