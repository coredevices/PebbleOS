/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "kernel/system_version_capabilities.h"
#include "pbl/services/comm_session/session.h"

void test_system_version_capabilities__advertises_music_output_routing(void) {
  const PebbleProtocolCapabilities capabilities = system_version_get_capabilities();

  cl_assert_equal_b(capabilities.music_output_routing_support, true);
  cl_assert(capabilities.flags & CommSessionMusicOutputRoutingSupport);
}
