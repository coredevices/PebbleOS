/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdbool.h>

struct pbl_bt_sm_pairing_info;
struct pbl_bt_sm_key;

bool sm_is_pairing_info_equal_identity(const struct pbl_bt_sm_pairing_info *a,
                                       const struct pbl_bt_sm_pairing_info *b);

bool sm_is_pairing_info_empty(const struct pbl_bt_sm_pairing_info *p);

bool sm_is_pairing_info_irk_not_used(const struct pbl_bt_sm_key *irk_key);
