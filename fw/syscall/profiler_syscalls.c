/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/syscall.h>
#include <pbl/kernel/time.h>

#include <system/profiler.h>

// ------------------------------------------------------------------------------------
// Find node by ptr
static bool prv_ptr_list_filter(ListNode *list_node, void *data) {
  ProfilerNode *node = (ProfilerNode *)list_node;
  return (node == data);
}

ProfilerNode *prv_find_node(ProfilerNode *find_node) {
  ListNode *node = list_find(g_profiler.nodes, prv_ptr_list_filter, (void *)find_node);

  return (ProfilerNode *)node;
}

PBL_SYSCALL(void, sys_profiler_init) {
#ifdef CONFIG_PROFILER
  profiler_init();
#endif
}

PBL_SYSCALL(void, sys_profiler_start) {
#ifdef CONFIG_PROFILER
  profiler_start();
#endif
}

PBL_SYSCALL(void, sys_profiler_stop) {
#ifdef CONFIG_PROFILER
  profiler_stop();
#endif
}

PBL_SYSCALL(void, sys_profiler_print_stats) {
#ifdef CONFIG_PROFILER
  profiler_print_stats();
#endif
}

PBL_SYSCALL(void, sys_profiler_node_start, (VAL, ProfilerNode *, node)) {
  if (from_user) {
    if (!list_contains(g_profiler.nodes, (ListNode *)node)) {
      // Instead of calling pbl_syscall_fail(), simply return. If PROFILE_INIT has not been
      // executed yet, there won't be any nodes in the list.
      return;
    }
  }

  node->start = pbl_cycle_get_32();
}

PBL_SYSCALL(void, sys_profiler_node_stop, (VAL, ProfilerNode *, node)) {
  // Capture the cycle count as soon as possible, before we validate the node argument
  uint32_t dwt_cyc_cnt = pbl_cycle_get_32();

  if (from_user) {
    if (!list_contains(g_profiler.nodes, (ListNode *)node)) {
      // Instead of calling pbl_syscall_fail(), simply return. If PROFILE_INIT has not been
      // executed yet, there won't be any nodes in the list.
      return;
    }
  }

  profiler_node_stop(node, dwt_cyc_cnt);
}
