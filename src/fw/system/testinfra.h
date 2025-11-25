/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <util/attributes.h>

// The automated testing framework shouldn't start operating on the system
// after a reset until PebbleOS is ready to handle requests. This function
// handles that notification
void notify_system_ready_for_communication(void);

#if IS_BIGBOARD
// This sends a notification to infra that we have detected an issue which needs manual
// intervention to debug. Infra should disable the board to give the team time to grab the board and
// investigate.
//
// Note: To preserve the current state, this routine sets the FORCE_PRF boot bit & then
// forces a coredump
NORETURN test_infra_quarantine_board(const char *quarantine_reason);
#endif /* IS_BIGBOARD */
