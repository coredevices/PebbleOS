/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "syscall_fuzz_app.h"

#include <setjmp.h>
#include <string.h>

#include <pbl/kernel/syscall_desc.h>
#include <pbl/logging/logging.h>
#include <pbl/util/math.h>
#include <pbl/util/size.h>

#include <applib/app.h>
#include <applib/app_timer.h>
#include <applib/ui/app_window_stack.h>
#include <applib/ui/window.h>
#include <kernel/pbl_malloc.h>
#include <process_state/app_state/app_state.h>
#include <syscall/syscall_fuzz.h>

// Calls every PBL_SYSCALL unprivileged: once with plain arguments, then once
// per argument and bad value. Pointer arguments that point outside the app's
// memory must be rejected; nothing may change the kernel canary or bring the
// kernel down. Results are logged, one line per syscall, for the integration
// test to check.

#define MAX_ARGS  10
#define SLOT_SIZE 128
#define BUF_LEN   16

enum rule_kind {
  // Not called: the reason is logged.
  RULE_SKIP,
  // An app callback, called later from the app's event loop: always a valid one.
  RULE_CALLBACK,
  // Values above max would only stall the app.
  RULE_MAX,
};

struct rule {
  const char *syscall;
  enum rule_kind kind;
  uint8_t arg;
  uint32_t max;
  const char *reason;
};

static const struct rule s_rules[] = {
  {"sys_syscall_fuzz_arm", RULE_SKIP, .reason = "controls the fuzzer"},
  {"sys_syscall_fuzz_get_targets", RULE_SKIP, .reason = "controls the fuzzer"},
  {"sys_syscall_fuzz_check_canary", RULE_SKIP, .reason = "controls the fuzzer"},
  {"app_timer_register", RULE_CALLBACK, 1},
  {"app_timer_register_repeatable", RULE_CALLBACK, 1},
  {"sys_accel_manager_data_subscribe", RULE_CALLBACK, 1},
  {"sys_app_inbox_service_register", RULE_CALLBACK, 2},
  {"sys_app_inbox_service_register", RULE_CALLBACK, 3},
  {"sys_app_outbox_send", RULE_CALLBACK, 2},
  {"sys_current_process_schedule_callback", RULE_CALLBACK, 0},
  {"sys_psleep", RULE_MAX, 0, 10},
};

struct fuzz {
  Window window;
  const struct pbl_syscall_desc *desc;
  struct syscall_fuzz_targets targets;
  uintptr_t pointers[12];
  size_t num_pointers;
  alignas(8) uint8_t values[MAX_ARGS][PBL_SYSCALL_DESC_ARG_MAX];
  const void *args[MAX_ARGS];
  uint8_t *slots;
  jmp_buf rejected;
  bool calling;
  uint32_t syscalls;
  uint32_t failures;
};

static const uint64_t s_ints[] = {
  0,
  1,
  0x7f,
  0xff,
  0x7fff,
  0xffff,
  0x7fffffff,
  0x80000000,
  0xffffffff,
  0x7fffffffffffffff,
  0x8000000000000000,
  0xffffffffffffffff,
};

static void prv_noop(void) {
}

static bool prv_is_pointer_kind(uint8_t kind) {
  return kind != PBL_SYSCALL_ARG_VAL;
}

static bool prv_is_optional(uint8_t kind) {
  return kind == PBL_SYSCALL_ARG_IN_OPT || kind == PBL_SYSCALL_ARG_OUT_OPT ||
         kind == PBL_SYSCALL_ARG_INOUT_OPT;
}

static const struct rule *prv_rule(const struct pbl_syscall_desc *desc, enum rule_kind kind,
                                   int arg) {
  for (size_t i = 0; i < ARRAY_LENGTH(s_rules); i++) {
    const struct rule *rule = &s_rules[i];
    if (rule->kind == kind && (arg < 0 || rule->arg == arg) &&
        strcmp(rule->syscall, desc->name) == 0) {
      return rule;
    }
  }
  return nullptr;
}

//! The argument holding arg's length or count, or -1.
static int prv_length_arg(const struct pbl_syscall_desc *desc, int arg) {
  for (int i = 0; i < desc->num_args; i++) {
    if (i != arg && strcmp(desc->args[i].name, desc->args[arg].extra) == 0) {
      return i;
    }
  }
  return -1;
}

//! The pointer argument whose length or count is arg, or -1.
static int prv_buffer_arg(const struct pbl_syscall_desc *desc, int arg) {
  for (int i = 0; i < desc->num_args; i++) {
    const uint8_t kind = desc->args[i].kind;
    const bool sized = kind == PBL_SYSCALL_ARG_IN_BUF || kind == PBL_SYSCALL_ARG_OUT_BUF ||
                       kind == PBL_SYSCALL_ARG_IN_ARRAY || kind == PBL_SYSCALL_ARG_OUT_ARRAY;
    if (sized && prv_length_arg(desc, i) == arg) {
      return i;
    }
  }
  return -1;
}

//! Store value as argument i, in its type's size; a structure gets it repeated.
static void prv_set_arg(struct fuzz *fuzz, int i, uint64_t value) {
  uint8_t *dst = fuzz->values[i];
  const size_t size = fuzz->desc->args[i].size;
  memset(dst, 0, PBL_SYSCALL_DESC_ARG_MAX);
  switch (size) {
    case sizeof(uint8_t):
      *dst = (uint8_t)value;
      break;
    case sizeof(uint16_t):
      memcpy(dst, &(uint16_t){(uint16_t)value}, size);
      break;
    case sizeof(uint32_t):
      memcpy(dst, &(uint32_t){(uint32_t)value}, size);
      break;
    case sizeof(uint64_t):
      memcpy(dst, &value, size);
      break;
    default:
      for (size_t off = 0; off < size; off += sizeof(uint32_t)) {
        const uint32_t word = (uint32_t)value;
        memcpy(dst + off, &word, MIN(sizeof(word), size - off));
      }
      break;
  }
}

static void prv_set_baseline(struct fuzz *fuzz) {
  const struct pbl_syscall_desc *desc = fuzz->desc;
  memset(fuzz->slots, 0, SLOT_SIZE * MAX_ARGS);
  for (int i = 0; i < desc->num_args; i++) {
    prv_set_arg(fuzz, i, 0);
  }
  for (int i = 0; i < desc->num_args; i++) {
    const struct pbl_syscall_arg_desc *arg = &desc->args[i];
    if (prv_is_pointer_kind(arg->kind)) {
      prv_set_arg(fuzz, i, (uintptr_t)&fuzz->slots[i * SLOT_SIZE]);
      const int len = prv_length_arg(desc, i);
      if (len >= 0) {
        const bool is_array =
            arg->kind == PBL_SYSCALL_ARG_IN_ARRAY || arg->kind == PBL_SYSCALL_ARG_OUT_ARRAY;
        prv_set_arg(fuzz, len, is_array ? 1 : BUF_LEN);
      }
    } else if (prv_rule(desc, RULE_CALLBACK, i)) {
      prv_set_arg(fuzz, i, (uintptr_t)prv_noop);
    }
  }
}

[[noreturn]] static void prv_rejected(void) {
  struct fuzz *fuzz = app_state_get_user_data();
  if (!fuzz->calling) {
    __builtin_trap();
  }
  longjmp(fuzz->rejected, 1);
}

//! @return whether the syscall rejected its arguments.
static bool prv_call(struct fuzz *fuzz) {
  fuzz->calling = true;
  if (setjmp(fuzz->rejected) != 0) {
    fuzz->calling = false;
    return true;
  }
  fuzz->desc->call(fuzz->args);
  fuzz->calling = false;
  return false;
}

static void prv_fail(struct fuzz *fuzz, int arg, uint64_t value, const char *what) {
  fuzz->failures++;
  if (value > UINT32_MAX) {
    PBL_LOG_ERR("FUZZ FAIL %s arg %d value %u:%u: %s", fuzz->desc->name, arg,
                (unsigned int)(value >> 32), (unsigned int)value, what);
  } else {
    PBL_LOG_ERR("FUZZ FAIL %s arg %d value %u: %s", fuzz->desc->name, arg, (unsigned int)value,
                what);
  }
}

static bool prv_points_outside(const struct fuzz *fuzz, uintptr_t value, size_t len) {
  return value < fuzz->targets.app_ram_start || value + len > fuzz->targets.app_ram_end ||
         value + len < value;
}

//! Bytes the syscall accesses through pointer argument arg, as the baseline sets it up.
static size_t prv_access_len(const struct fuzz *fuzz, int arg) {
  const struct pbl_syscall_arg_desc *a = &fuzz->desc->args[arg];
  switch (a->kind) {
    case PBL_SYSCALL_ARG_IN_BUF:
    case PBL_SYSCALL_ARG_OUT_BUF:
      return prv_length_arg(fuzz->desc, arg) >= 0 ? BUF_LEN : 1;
    case PBL_SYSCALL_ARG_STR:
    case PBL_SYSCALL_ARG_STRN:
      return 1;
    default:
      return a->elem_size;
  }
}

static uint32_t prv_try(struct fuzz *fuzz, int arg, uint64_t value) {
  const struct pbl_syscall_desc *desc = fuzz->desc;
  const struct pbl_syscall_arg_desc *a = &desc->args[arg];
  prv_set_baseline(fuzz);
  prv_set_arg(fuzz, arg, value);

  // A length or count gets its buffer at the very end of the app's memory, so
  // that one too long reaches past it instead of over the fuzzer's own state.
  const int buf = prv_buffer_arg(desc, arg);
  size_t buf_len = 0;
  if (buf >= 0) {
    buf_len = prv_access_len(fuzz, buf);
    prv_set_arg(fuzz, buf, fuzz->targets.app_ram_end - buf_len);
  }

  const bool rejected = prv_call(fuzz);

  if (prv_is_pointer_kind(a->kind) && !rejected && !(value == 0 && prv_is_optional(a->kind)) &&
      prv_points_outside(fuzz, value, prv_access_len(fuzz, arg))) {
    prv_fail(fuzz, arg, value, "pointer outside the app accepted");
  }
  if (buf >= 0 && !rejected && value * desc->args[buf].elem_size > buf_len) {
    prv_fail(fuzz, arg, value, "length past the app accepted");
  }
  if (!sys_syscall_fuzz_check_canary()) {
    prv_fail(fuzz, arg, value, "kernel canary changed");
  }
  return rejected;
}

static void prv_fuzz_syscall(struct fuzz *fuzz) {
  const struct pbl_syscall_desc *desc = fuzz->desc;
  fuzz->syscalls++;

  const struct rule *skip = prv_rule(desc, RULE_SKIP, -1);
  if (skip || (desc->flags & PBL_SYSCALL_DESC_NORETURN)) {
    PBL_LOG_INFO("FUZZ SKIP %s: %s", desc->name, skip ? skip->reason : "never returns");
    return;
  }

  prv_set_baseline(fuzz);
  const bool baseline_rejected = prv_call(fuzz);
  uint32_t cases = 1;
  uint32_t rejected = baseline_rejected;

  for (int i = 0; i < desc->num_args; i++) {
    const struct pbl_syscall_arg_desc *arg = &desc->args[i];
    if (prv_rule(desc, RULE_CALLBACK, i)) {
      continue;
    }
    const struct rule *max = prv_rule(desc, RULE_MAX, i);
    if (prv_is_pointer_kind(arg->kind) || arg->type_class == PBL_SYSCALL_TYPE_POINTER) {
      for (size_t v = 0; v < fuzz->num_pointers; v++) {
        rejected += prv_try(fuzz, i, fuzz->pointers[v]);
        cases++;
      }
    }
    if (prv_is_pointer_kind(arg->kind)) {
      continue;
    }
    if (arg->type_class == PBL_SYSCALL_TYPE_BOOL) {
      rejected += prv_try(fuzz, i, 1);
      cases++;
      continue;
    }
    const size_t bits = MIN(arg->size, sizeof(uint32_t)) * 8;
    const uint64_t limit = arg->size == sizeof(uint64_t) ? UINT64_MAX : (1ull << bits) - 1;
    for (size_t v = 0; v < ARRAY_LENGTH(s_ints); v++) {
      const uint64_t value = s_ints[v];
      if (value > limit || (max && value > max->max)) {
        continue;
      }
      rejected += prv_try(fuzz, i, value);
      cases++;
    }
  }

  PBL_LOG_INFO("FUZZ %s cases %u rejected %u baseline %s", desc->name, (unsigned int)cases,
               (unsigned int)rejected, baseline_rejected ? "rejected" : "accepted");
}

static void prv_step(void *data) {
  struct fuzz *fuzz = data;
  if (fuzz->desc == __syscall_desc_end__) {
    sys_syscall_fuzz_arm(nullptr);
    PBL_LOG_INFO("FUZZ DONE syscalls %u failures %u", (unsigned int)fuzz->syscalls,
                 (unsigned int)fuzz->failures);
    return;
  }
  prv_fuzz_syscall(fuzz);
  fuzz->desc++;
  app_timer_register(0, prv_step, fuzz);
}

static void prv_init(struct fuzz *fuzz) {
  fuzz->slots = app_malloc_check(SLOT_SIZE * MAX_ARGS);
  fuzz->desc = __syscall_desc_start__;
  for (int i = 0; i < MAX_ARGS; i++) {
    fuzz->args[i] = fuzz->values[i];
  }

  sys_syscall_fuzz_arm(prv_rejected);
  sys_syscall_fuzz_get_targets(&fuzz->targets);
  const struct syscall_fuzz_targets *t = &fuzz->targets;
  const uintptr_t pointers[] = {
    0,
    4,
    t->canary,
    t->kernel_stack,
    t->syscall_stack,
    t->worker_ram,
    t->flash,
    t->app_ram_start - 4,
    t->app_ram_end - 4,
    UINTPTR_MAX - 15,
  };
  memcpy(fuzz->pointers, pointers, sizeof(pointers));
  fuzz->num_pointers = ARRAY_LENGTH(pointers);

  PBL_LOG_INFO("FUZZ START syscalls %u",
               (unsigned int)(__syscall_desc_end__ - __syscall_desc_start__));
  app_timer_register(0, prv_step, fuzz);
}

static void prv_main(void) {
  struct fuzz *fuzz = app_malloc_check(sizeof(*fuzz));
  memset(fuzz, 0, sizeof(*fuzz));
  app_state_set_user_data(fuzz);
  window_init(&fuzz->window, WINDOW_NAME("Syscall Fuzz"));
  app_window_stack_push(&fuzz->window, false);
  prv_init(fuzz);
  app_event_loop();
}

const PebbleProcessMd *syscall_fuzz_get_info(void) {
  static const PebbleProcessMdSystem s_info = {
    .common.main_func = prv_main,
    .common.is_unprivileged = true,
    .name = "Syscall Fuzz",
  };
  return (const PebbleProcessMd *)&s_info;
}
