/* SPDX-FileCopyrightText: 2016 Samsung Electronics Co., Ltd. */
/* SPDX-FileCopyrightText: 2016 University of Szeged. */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>
#include "shell.h"
#include "jerry-api.h"

/**
 * Jerryscript simple test
 */
int test_jerry (int argc, char **argv)
{
  /* Suppress compiler errors */
  (void) argc;
  (void) argv;
  const jerry_char_t script[] = "print ('Hello, World!');";
  printf ("This test run the following script code: [%s]\n\n", script);

  size_t script_size = strlen ((const char *) script);
  bool ret_value = jerry_run_simple (script, script_size, JERRY_INIT_EMPTY);

  return (ret_value ? 1 : 0);
} /* test_jerry */

const shell_command_t shell_commands[] = {
  { "test", "Jerryscript Hello World test", test_jerry },
  { NULL, NULL, NULL }
};

int main (void)
{
  printf ("You are running RIOT on a(n) %s board.\n", RIOT_BOARD);
  printf ("This board features a(n) %s MCU.\n", RIOT_MCU);

  /* start the shell */
  char line_buf[SHELL_DEFAULT_BUFSIZE];
  shell_run (shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

  return 0;
}
