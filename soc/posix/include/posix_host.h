/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

// Host services of a native build. Drivers are split in two: the top half is
// firmware code; the bottom half talks to the host and sees none of the
// firmware headers. Bottoms run on the process main thread through hooks
// (SDL needs that on macOS), and may take command line options.

//! Command line option.
struct posix_host_option {
  //! getopt() letter.
  char flag;
  //! Name of the argument, or NULL for an option without one.
  const char *arg;
  //! Help text.
  const char *help;
  //! Called with the argument (NULL without one) when the option is given.
  void (*set)(const char *value);
};

//! Main thread hook.
struct posix_host_hook {
  //! Before the firmware starts, or NULL.
  void (*init)(void);
  //! On every turn of the main loop: at least every 10 ms, or once woken. NULL for none.
  void (*poll)(void);
  //! When quitting, or NULL.
  void (*exit)(void);
};

void posix_host_option_register(const struct posix_host_option *option);
void posix_host_hook_register(const struct posix_host_hook *hook);

#define POSIX_HOST_CONCAT_(a, b) a##b
#define POSIX_HOST_CONCAT(a, b)  POSIX_HOST_CONCAT_(a, b)

//! Registers a command line option at startup.
#define POSIX_HOST_OPTION(...) POSIX_HOST_OPTION_(__COUNTER__, __VA_ARGS__)
#define POSIX_HOST_OPTION_(_id, ...)                                                              \
  static const struct posix_host_option POSIX_HOST_CONCAT(prv_host_option_, _id) = {__VA_ARGS__}; \
  __attribute__((constructor)) static void POSIX_HOST_CONCAT(prv_host_option_reg_, _id)(void) {   \
    posix_host_option_register(&POSIX_HOST_CONCAT(prv_host_option_, _id));                        \
  }

//! Registers a main thread hook at startup.
#define POSIX_HOST_HOOK(...) POSIX_HOST_HOOK_(__COUNTER__, __VA_ARGS__)
#define POSIX_HOST_HOOK_(_id, ...)                                                            \
  static const struct posix_host_hook POSIX_HOST_CONCAT(prv_host_hook_, _id) = {__VA_ARGS__}; \
  __attribute__((constructor)) static void POSIX_HOST_CONCAT(prv_host_hook_reg_, _id)(void) { \
    posix_host_hook_register(&POSIX_HOST_CONCAT(prv_host_hook_, _id));                        \
  }

//! Runs the main thread hooks soon. Any thread.
void posix_host_wake(void);

//! Quits the process, running the exit hooks. Any thread.
void posix_host_quit(void);

//! Restarts the process, as a reset would the watch.
void posix_host_reboot(void) __attribute__((noreturn));

//! Microseconds on a monotonic clock.
uint64_t posix_host_monotonic_us(void);

//! Directory of the executable.
const char *posix_host_exe_dir(void);

//! Starts the firmware on a thread of its own. Implemented by the firmware.
void posix_fw_start(void);
