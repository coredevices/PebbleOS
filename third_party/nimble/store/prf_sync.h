/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

/*
 * Keeps the PRF store, which holds one bonding, in line with the config store: the bonding
 * written last is the one it holds.
 */

/**
 * Take the bonding of the PRF store, if it has one, into the config store.
 *
 * @return true if that changed the config store.
 */
bool nimble_store__prf_sync_load(void);

/**
 * Put the bonding written last to the config store in the PRF store, or erase the one there
 * once it is gone. Call it each time the keys of the config store change.
 *
 * @return 0 on success, a code of the host otherwise.
 */
int nimble_store__prf_sync_save(void);
