/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "samsa_runtime.h"

#include <inttypes.h>

#include <pbl/logging/logging.h>
#include <pbl/util/size.h>

#include <kernel/pebble_tasks.h>
#include <process_management/pebble_process_md.h>
#include <process_management/process_manager.h>
#include <process_management/process_runtime.h>

PBL_LOG_MODULE_DEFINE(samsa, CONFIG_SAMSA_LOG_LEVEL);

extern char __APP_RAM__[];
extern char __APP_RAM_end__[];
extern char __WORKER_RAM__[];
extern char __WORKER_RAM_end__[];

// The 32-bit address space processes see. The SDK's function table and the
// hooks it points to are shared; the app and the worker each have a part of
// their own for their RAM, scratch and handles, so an address tells whose
// it is.
#define GUEST_TABLE   UINT32_C(0x10000000)
#define HANDLE_STRIDE UINT32_C(0x10000)
#define MAX_HANDLES   4096
#define MAX_OBJS      1024
#define MAX_OPAQUE    256

#define APP_SCRATCH_SIZE    (256 * 1024)
#define APP_ARENA_SIZE      (256 * 1024)
#define WORKER_SCRATCH_SIZE (32 * 1024)
#define WORKER_ARENA_SIZE   (32 * 1024)

// App views of host objects (shadows, in scratch) and host copies of app
// objects (in the arena), for types laid out differently.
typedef struct {
  uint32_t guest;
  void *host;
  const SamsaType *type;
  bool shadow;
  bool retained;
  uint32_t count;
  uint8_t *snap;
} Obj;

typedef struct {
  const char *name;
  PebbleTask task;
  char *ram;
  char *ram_end;
  uint32_t ram_base;
  uint32_t scratch_base;
  uint32_t handles_base;
  uint8_t *scratch;
  uint32_t scratch_size;
  uint8_t *arena;
  size_t arena_size;

  SamsaCpu cpu;
  SamsaRegion regions[3];
  SamsaHook hooks[1];
  bool running;
  uint32_t last_call;

  // Firmware objects outside the process's RAM, one 64 KiB window of its
  // address space each.
  const uint8_t *handles[MAX_HANDLES];
  unsigned num_handles;
  // Values the firmware handed out as pointers that are not addresses, such
  // as animation handles, so they are accepted when the app hands them back.
  uint32_t opaque[MAX_OPAQUE];
  unsigned next_opaque;
  // Values the process passed as pointers that are not addresses, such as an
  // int as a callback's context, so it gets them back as they were.
  uint32_t ints[MAX_OPAQUE];
  unsigned next_int;

  uint32_t scratch_used;
  size_t arena_used;
  Obj objs[MAX_OBJS];
  unsigned num_objs;
  // Set while converting objects the firmware keeps, which marks everything
  // they point to as kept too.
  bool retaining;
} Process;

static uint8_t s_app_scratch[APP_SCRATCH_SIZE] __attribute__((aligned(8)));
static uint8_t s_app_arena[APP_ARENA_SIZE] __attribute__((aligned(16)));
static uint8_t s_worker_scratch[WORKER_SCRATCH_SIZE] __attribute__((aligned(8)));
static uint8_t s_worker_arena[WORKER_ARENA_SIZE] __attribute__((aligned(16)));

static Process s_processes[] = {
  {
    .name = "App",
    .task = PebbleTask_App,
    .ram = __APP_RAM__,
    .ram_end = __APP_RAM_end__,
    .ram_base = UINT32_C(0x20000000),
    .scratch_base = UINT32_C(0x30000000),
    .handles_base = UINT32_C(0x40000000),
    .scratch = s_app_scratch,
    .scratch_size = sizeof(s_app_scratch),
    .arena = s_app_arena,
    .arena_size = sizeof(s_app_arena),
  },
  {
    .name = "Worker",
    .task = PebbleTask_Worker,
    .ram = __WORKER_RAM__,
    .ram_end = __WORKER_RAM_end__,
    .ram_base = UINT32_C(0x28000000),
    .scratch_base = UINT32_C(0x38000000),
    .handles_base = UINT32_C(0x50000000),
    .scratch = s_worker_scratch,
    .scratch_size = sizeof(s_worker_scratch),
    .arena = s_worker_arena,
    .arena_size = sizeof(s_worker_arena),
  },
};

static Process *prv_process_for_task(PebbleTask task) {
  return task == PebbleTask_Worker ? &s_processes[1] : &s_processes[0];
}

//! The process of the task the firmware calls from.
static Process *prv_current(void) {
  return prv_process_for_task(pebble_task_get_current());
}

[[noreturn]] static void prv_kill(Process *p, const char *why);

// ----------------------------------------------------------------------------
// Addresses

static bool prv_in(const void *h, const void *start, size_t size) {
  return (const uint8_t *)h >= (const uint8_t *)start &&
         (const uint8_t *)h < (const uint8_t *)start + size;
}

static bool prv_find(const uint32_t *values, uint32_t v) {
  for (unsigned i = 0; i < MAX_OPAQUE; i++) {
    if (values[i] == v) {
      return true;
    }
  }
  return false;
}

static void prv_remember(uint32_t *values, unsigned *next, uint32_t v) {
  if (!prv_find(values, v)) {
    values[(*next)++ % MAX_OPAQUE] = v;
  }
}

//! Host pointer for an address of the process, and whether it was one: a
//! value that is not passes through as it is.
static void *prv_g2h_mapped(const Process *p, uint32_t g, bool *mapped) {
  *mapped = true;
  if (g == 0) {
    return nullptr;
  }
  for (unsigned i = 0; i < PBL_ARRAY_SIZE(p->regions); i++) {
    const SamsaRegion *r = &p->regions[i];
    if (g - r->guest_base < r->size) {
      return r->host + (g - r->guest_base);
    }
  }
  if (g - p->handles_base < MAX_HANDLES * HANDLE_STRIDE) {
    const uint32_t idx = (g - p->handles_base) / HANDLE_STRIDE;
    if (idx < p->num_handles) {
      return (void *)(p->handles[idx] + (g - p->handles_base) % HANDLE_STRIDE);
    }
  }
  *mapped = false;
  return (void *)(uintptr_t)g;
}

static void *prv_g2h(const Process *p, uint32_t g) {
  bool mapped;
  return prv_g2h_mapped(p, g, &mapped);
}

void *samsa_g2h(uint32_t g) {
  Process *p = prv_current();
  bool mapped;
  void *h = prv_g2h_mapped(p, g, &mapped);
  if (!mapped) {
    prv_remember(p->ints, &p->next_int, g);
  }
  return h;
}

void *samsa_g2h_ptr(uint32_t g) {
  Process *p = prv_current();
  bool mapped;
  void *h = prv_g2h_mapped(p, g, &mapped);
  if (!mapped) {
    p->cpu.fault_addr = g;
    prv_kill(p, "bad pointer passed to the SDK");
  }
  return h;
}

void *samsa_g2h_handle(uint32_t g) {
  Process *p = prv_current();
  bool mapped;
  void *h = prv_g2h_mapped(p, g, &mapped);
  if (!mapped && !prv_find(p->opaque, g)) {
    p->cpu.fault_addr = g;
    prv_kill(p, "bad pointer passed to the SDK");
  }
  return h;
}

static uint32_t prv_h2g(Process *p, const void *h) {
  if (h == nullptr) {
    return 0;
  }
  for (unsigned i = 0; i < PBL_ARRAY_SIZE(p->regions); i++) {
    const SamsaRegion *r = &p->regions[i];
    if (prv_in(h, r->host, r->size)) {
      return r->guest_base + (uint32_t)((const uint8_t *)h - r->host);
    }
  }
  // A value the process passed in goes back as it was. On a 64-bit host,
  // nothing else that fits 32 bits is an address either: it is a value the
  // firmware made up, which the process may hand back. On a 32-bit host, any
  // other value may be an address, and gets a handle.
  const uintptr_t v = (uintptr_t)h;
  if (v <= UINT32_MAX && prv_find(p->ints, (uint32_t)v)) {
    return (uint32_t)v;
  }
  if (sizeof(void *) > sizeof(uint32_t) && v <= UINT32_MAX) {
    prv_remember(p->opaque, &p->next_opaque, (uint32_t)v);
    return (uint32_t)v;
  }
  const uint8_t *b = h;
  for (unsigned i = 0; i < p->num_handles; i++) {
    if (b >= p->handles[i] && (uintptr_t)(b - p->handles[i]) < HANDLE_STRIDE) {
      return p->handles_base + i * HANDLE_STRIDE + (uint32_t)(b - p->handles[i]);
    }
  }
  if (p->num_handles == MAX_HANDLES) {
    PBL_LOG_ERR("Out of handles for %p", h);
    return 0;
  }
  p->handles[p->num_handles] = b;
  return p->handles_base + p->num_handles++ * HANDLE_STRIDE;
}

uint32_t samsa_h2g(const void *h) {
  return prv_h2g(prv_current(), h);
}

// Processes read and write firmware objects through their handles.
static bool prv_is_handle(const Process *p, uint32_t addr) {
  return addr - p->handles_base < p->num_handles * HANDLE_STRIDE;
}

static bool prv_bus_read(SamsaCpu *cpu, uint32_t addr, unsigned size, uint32_t *out) {
  const Process *p = cpu->user;
  if (!prv_is_handle(p, addr)) {
    return false;
  }
  *out = 0;
  memcpy(out, prv_g2h(p, addr), size);
  return true;
}

static bool prv_bus_write(SamsaCpu *cpu, uint32_t addr, unsigned size, uint32_t val) {
  const Process *p = cpu->user;
  if (!prv_is_handle(p, addr)) {
    return false;
  }
  memcpy(prv_g2h(p, addr), &val, size);
  return true;
}

static const SamsaBus s_bus = {
  .read = prv_bus_read,
  .write = prv_bus_write,
};

// ----------------------------------------------------------------------------
// Objects of types laid out differently
//
// Each object has an app side and a host side. A snapshot of the app side
// as last converted tells which side changed: an app object the app did not
// touch since is not converted again, so the firmware's own changes to its
// side (an AppSync it updates as messages arrive) survive.

static void *prv_arena_alloc(Process *p, size_t size) {
  size = (size + 15) & ~(size_t)15;
  if (p->arena_used + size > p->arena_size) {
    return nullptr;
  }
  void *a = p->arena + p->arena_used;
  p->arena_used += size;
  return a;
}

static Obj *prv_obj_find_guest(Process *p, uint32_t g, const SamsaType *type) {
  for (unsigned i = 0; i < p->num_objs; i++) {
    if (p->objs[i].guest == g && p->objs[i].type == type) {
      return &p->objs[i];
    }
  }
  return nullptr;
}

static Obj *prv_obj_find_host(Process *p, const void *h, const SamsaType *type) {
  for (unsigned i = 0; i < p->num_objs; i++) {
    if (p->objs[i].host == h && (!type || p->objs[i].type == type)) {
      return &p->objs[i];
    }
  }
  return nullptr;
}

//! The app's view of a firmware object, whatever type it was made for: SDK 2
//! apps cast a property animation to the animation it starts with.
static Obj *prv_shadow_find(Process *p, uint32_t g, const void *h) {
  for (unsigned i = 0; i < p->num_objs; i++) {
    if (p->objs[i].shadow && (g ? p->objs[i].guest == g : p->objs[i].host == h)) {
      return &p->objs[i];
    }
  }
  return nullptr;
}

static Obj *prv_obj_add(Process *p, uint32_t g, void *h, uint32_t count, const SamsaType *type,
                        bool shadow) {
  uint8_t *snap = prv_arena_alloc(p, count * type->guest_size);
  if (p->num_objs == MAX_OBJS || !snap) {
    PBL_LOG_ERR("Out of room to convert %s", type->name);
    return nullptr;
  }
  Obj *o = &p->objs[p->num_objs++];
  *o = (Obj){
    .guest = g,
    .host = h,
    .type = type,
    .shadow = shadow,
    .retained = p->retaining,
    .count = count,
    .snap = snap,
  };
  return o;
}

static void prv_obj_to_host(Process *p, Obj *o, bool force) {
  const uint8_t *gbytes = prv_g2h(p, o->guest);
  const uint32_t size = o->count * o->type->guest_size;
  if (force || memcmp(o->snap, gbytes, size) != 0) {
    void (*g2h)(const uint8_t *, void *) =
        (o->shadow && o->type->shadow_g2h) ? o->type->shadow_g2h : o->type->g2h;
    for (uint32_t i = 0; i < o->count; i++) {
      g2h(gbytes + i * o->type->guest_size, (char *)o->host + i * o->type->host_size);
    }
    memcpy(o->snap, gbytes, size);
  }
}

static void prv_obj_to_guest(Process *p, Obj *o) {
  uint8_t *gbytes = prv_g2h(p, o->guest);
  for (uint32_t i = 0; i < o->count; i++) {
    o->type->h2g((const char *)o->host + i * o->type->host_size, gbytes + i * o->type->guest_size);
  }
  memcpy(o->snap, gbytes, o->count * o->type->guest_size);
}

void *samsa_obj_in_array(uint32_t g, uint32_t count, const SamsaType *type) {
  if (g == 0 || count == 0) {
    return samsa_g2h_ptr(g);
  }
  samsa_g2h_ptr(g);
  samsa_g2h_ptr(g + count * type->guest_size - 1);
  Process *p = prv_current();
  Obj *o = prv_obj_find_guest(p, g, type);
  if (!o) {
    o = prv_shadow_find(p, g, nullptr);
  }
  if (o && o->count >= count) {
    prv_obj_to_host(p, o, false);
    return o->host;
  }
  void *h = prv_arena_alloc(p, count * type->host_size);
  o = h ? prv_obj_add(p, g, h, count, type, false) : nullptr;
  if (!o) {
    PBL_LOG_ERR("Out of arena for %s", type->name);
    return nullptr;
  }
  memset(h, 0, count * type->host_size);
  prv_obj_to_host(p, o, true);
  return h;
}

void *samsa_obj_in_array_soft(uint32_t g, uint32_t count, const SamsaType *type) {
  const Process *p = prv_current();
  bool first, last = true;
  prv_g2h_mapped(p, g, &first);
  if (count) {
    prv_g2h_mapped(p, g + count * type->guest_size - 1, &last);
  }
  if (g == 0 || !first || !last) {
    return nullptr;
  }
  return samsa_obj_in_array(g, count, type);
}

void *samsa_obj_in_array_retained(uint32_t g, uint32_t count, const SamsaType *type) {
  Process *p = prv_current();
  const bool was = p->retaining;
  p->retaining = true;
  void *h = samsa_obj_in_array(g, count, type);
  Obj *o = h ? prv_obj_find_guest(p, g, type) : nullptr;
  if (o) {
    o->retained = true;
  }
  p->retaining = was;
  return h;
}

uint32_t samsa_obj_guest_of(void *h, const SamsaType *type) {
  Process *p = prv_current();
  Obj *o = h ? prv_obj_find_host(p, h, type) : nullptr;
  return o ? o->guest : prv_h2g(p, h);
}

//! Bring kept objects up to date with what the app changed since.
static void prv_sync_retained(Process *p) {
  p->retaining = true;
  for (unsigned i = 0; i < p->num_objs; i++) {
    if (p->objs[i].retained) {
      prv_obj_to_host(p, &p->objs[i], false);
    }
  }
  p->retaining = false;
}

void *samsa_obj_in(uint32_t g, const SamsaType *type) {
  return samsa_obj_in_array(g, 1, type);
}

void *samsa_obj_in_soft(uint32_t g, const SamsaType *type) {
  bool mapped;
  prv_g2h_mapped(prv_current(), g, &mapped);
  if (g == 0 || !mapped) {
    return nullptr;
  }
  return samsa_obj_in_array(g, 1, type);
}

uint32_t samsa_obj_out(void *h, const SamsaType *type) {
  if (h == nullptr) {
    return 0;
  }
  Process *p = prv_current();
  Obj *o = prv_obj_find_host(p, h, type);
  if (!o) {
    o = prv_shadow_find(p, 0, h);
  }
  if (!o) {
    const uint32_t size = (type->guest_size + 7) & ~UINT32_C(7);
    if (p->scratch_used + size > p->scratch_size) {
      PBL_LOG_ERR("Out of scratch for %s", type->name);
      return 0;
    }
    o = prv_obj_add(p, p->scratch_base + p->scratch_used, h, 1, type, true);
    if (!o) {
      return 0;
    }
    p->scratch_used += size;
  }
  prv_obj_to_guest(p, o);
  return o->guest;
}

void samsa_obj_sync(uint32_t g, void *h, const SamsaType *type) {
  Process *p = prv_current();
  Obj *o = g ? prv_obj_find_guest(p, g, type) : nullptr;
  if (o && o->host == h) {
    prv_obj_to_guest(p, o);
  }
}

void samsa_obj_back(uint32_t g, void *h, const SamsaType *type) {
  Process *p = prv_current();
  Obj *o = g ? prv_obj_find_guest(p, g, type) : nullptr;
  if (o && o->host == h) {
    prv_obj_to_host(p, o, false);
  }
}

// ----------------------------------------------------------------------------
// Callbacks

void *samsa_cb_get(SamsaCbPool *pool, uint32_t g) {
  if (g == 0) {
    return nullptr;
  }
  for (unsigned i = 0; i < pool->count; i++) {
    if (pool->guest[i] == g) {
      return pool->fns[i];
    }
  }
  for (unsigned i = 0; i < pool->count; i++) {
    if (pool->guest[i] == 0) {
      pool->guest[i] = g;
      return pool->fns[i];
    }
  }
  PBL_LOG_ERR("Out of callbacks for %s", pool->signature);
  return nullptr;
}

// Callbacks whose signature is not known, from unions of callbacks such as
// a property animation's setters. Each takes a pointer and one value of up
// to 64 bits, which a 64-bit host passes in a single register for all of
// them, and a getter returns its value in one. A WebAssembly host checks the
// signature of each call: these are the setters', which take a pointer and a
// 32-bit value and return nothing. It passes a pointer for the larger values
// (a GPoint or a GRect), which an app's setter of those then cannot read.
#define RAW_SLOTS 16
static uint32_t s_raw_guest[RAW_SLOTS];

static uint32_t prv_raw_call(unsigned k, void *a0, uint64_t a1) {
  const uint32_t w[3] = {samsa_h2g(a0), (uint32_t)a1, (uint32_t)(a1 >> 32)};
  uint32_t r = 0;
  samsa_call_guest(s_raw_guest[k], w, PBL_ARRAY_SIZE(w), &r);
  return r;
}

#if UINTPTR_MAX > UINT32_MAX
#define RAW(k)                                         \
  static uint64_t prv_raw_##k(void *a0, uint64_t a1) { \
    return prv_raw_call(k, a0, a1);                    \
  }
#else
#define RAW(k)                                     \
  static void prv_raw_##k(void *a0, uint32_t a1) { \
    prv_raw_call(k, a0, a1);                       \
  }
#endif
// clang-format off
RAW(0) RAW(1) RAW(2) RAW(3) RAW(4) RAW(5) RAW(6) RAW(7)
RAW(8) RAW(9) RAW(10) RAW(11) RAW(12) RAW(13) RAW(14) RAW(15)
#undef RAW

static void *const s_raw_fns[RAW_SLOTS] = {
  prv_raw_0, prv_raw_1, prv_raw_2,  prv_raw_3,  prv_raw_4,  prv_raw_5,  prv_raw_6,  prv_raw_7,
  prv_raw_8, prv_raw_9, prv_raw_10, prv_raw_11, prv_raw_12, prv_raw_13, prv_raw_14, prv_raw_15,
};
// clang-format on

static SamsaCbPool s_raw_pool = {"raw", s_raw_guest, s_raw_fns, RAW_SLOTS};

void *samsa_cb_raw_get(uint32_t g) {
  return samsa_cb_get(&s_raw_pool, g);
}

uint32_t samsa_cb_to_guest(void *fn) {
  if (fn == nullptr) {
    return 0;
  }
  for (unsigned i = 0; i < RAW_SLOTS; i++) {
    if (s_raw_fns[i] == fn) {
      return s_raw_guest[i];
    }
  }
  for (SamsaCbPool *const *p = g_samsa_cb_pools; *p; p++) {
    for (unsigned i = 0; i < (*p)->count; i++) {
      if ((*p)->fns[i] == fn) {
        return (*p)->guest[i];
      }
    }
  }
  PBL_LOG_WRN("Firmware function %p handed to app", fn);
  return 0;
}

// ----------------------------------------------------------------------------
// Running

static const char *prv_stop_name(SamsaStop stop) {
  switch (stop) {
    case SAMSA_STOP_BUDGET:
      return "budget";
    case SAMSA_STOP_HOOK:
      return "hook";
    case SAMSA_STOP_RETURN:
      return "return";
    case SAMSA_STOP_SVC:
      return "svc";
    case SAMSA_STOP_BKPT:
      return "bkpt";
    case SAMSA_STOP_UNDEF:
      return "undefined instruction";
    case SAMSA_STOP_MEMFAULT:
      return "memory fault";
    case SAMSA_STOP_USAGE:
      return "usage fault";
  }
  return "?";
}

[[noreturn]] static void prv_kill(Process *p, const char *why) {
  PBL_LOG_ERR("%s crashed: pc 0x%" PRIx32 " lr 0x%" PRIx32 " addr 0x%" PRIx32 ", %s in %s", p->name,
              p->cpu.fault_pc, p->cpu.r[14], p->cpu.fault_addr, why,
              g_samsa_thunk_names[p->last_call] ?: "?");
  p->running = false;
  // As on a watch, the process goes wherever it was; the kernel cleans up
  // after it.
  process_manager_task_exit();
}

static SamsaStop prv_handler(SamsaCpu *cpu, SamsaStop why, void *ctx) {
  if (why != SAMSA_STOP_HOOK) {
    return why;
  }
  Process *p = cpu->user;
  const uint32_t idx = (cpu->r[15] - SAMSA_GUEST_HOOKS) / 4;
  if (idx >= g_samsa_num_thunks) {
    return SAMSA_STOP_USAGE;
  }
  p->last_call = idx;
  prv_sync_retained(p);
  SamsaThunk thunk = g_samsa_thunks[idx];
  if (!thunk) {
    prv_kill(p, "unsupported SDK call");
  }
  thunk(cpu);
  return SAMSA_STOP_BUDGET;
}

void samsa_call_guest(uint32_t fn, const uint32_t *args, unsigned nargs, uint32_t *ret) {
  Process *p = prv_current();
  if (!p->running || pebble_task_get_current() != p->task ||
      fn - p->ram_base >= (uint32_t)(p->ram_end - p->ram)) {
    PBL_LOG_ERR("Callback 0x%" PRIx32 " outside its process's task", fn);
    *ret = 0;
    return;
  }
  const SamsaStop stop =
      samsa_call_budget(&p->cpu, fn | 1, args, nargs, ret, prv_handler, nullptr, UINT32_MAX);
  if (stop != SAMSA_STOP_RETURN) {
    prv_kill(p, prv_stop_name(stop));
  }
}

//! Forget what an earlier run of the process left behind.
static void prv_reset(Process *p) {
  const uint32_t ram_size = (uint32_t)(p->ram_end - p->ram);
  for (SamsaCbPool *const *pool = g_samsa_cb_pools; *pool; pool++) {
    for (unsigned i = 0; i < (*pool)->count; i++) {
      if ((*pool)->guest[i] - p->ram_base < ram_size) {
        (*pool)->guest[i] = 0;
      }
    }
  }
  for (unsigned i = 0; i < RAW_SLOTS; i++) {
    if (s_raw_guest[i] - p->ram_base < ram_size) {
      s_raw_guest[i] = 0;
    }
  }
  p->num_handles = 0;
  memset(p->opaque, 0, sizeof(p->opaque));
  p->next_opaque = 0;
  memset(p->ints, 0, sizeof(p->ints));
  p->next_int = 0;
  p->scratch_used = 0;
  p->arena_used = 0;
  p->num_objs = 0;
  p->retaining = false;
  p->last_call = 0;

  samsa_reset(&p->cpu);
  p->regions[0] = (SamsaRegion){
    .guest_base = p->ram_base,
    .size = ram_size,
    .host = (uint8_t *)p->ram,
    .perms = SAMSA_R | SAMSA_W | SAMSA_X,
  };
  p->regions[1] = (SamsaRegion){
    .guest_base = GUEST_TABLE,
    .size = g_samsa_num_thunks * sizeof(uint32_t),
    .host = (uint8_t *)g_samsa_sdk_table,
    .perms = SAMSA_R,
  };
  p->regions[2] = (SamsaRegion){
    .guest_base = p->scratch_base,
    .size = p->scratch_size,
    .host = p->scratch,
    .perms = SAMSA_R | SAMSA_W,
  };
  p->hooks[0] = (SamsaHook){SAMSA_GUEST_HOOKS, SAMSA_GUEST_HOOKS + g_samsa_num_thunks * 4};
  p->cpu.regions = p->regions;
  p->cpu.num_regions = PBL_ARRAY_SIZE(p->regions);
  p->cpu.hooks = p->hooks;
  p->cpu.num_hooks = PBL_ARRAY_SIZE(p->hooks);
  p->cpu.bus = &s_bus;
  p->cpu.user = p;
}

// ----------------------------------------------------------------------------
// The process runtime of the firmware: processes not built into it are ARM
// code, which Samsa interprets.

uint32_t process_runtime_address(PebbleTask task, const void *address) {
  const Process *p = prv_process_for_task(task);
  return p->ram_base + (uint32_t)((const char *)address - p->ram);
}

uint32_t process_runtime_sdk_table(PebbleTask task) {
  return GUEST_TABLE;
}

void process_runtime_run(const PebbleProcessMd *app_md, PebbleTask task, void *entry,
                         void *stack_top) {
  if (app_md->process_storage == ProcessStorageBuiltin) {
    const PebbleMain main_func = entry;
    main_func();
    return;
  }
  Process *p = prv_process_for_task(task);
  prv_reset(p);
  p->cpu.r[13] = prv_h2g(p, stack_top) & ~UINT32_C(7);
  p->running = true;

  const uint32_t entry_g = prv_h2g(p, (void *)((uintptr_t)entry & ~(uintptr_t)1));
  PBL_LOG_INFO("Running %s at 0x%" PRIx32 " with Samsa", p->name, entry_g);
  uint32_t ret;
  samsa_call_guest(entry_g, nullptr, 0, &ret);
  p->running = false;
}
