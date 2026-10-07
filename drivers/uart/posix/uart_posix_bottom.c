/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <termios.h>
#include <unistd.h>

#include "posix_host.h"
#include "uart_posix_bottom.h"

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

// On the terminal, Ctrl-C opens the prompt as on the watch, so Ctrl-\ quits.
#define CONSOLE_QUIT 0x1c

struct channel {
  int port;
  int listen_fd;
  atomic_int client_fd;
};

static struct channel s_channels[UART_POSIX_NUM_CHANNELS] = {
  [UART_POSIX_CONSOLE] = {.listen_fd = -1, .client_fd = -1},
  [UART_POSIX_QEMU] = {.listen_fd = -1, .client_fd = -1},
};

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

static void *prv_terminal_thread(void *arg) {
  uint8_t c;
  while (read(STDIN_FILENO, &c, 1) == 1) {
    if (c == CONSOLE_QUIT) {
      posix_host_quit();
      continue;
    }
    uart_posix_rx(UART_POSIX_CONSOLE, c);
  }
  return NULL;
}

// One client at a time, as QEMU's TCP serial ports.
static void *prv_tcp_thread(void *arg) {
  const enum uart_posix_channel id = (enum uart_posix_channel)(intptr_t)arg;
  struct channel *channel = &s_channels[id];
  for (;;) {
    int fd = accept(channel->listen_fd, NULL, NULL);
    if (fd < 0) {
      continue;
    }
    fcntl(fd, F_SETFD, FD_CLOEXEC);
#ifdef SO_NOSIGPIPE
    setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &(int){1}, sizeof(int));
#endif
    atomic_store(&channel->client_fd, fd);
    uint8_t buf[256];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
      for (ssize_t i = 0; i < n; i++) {
        uart_posix_rx(id, buf[i]);
      }
    }
    atomic_store(&channel->client_fd, -1);
    close(fd);
  }
  return NULL;
}

static void prv_tcp_listen(enum uart_posix_channel id) {
  struct channel *channel = &s_channels[id];
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    perror("socket");
    exit(1);
  }
  fcntl(fd, F_SETFD, FD_CLOEXEC);
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
  struct sockaddr_in addr = {
    .sin_family = AF_INET,
    .sin_port = htons((uint16_t)channel->port),
    .sin_addr.s_addr = htonl(INADDR_LOOPBACK),
  };
  if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0 || listen(fd, 1) != 0) {
    perror("bind");
    exit(1);
  }
  channel->listen_fd = fd;
}

static void prv_start_thread(void *(*fn)(void *), void *arg) {
  pthread_t tid;
  pthread_create(&tid, NULL, fn, arg);
  pthread_detach(tid);
}

void uart_posix_bottom_start(enum uart_posix_channel id) {
  if (s_channels[id].port != 0) {
    prv_tcp_listen(id);
    prv_start_thread(prv_tcp_thread, (void *)(intptr_t)id);
  } else if (id == UART_POSIX_CONSOLE) {
    prv_terminal_raw();
    prv_start_thread(prv_terminal_thread, NULL);
  }
}

void uart_posix_bottom_write(enum uart_posix_channel id, uint8_t c) {
  struct channel *channel = &s_channels[id];
  // The console always reaches stdout, so that a log of the process has it.
  if (id == UART_POSIX_CONSOLE) {
    (void)write(STDOUT_FILENO, &c, 1);
  }
  if (channel->port == 0) {
    return;
  }
  const int fd = atomic_load(&channel->client_fd);
  if (fd >= 0) {
    (void)send(fd, &c, 1, MSG_NOSIGNAL);
  }
}

static void prv_set_console_port(const char *value) {
  s_channels[UART_POSIX_CONSOLE].port = atoi(value);
}

static void prv_set_qemu_port(const char *value) {
  s_channels[UART_POSIX_QEMU].port = atoi(value);
}

POSIX_HOST_OPTION(.flag = 'c', .arg = "port", .help = "serve the console on this TCP port",
                  .set = prv_set_console_port)
POSIX_HOST_OPTION(.flag = 'p', .arg = "port",
                  .help = "serve the QEMU serial protocol on this TCP port",
                  .set = prv_set_qemu_port)
POSIX_HOST_HOOK(.exit = prv_terminal_restore)
