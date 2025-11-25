/* SPDX-FileCopyrightText: 2016 Linaro */
/* SPDX-License-Identifier: Apache-2.0 */

#include <zephyr.h>
#include <uart.h>
#include <drivers/console/uart_console.h>
#include "getline-zephyr.h"

/* While app processes one input line, Zephyr will have another line
   buffer to accumulate more console input. */
static struct uart_console_input line_bufs[2];

static struct nano_fifo free_queue;
static struct nano_fifo used_queue;

char *zephyr_getline(void)
{
  static struct uart_console_input *cmd;

  /* Recycle cmd buffer returned previous time */
  if (cmd != NULL)
  {
    nano_fifo_put(&free_queue, cmd);
  }

  cmd = nano_fifo_get(&used_queue, TICKS_UNLIMITED);
  return cmd->line;
}

void zephyr_getline_init(void)
{
  int i;

  nano_fifo_init(&used_queue);
  nano_fifo_init(&free_queue);
  for (i = 0; i < sizeof(line_bufs) / sizeof(*line_bufs); i++)
  {
    nano_fifo_put(&free_queue, &line_bufs[i]);
  }

  /* Zephyr UART handler takes an empty buffer from free_queue,
     stores UART input in it until EOL, and then puts it into
     used_queue. */
  uart_register_input(&free_queue, &used_queue, NULL);
}
