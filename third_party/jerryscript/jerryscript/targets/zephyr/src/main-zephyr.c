/* SPDX-FileCopyrightText: 2016 Intel Corporation */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <zephyr.h>
#include <misc/printk.h>
#include <misc/shell.h>
#include "getline-zephyr.h"

#include "jerry-api.h"

static jerry_value_t print_function;

static int shell_cmd_handler (char *source_buffer)
{
  jerry_value_t ret_val;

  ret_val = jerry_eval ((jerry_char_t *) source_buffer,
    strlen (source_buffer),
    false);

  if (jerry_value_has_error_flag (ret_val))
  {
    /* User-friendly error messages require at least "cp" JerryScript
       profile. Include a message prefix in case "cp_minimal" profile
       is used. */
    printf ("Error executing statement: ");
    /* Clear error flag, otherwise print call below won't produce any
       output. */
    jerry_value_clear_error_flag (&ret_val);
  }

  if (!jerry_value_has_error_flag (print_function))
  {
    jerry_value_t ret_val_print = jerry_call_function (print_function,
      jerry_create_undefined (),
      &ret_val,
      1);
    jerry_release_value (ret_val_print);
  }

  jerry_release_value (ret_val);

  return 0;
} /* shell_cmd_handler */

void main (void)
{
  uint32_t zephyr_ver = sys_kernel_version_get ();
  printf ("JerryScript build: " __DATE__ " " __TIME__ "\n");
  printf ("JerryScript API %d.%d\n", JERRY_API_MAJOR_VERSION, JERRY_API_MINOR_VERSION);
  printf ("Zephyr version %d.%d.%d\n", (int)SYS_KERNEL_VER_MAJOR (zephyr_ver),
    (int)SYS_KERNEL_VER_MINOR (zephyr_ver),
    (int)SYS_KERNEL_VER_PATCHLEVEL (zephyr_ver));

  zephyr_getline_init ();
  jerry_init (JERRY_INIT_EMPTY);
  jerry_value_t global_obj_val = jerry_get_global_object ();

  jerry_value_t print_func_name_val = jerry_create_string ((jerry_char_t *) "print");
  print_function = jerry_get_property (global_obj_val, print_func_name_val);
  jerry_release_value (print_func_name_val);
  jerry_release_value (global_obj_val);
  if (jerry_value_has_error_flag (print_function))
  {
    printf ("Error: could not look up print function, expression results won't be printed\n");
  }

  while (1)
  {
    char *s;
    printf("js> ");
    fflush(stdout);
    s = zephyr_getline ();
    if (*s)
    {
      shell_cmd_handler (s);
    }
  }

  /* As we never retturn from REPL above, don't call jerry_cleanup() here. */
} /* main */
