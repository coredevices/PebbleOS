/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <pbl/kernel/sem.h>
#include <pbl/kernel/time.h>
#include <pbl/kernel/work.h>
#include <pbl/shell/backend.h>
#include <pbl/shell/shell.h>
#include <pbl/util/size.h>

static bool s_led;

/* A command with its argument count checked by the shell: a name, and an optional greeting. */
static int prv_cmd_greet(const struct pbl_shell *sh, size_t argc, char **argv) {
  pbl_shell_print(sh, "%s, %s!", argc > 2 ? argv[2] : "Hello", argv[1]);
  return 0;
}

PBL_SHELL_CMD_ARG_REGISTER(greet, nullptr, "Greet someone: greet <name> [greeting]", prv_cmd_greet,
                           2, 1);

/* Subcommands; the parent has no handler, so on its own it prints them. */
static int prv_cmd_led_set(const struct pbl_shell *sh, size_t argc, char **argv) {
  s_led = strcmp(argv[0], "on") == 0;
  pbl_shell_print(sh, "led is %s", s_led ? "on" : "off");
  return 0;
}

static int prv_cmd_led_toggle(const struct pbl_shell *sh, size_t argc, char **argv) {
  s_led = !s_led;
  pbl_shell_print(sh, "led is %s", s_led ? "on" : "off");
  return 0;
}

static const struct pbl_shell_cmd s_led_cmds[] = {
  PBL_SHELL_CMD(off, nullptr, "Turn it off", prv_cmd_led_set),
  PBL_SHELL_CMD(on, nullptr, "Turn it on", prv_cmd_led_set),
  PBL_SHELL_CMD(toggle, nullptr, "Toggle it", prv_cmd_led_toggle),
  PBL_SHELL_SUBCMD_SET_END,
};

PBL_SHELL_CMD_REGISTER(led, s_led_cmds, "A pretend LED", nullptr);

/* A command that finishes later, without blocking the thread commands run on. */
static const struct pbl_shell *s_wait_sh;

static void prv_wait_done(struct pbl_work *work) {
  pbl_shell_print(s_wait_sh, "waited");
  pbl_shell_cmd_done(s_wait_sh, 0);
}

static PBL_DELAYED_WORK_DEFINE(s_wait_work, prv_wait_done);

static int prv_cmd_wait(const struct pbl_shell *sh, size_t argc, char **argv) {
  unsigned long ms;
  int ret;

  ret = pbl_shell_strtoul(argv[1], &ms);
  if (ret < 0) {
    pbl_shell_error(sh, "not a number: %s", argv[1]);
    return ret;
  }

  s_wait_sh = sh;
  pbl_delayed_work_schedule(&pbl_sys_workq, &s_wait_work, PBL_MSEC(ms));

  return -EINPROGRESS;
}

PBL_SHELL_CMD_ARG_REGISTER(wait, nullptr, "Finish after some milliseconds: wait <ms>", prv_cmd_wait,
                           2, 0);

/*
 * A line shell, the kind a transport that carries whole commands implements. Its output goes to
 * the standard output, and the end of each command is signaled with a semaphore.
 */
static PBL_SEM_DEFINE(s_done, 0, 1);
static int s_ret;

static void prv_write(const struct pbl_shell *sh, const char *data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    if (data[i] != '\r') {
      putchar(data[i]);
    }
  }
}

static void prv_done(const struct pbl_shell *sh, int ret) {
  s_ret = ret;
  pbl_sem_give(&s_done);
}

static const struct pbl_shell_backend_api s_api = {
  .write = prv_write,
  .done = prv_done,
};

PBL_SHELL_DEFINE(s_script, nullptr, &s_api, nullptr);

static const char *const s_lines[] = {
  "greet World", "greet Pebble Hi", "greet",    "led",       "led on",
  "led toggle",  "led blink",       "wait 100", "wait soon", "beep",
};

int main(void) {
  for (size_t i = 0; i < PBL_ARRAY_SIZE(s_lines); i++) {
    int ret;

    printf("shell> %s\n", s_lines[i]);
    ret = pbl_shell_execute_line(&s_script, s_lines[i], strlen(s_lines[i]));
    if (ret < 0) {
      fprintf(stderr, "shell: command not submitted (%d)\n", ret);
      return ret;
    }

    (void)pbl_sem_take(&s_done, PBL_FOREVER);
    printf("shell: returned %d\n", s_ret);
  }

  printf("Sample done\n");

  return 0;
}
