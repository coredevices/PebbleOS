/* SPDX-FileCopyrightText: 2014-2015 Samsung Electronics Co., Ltd. */
/* SPDX-FileCopyrightText: 2016 University of Szeged. */
/* SPDX-License-Identifier: Apache-2.0 */

#include "mbed-drivers/mbed.h"

#include "jerry-core/jerry-api.h"
#include "jerry_run.h"

#include "jerry_targetjs.h"

static Serial pc (USBTX, USBRX); //tx, rx

static int jerry_task_init (void)
{
  int retcode;

  DECLARE_JS_CODES;

  /* run main.js */
  retcode = js_entry (js_codes[0].source, js_codes[0].length);
  if (retcode != 0)
  {
    printf ("js_entry failed code(%d) [%s]\r\n", retcode, js_codes[0].name);
    js_exit ();
    return -1;
  }
  /* run rest of the js files */
  for (int src = 1; js_codes[src].source; src++)
  {
    retcode = js_eval (js_codes[src].source, js_codes[src].length);
    if (retcode != 0)
    {
      printf ("js_eval failed code(%d) [%s]\r\n", retcode, js_codes[src].name);
      js_exit ();
      return -2;
    }
  }
  return 0;
}

static void jerry_loop (void)
{
  static uint32_t _jcount = 0;

  js_loop (_jcount++);
}

void app_start (int, char**)
{
  /* set 9600 baud rate for stdout */
  pc.baud (9600);

  printf ("\r\nJerryScript in mbed\r\n");
  printf ("Version: \t%d.%d\n\n", JERRY_API_MAJOR_VERSION, JERRY_API_MINOR_VERSION);
  
  if (jerry_task_init () == 0)
  {
    minar::Scheduler::postCallback(jerry_loop).period(minar::milliseconds(100));
  }
}
