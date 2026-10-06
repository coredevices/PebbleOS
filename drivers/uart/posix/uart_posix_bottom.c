/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#include "posix_host.h"
#include "uart_posix_bottom.h"

// The terminal is the watch's serial console: Ctrl-C opens the prompt, as on
// the watch, so Ctrl-\ quits instead.

#define CONSOLE_QUIT 0x1c

static struct termios s_saved_termios;
static bool s_raw;

static void prv_terminal_restore(void) {
  if (s_raw) {
    tcsetattr(STDIN_FILENO, TCSANOW, &s_saved_termios);
  }
}

static void prv_terminal_restore_on_signal(int sig) {
  prv_terminal_restore();
  raise(sig);
}

static void prv_terminal_raw(void) {
  if (!isatty(STDIN_FILENO) || tcgetattr(STDIN_FILENO, &s_saved_termios) != 0) {
    return;
  }
  atexit(prv_terminal_restore);
  struct sigaction sa = {.sa_handler = prv_terminal_restore_on_signal, .sa_flags = SA_RESETHAND};
  sigaction(SIGABRT, &sa, NULL);
  sigaction(SIGSEGV, &sa, NULL);
  sigaction(SIGBUS, &sa, NULL);

  struct termios raw = s_saved_termios;
  raw.c_lflag &= ~(ICANON | ECHO | ISIG | IEXTEN);
  raw.c_iflag &= ~(IXON | ICRNL);
  raw.c_cc[VMIN] = 1;
  raw.c_cc[VTIME] = 0;
  tcsetattr(STDIN_FILENO, TCSANOW, &raw);
  s_raw = true;
}

static void *prv_console_thread(void *arg) {
  uint8_t c;
  while (read(STDIN_FILENO, &c, 1) == 1) {
    if (c == CONSOLE_QUIT) {
      posix_host_quit();
      continue;
    }
    uart_posix_console_rx(c);
  }
  return NULL;
}

void uart_posix_bottom_console_start(void) {
  prv_terminal_raw();
  pthread_t tid;
  pthread_create(&tid, NULL, prv_console_thread, NULL);
  pthread_detach(tid);
}

POSIX_HOST_HOOK(.exit = prv_terminal_restore)
