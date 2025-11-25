/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdbool.h>

typedef struct SMPairingInfo SMPairingInfo;
typedef struct SM128BitKey SM128BitKey;

bool sm_is_pairing_info_equal_identity(const SMPairingInfo *a, const SMPairingInfo *b);

bool sm_is_pairing_info_empty(const SMPairingInfo *p);

bool sm_is_pairing_info_irk_not_used(const SM128BitKey *irk_key);
