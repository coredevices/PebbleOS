/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <getopt.h>
#include <libgen.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include <posix_host.h>

#define MAX_OPTIONS 16
#define MAX_HOOKS   16
#define POLL_MS     10

static const struct posix_host_option *s_options[MAX_OPTIONS];
static size_t s_num_options;
static const struct posix_host_hook *s_hooks[MAX_HOOKS];
static size_t s_num_hooks;

static pthread_mutex_t s_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t s_cond = PTHREAD_COND_INITIALIZER;
static bool s_woken;
static bool s_quit;
static bool s_reboot;

static char **s_argv;
static char s_exe_dir[PATH_MAX];
static int s_exit_after_s;

void posix_host_option_register(const struct posix_host_option *option) {
  if (s_num_options < MAX_OPTIONS) {
    s_options[s_num_options++] = option;
  }
}

void posix_host_hook_register(const struct posix_host_hook *hook) {
  if (s_num_hooks < MAX_HOOKS) {
    s_hooks[s_num_hooks++] = hook;
  }
}

void posix_host_wake(void) {
  pthread_mutex_lock(&s_lock);
  s_woken = true;
  pthread_cond_signal(&s_cond);
  pthread_mutex_unlock(&s_lock);
}

void posix_host_quit(void) {
  pthread_mutex_lock(&s_lock);
  s_quit = true;
  pthread_cond_signal(&s_cond);
  pthread_mutex_unlock(&s_lock);
}

// The main thread restarts the process, once the exit hooks ran there.
void posix_host_reboot(void) {
  pthread_mutex_lock(&s_lock);
  s_reboot = true;
  s_quit = true;
  pthread_cond_signal(&s_cond);
  pthread_mutex_unlock(&s_lock);
  for (;;) {
    pause();
  }
}

uint64_t posix_host_monotonic_us(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000 + (uint64_t)ts.tv_nsec / 1000;
}

const char *posix_host_exe_dir(void) {
  return s_exe_dir;
}

static void prv_set_exit_after(const char *value) {
  s_exit_after_s = atoi(value);
}

POSIX_HOST_OPTION(.flag = 't', .arg = "seconds", .help = "quit after this long",
                  .set = prv_set_exit_after)

static void prv_usage(const char *argv0) {
  fprintf(stderr, "usage: %s [options]\n", argv0);
  for (size_t i = 0; i < s_num_options; i++) {
    const struct posix_host_option *o = s_options[i];
    fprintf(stderr, "  -%c %-10s %s\n", o->flag, o->arg ? o->arg : "", o->help);
  }
}

static void prv_parse_options(int argc, char **argv) {
  char optstring[2 * MAX_OPTIONS + 2] = "h";
  for (size_t i = 0; i < s_num_options; i++) {
    size_t len = strlen(optstring);
    optstring[len++] = s_options[i]->flag;
    if (s_options[i]->arg != NULL) {
      optstring[len++] = ':';
    }
    optstring[len] = '\0';
  }

  int opt;
  while ((opt = getopt(argc, argv, optstring)) != -1) {
    const struct posix_host_option *option = NULL;
    for (size_t i = 0; i < s_num_options; i++) {
      if (s_options[i]->flag == opt) {
        option = s_options[i];
      }
    }
    if (option == NULL) {
      prv_usage(argv[0]);
      exit(opt == 'h' ? 0 : 1);
    }
    option->set(optarg);
  }
}

// Waits up to POLL_MS for a wake-up; returns whether to quit.
static bool prv_wait(void) {
  struct timeval now;
  gettimeofday(&now, NULL);
  uint64_t ns = (uint64_t)now.tv_usec * 1000 + (uint64_t)POLL_MS * 1000000;
  struct timespec deadline = {
    .tv_sec = now.tv_sec + (time_t)(ns / 1000000000),
    .tv_nsec = (long)(ns % 1000000000),
  };

  pthread_mutex_lock(&s_lock);
  while (!s_woken && !s_quit) {
    if (pthread_cond_timedwait(&s_cond, &s_lock, &deadline) != 0) {
      break;
    }
  }
  s_woken = false;
  bool quit = s_quit;
  pthread_mutex_unlock(&s_lock);
  return quit;
}

int main(int argc, char **argv) {
  s_argv = argv;
  char exe[PATH_MAX];
  if (realpath(argv[0], exe) != NULL) {
    snprintf(s_exe_dir, sizeof(s_exe_dir), "%s", dirname(exe));
  }

  prv_parse_options(argc, argv);

  for (size_t i = 0; i < s_num_hooks; i++) {
    if (s_hooks[i]->init != NULL) {
      s_hooks[i]->init();
    }
  }

  posix_fw_start();

  const uint64_t exit_at_us = posix_host_monotonic_us() + (uint64_t)s_exit_after_s * 1000000;
  do {
    if (s_exit_after_s > 0 && posix_host_monotonic_us() >= exit_at_us) {
      break;
    }
    for (size_t i = 0; i < s_num_hooks; i++) {
      if (s_hooks[i]->poll != NULL) {
        s_hooks[i]->poll();
      }
    }
  } while (!prv_wait());

  for (size_t i = 0; i < s_num_hooks; i++) {
    if (s_hooks[i]->exit != NULL) {
      s_hooks[i]->exit();
    }
  }

  if (s_reboot) {
    fprintf(stderr, "\n*** reboot ***\n\n");
    execv(s_argv[0], s_argv);
    perror("execv");
    exit(1);
  }
  exit(0);
}
