/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdbool.h>

/**
 * @defgroup btutil_sm_util Pairing information helpers
 * @ingroup btutil
 * @brief Compare and inspect struct pbl_bt_sm_pairing_info.
 * @{
 */

struct pbl_bt_sm_pairing_info;
struct pbl_bt_sm_key;

/**
 * @brief Check whether two pairings have the same remote identity.
 *
 * @param a First pairing.
 * @param b Second pairing.
 * @return true if both have valid identity information with equal identity address and IRK.
 */
bool sm_is_pairing_info_equal_identity(const struct pbl_bt_sm_pairing_info *a,
                                       const struct pbl_bt_sm_pairing_info *b);

/**
 * @brief Check whether a pairing holds no keys.
 *
 * @param p The pairing.
 * @return true if none of the encryption, identity or signing information is valid.
 */
bool sm_is_pairing_info_empty(const struct pbl_bt_sm_pairing_info *p);

/**
 * @brief Check whether an IRK is unused.
 *
 * Per the Bluetooth Core Specification v4.2, Vol 3, Part C, 10.7, an IRK that does not apply is
 * all zeros.
 *
 * @param irk_key The IRK.
 * @return true if @p irk_key is all zeros.
 */
bool sm_is_pairing_info_irk_not_used(const struct pbl_bt_sm_key *irk_key);

/** @} */
