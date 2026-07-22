/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "syscall/syscall.h"
#include "syscall/syscall_internal.h"

#ifdef CONFIG_TOUCH_CLICK_SYNTHESIS

#include "services/touch/touch_click_synth.h"

// Publish the focused window's action bar descriptor from applib (app task) to
// the KernelMain synthesis bridge, so a tap on the bar can be routed to
// UP/SELECT/DOWN by its vertical zone.
DEFINE_SYSCALL(void, sys_touch_click_synth_set_action_bar,
               const ActionBarSynthDescriptor *descriptor) {
  if (PRIVILEGE_WAS_ELEVATED) {
    syscall_assert_userspace_buffer(descriptor, sizeof(*descriptor));
  }
  ActionBarSynthDescriptor local = *descriptor;
  touch_click_synth_set_action_bar(&local);
}

#endif  // CONFIG_TOUCH_CLICK_SYNTHESIS
