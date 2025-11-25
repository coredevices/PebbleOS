/* SPDX-FileCopyrightText: 2014-2015 Samsung Electronics Co., Ltd. */
/* SPDX-FileCopyrightText: 2016 University of Szeged. */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdlib.h>
#include <stdio.h>

#include "jerry-core/jerry-api.h"
#include "jerry_extapi.h"
#include "jerry_run.h"

static const char* fn_sys_loop_name = "sysloop";

int js_entry (const char *source_p, const size_t source_size)
{
  const jerry_char_t *jerry_src = (const jerry_char_t *) source_p;
  jerry_init (JERRY_INIT_EMPTY);
  uint8_t ret_code = 0;

  js_register_functions ();

  jerry_value_t parsed_code = jerry_parse (jerry_src, source_size, false);

  if (!jerry_value_has_error_flag (parsed_code))
  {
    jerry_value_t ret_value = jerry_run (parsed_code);

    if (jerry_value_has_error_flag (ret_value))
      {
      printf ("Error: ret_value has an error flag!\r\n");
      return ret_code = -1;
      }

    jerry_release_value (ret_value);
  }
  else
  {
    printf ("Error: jerry_parse failed!\r\n");
    ret_code = -1;
  }

  jerry_release_value (parsed_code);

  return ret_code;
}

int js_eval (const char *source_p, const size_t source_size)
{
  int status = 0;

  jerry_value_t ret_val = jerry_eval ((jerry_char_t *) source_p,
                                      source_size,
                                      false);

  if (jerry_value_has_error_flag (ret_val))
  {
    printf ("Error: jerry_eval failed!\r\n");
    status = -1;
  }

  jerry_release_value (ret_val);

  return status;
}

int js_loop (uint32_t ticknow)
{
  int status = 0;
  jerry_value_t global_obj = jerry_get_global_object ();
  jerry_value_t sys_name = jerry_create_string ((const jerry_char_t *) fn_sys_loop_name);
  jerry_value_t sysloop_func = jerry_get_property (global_obj, sys_name);
  
  jerry_release_value (sys_name);

  if (jerry_value_has_error_flag (sysloop_func))
  {
    printf ("Error: '%s' not defined!!!\r\n", fn_sys_loop_name);
    jerry_release_value (global_obj);
    jerry_release_value (sysloop_func);
    return -1;
  }

  if (!jerry_value_is_function (sysloop_func))
  {
    printf ("Error: '%s' is not a function!!!\r\n", fn_sys_loop_name);
    jerry_release_value (global_obj);
    jerry_release_value (sysloop_func);
    return -2;
  }

  jerry_value_t val_args[1];
  uint16_t val_argv = 1;

  val_args[0] = jerry_create_number (ticknow);

  jerry_value_t ret_val_sysloop = jerry_call_function (sysloop_func,
                                                       global_obj,
                                                       val_args,
                                                       val_argv);
  if (jerry_value_has_error_flag (ret_val_sysloop))
  {
    status = -3;
  }

  jerry_release_value (global_obj);
  jerry_release_value (ret_val_sysloop);
  jerry_release_value (sysloop_func);
  jerry_release_value (val_args[0]);

  return status;
}

void js_exit (void)
{
  jerry_cleanup ();
}
