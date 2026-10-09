/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "uart_posix_bottom.h"

#include <errno.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <netinet/in.h>
#include <posix_host.h>
#include <pthread.h>
#include <sys/socket.h>
#include <termios.h>
#include <unistd.h>

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

// On the terminal, Ctrl-C opens the prompt as on the watch, so Ctrl-\ quits.
#define CONSOLE_QUIT 0x1c

#define CONNECT_TIMEOUT_MS 10000
#define CONNECT_RETRY_US   50000
#define RX_HOLD_POLL_US    1000

struct channel {
  int port;
  bool connect;
  const char *device;
  bool flow_control;
  int listen_fd;
  atomic_int client_fd;
};

static struct channel s_channels[UART_POSIX_NUM_CHANNELS] = {
  [UART_POSIX_CONSOLE] = {.listen_fd = -1, .client_fd = -1},
  [UART_POSIX_QEMU] = {.listen_fd = -1, .client_fd = -1},
  [UART_POSIX_BT_HCI] = {.connect = true, .flow_control = true, .listen_fd = -1, .client_fd = -1},
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

static void prv_socket_options(int fd) {
  fcntl(fd, F_SETFD, FD_CLOEXEC);
#ifdef SO_NOSIGPIPE
  setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &(int){1}, sizeof(int));
#endif
}

static void prv_receive(enum uart_posix_channel id, int fd) {
  struct channel *channel = &s_channels[id];
  uint8_t buf[256];
  ssize_t n;
  while ((n = read(fd, buf, sizeof(buf))) > 0) {
    for (ssize_t i = 0; i < n; i++) {
      while (channel->flow_control && !uart_posix_rx_enabled(id)) {
        usleep(RX_HOLD_POLL_US);
      }
      uart_posix_rx(id, buf[i]);
    }
  }
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
    prv_socket_options(fd);
    atomic_store(&channel->client_fd, fd);
    prv_receive(id, fd);
    atomic_store(&channel->client_fd, -1);
    close(fd);
  }
  return NULL;
}

// A connected port or an opened device: when it closes, the UART is cut off.
static void *prv_peer_thread(void *arg) {
  const enum uart_posix_channel id = (enum uart_posix_channel)(intptr_t)arg;
  struct channel *channel = &s_channels[id];
  prv_receive(id, atomic_load(&channel->client_fd));
  fprintf(stderr, "uart%d: peer closed\n", (int)id);
  return NULL;
}

static int prv_tcp_connect(int port) {
  const struct sockaddr_in addr = {
    .sin_family = AF_INET,
    .sin_port = htons((uint16_t)port),
    .sin_addr.s_addr = htonl(INADDR_LOOPBACK),
  };
  for (int waited_ms = 0; waited_ms < CONNECT_TIMEOUT_MS; waited_ms += CONNECT_RETRY_US / 1000) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
      break;
    }
    if (connect(fd, (const struct sockaddr *)&addr, sizeof(addr)) == 0) {
      prv_socket_options(fd);
      return fd;
    }
    close(fd);
    usleep(CONNECT_RETRY_US);
  }
  fprintf(stderr, "cannot connect to port %d\n", port);
  exit(1);
}

static int prv_serial_open(const char *path) {
  int fd = open(path, O_RDWR | O_NOCTTY | O_CLOEXEC);
  struct termios tio;
  if (fd < 0 || tcgetattr(fd, &tio) != 0) {
    fprintf(stderr, "%s: %s\n", path, strerror(errno));
    exit(1);
  }
  cfmakeraw(&tio);
#ifdef B1000000
  cfsetspeed(&tio, B1000000);
#endif
  tcsetattr(fd, TCSANOW, &tio);
  return fd;
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
  struct channel *channel = &s_channels[id];
  if (channel->device != NULL || (channel->connect && channel->port != 0)) {
    const int fd =
        channel->device ? prv_serial_open(channel->device) : prv_tcp_connect(channel->port);
    atomic_store(&channel->client_fd, fd);
    prv_start_thread(prv_peer_thread, (void *)(intptr_t)id);
  } else if (channel->port != 0) {
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
  const int fd = atomic_load(&channel->client_fd);
  if (fd < 0) {
    return;
  }
  if (channel->device != NULL) {
    (void)write(fd, &c, 1);
  } else {
    (void)send(fd, &c, 1, MSG_NOSIGNAL);
  }
}

static void prv_set_console_port(const char *value) {
  s_channels[UART_POSIX_CONSOLE].port = atoi(value);
}

static void prv_set_qemu_port(const char *value) {
  s_channels[UART_POSIX_QEMU].port = atoi(value);
}

static void prv_set_bt_hci(const char *value) {
  if (value[0] == '/') {
    s_channels[UART_POSIX_BT_HCI].device = value;
  } else {
    s_channels[UART_POSIX_BT_HCI].port = atoi(value);
  }
}

POSIX_HOST_OPTION(.flag = 'c', .arg = "port", .help = "serve the console on this TCP port",
                  .set = prv_set_console_port)
POSIX_HOST_OPTION(.flag = 'p', .arg = "port",
                  .help = "serve the QEMU serial protocol on this TCP port",
                  .set = prv_set_qemu_port)
POSIX_HOST_OPTION(.flag = 'b', .arg = "port|device",
                  .help = "connect the Bluetooth HCI UART to this TCP port or serial device",
                  .set = prv_set_bt_hci)
POSIX_HOST_HOOK(.exit = prv_terminal_restore)
