/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stddef.h>
#include <stdlib.h>

typedef void (*pbl_init_fn)(void);

struct exit_handler {
  struct exit_handler *next;
  void (*fn)(void *arg);
  void *arg;
};

#pragma GCC visibility push(hidden)
extern pbl_init_fn __preinit_array_start[];
extern pbl_init_fn __preinit_array_end[];
extern pbl_init_fn __init_array_start[];
extern pbl_init_fn __init_array_end[];
extern pbl_init_fn __fini_array_start[];
extern pbl_init_fn __fini_array_end[];
#pragma GCC visibility pop

int main(void);

void *__dso_handle;

static struct exit_handler *s_handlers;
static void (*s_atexit_run)(void);

static void prv_atexit_run(void) {
  while (s_handlers != NULL) {
    struct exit_handler *handler = s_handlers;

    s_handlers = handler->next;
    handler->fn(handler->arg);
    free(handler);
  }
}

int __cxa_atexit(void (*fn)(void *arg), void *arg, void *dso_handle) {
  struct exit_handler *handler = malloc(sizeof(*handler));

  if (handler == NULL) {
    return -1;
  }

  handler->next = s_handlers;
  handler->fn = fn;
  handler->arg = arg;
  s_handlers = handler;
  s_atexit_run = prv_atexit_run;

  return 0;
}

int __aeabi_atexit(void *arg, void (*fn)(void *arg), void *dso_handle) {
  return __cxa_atexit(fn, arg, dso_handle);
}

int atexit(void (*fn)(void)) {
  return __cxa_atexit((void (*)(void *))fn, NULL, NULL);
}

int pbl_process_entry(void) {
  for (pbl_init_fn *fn = __preinit_array_start; fn < __preinit_array_end; fn++) {
    (*fn)();
  }

  for (pbl_init_fn *fn = __init_array_start; fn < __init_array_end; fn++) {
    (*fn)();
  }

  int ret = main();

  if (s_atexit_run != NULL) {
    s_atexit_run();
  }

  for (pbl_init_fn *fn = __fini_array_end; fn > __fini_array_start;) {
    (*--fn)();
  }

  return ret;
}
