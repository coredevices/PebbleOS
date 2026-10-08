/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#if defined(CONFIG_SHELL) && defined(CONFIG_TOUCH)

#include <pbl/shell/shell.h>

#include <applib/ui/recognizer/touch_nav.h>
#include <kernel/ui/modals/modal_manager.h>
#include <pbl/services/touch/touch_nav_service.h>
#include <pbl/util/size.h>

static int prv_cmd_nav_enable(const struct pbl_shell *sh, size_t argc, char **argv) {
  touch_nav_set_enabled(true);
  pbl_shell_print(sh, "touch nav enabled");
  return 0;
}

static int prv_cmd_nav_disable(const struct pbl_shell *sh, size_t argc, char **argv) {
  touch_nav_set_enabled(false);
  pbl_shell_print(sh, "touch nav disabled");
  return 0;
}

static int prv_cmd_nav_log(const struct pbl_shell *sh, size_t argc, char **argv) {
  static const char *const kind_names[] = {"route", "emit", "drop", "gate"};
  const TouchNavState *state = modal_manager_get_touch_nav_state();
  const uint8_t count = state->log_count;

  pbl_shell_print(sh, "started=%u completed=%u failed=%u cancelled=%u dropped=%u gated=%u",
                  state->counters.started, state->counters.completed, state->counters.failed,
                  state->counters.cancelled, state->counters.dropped, state->counters.gated);

  for (uint8_t i = 0; i < count; i++) {
    const uint8_t idx =
        (uint8_t)((state->log_head + TOUCH_NAV_LOG_ENTRIES - count + i) % TOUCH_NAV_LOG_ENTRIES);
    const TouchNavLogEntry *e = &state->log[idx];
    const char *name = (e->kind < ARRAY_LENGTH(kind_names)) ? kind_names[e->kind] : "?";
    pbl_shell_print(sh, "  [%u] %s detail=%u", i, name, e->detail);
  }

  return 0;
}

static const struct pbl_shell_cmd sub_touch_nav[] = {
  PBL_SHELL_CMD(log, NULL, "Show the navigation counters and log", prv_cmd_nav_log),
  PBL_SHELL_CMD(enable, NULL, "Enable touch navigation", prv_cmd_nav_enable),
  PBL_SHELL_CMD(disable, NULL, "Disable touch navigation", prv_cmd_nav_disable),
  PBL_SHELL_SUBCMD_SET_END,
};

static const struct pbl_shell_cmd sub_touch[] = {
  PBL_SHELL_CMD(nav, sub_touch_nav, "Touch navigation", NULL),
  PBL_SHELL_SUBCMD_SET_END,
};

PBL_SHELL_CMD_REGISTER(touch, sub_touch, "Touch", NULL);

#endif
