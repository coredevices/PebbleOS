// SPDX-FileCopyrightText: 2026 Core Devices LLC
// SPDX-License-Identifier: Apache-2.0
//
// Host simulation of the Zephyr BR/EDR host: Zephyr, port/ and ctl/ with the
// HCI router and a simulated controller and phone, on the POSIX PebbleOS
// kernel. See README.md.
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pbl/bluetooth/hci_router.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mutex.h>
#include <pbl/kernel/sem.h>
#include <pbl/kernel/thread.h>

#include <kernel_test.h>
#include <zbt/zbt.h>

[[noreturn]] void pbl_panic_bare_at(const char *filename, int line) {
  fprintf(stderr, "kernel panic at %s:%d\n", filename, line);
  abort();
}
void *kernel_malloc(size_t bytes) {
  return malloc(bytes);
}
void *kernel_zalloc(size_t bytes) {
  return calloc(1, bytes);
}
void *kernel_calloc(size_t count, size_t size) {
  return calloc(count, size);
}
void kernel_free(void *ptr) {
  free(ptr);
}
// The call's audio: counted, with the voice link up between start and stop.
#include <pbl/bluetooth/call_audio.h>
static int s_audio_starts, s_audio_stops;
static bool s_audio_on;
void call_audio_start(void) {
  // The real one takes mutexes and waits; so must its caller allow.
  pbl_thread_sleep(PBL_MSEC(2));
  if (!s_audio_on) {
    s_audio_on = true;
    s_audio_starts++;
  }
}
void call_audio_stop(void) {
  pbl_thread_sleep(PBL_MSEC(2));
  if (s_audio_on) {
    s_audio_on = false;
    s_audio_stops++;
  }
}

// The watch's name, as LE advertises it.
void bt_local_id_copy_device_name(char name_out[20], bool is_le) {
  strncpy(name_out, "Pebble 27AC", 20);
}
uint32_t pbl_rand32(void) {
  return (uint32_t)rand();
}

/* ---- router glue (as hci_sf32lb52.c) ---- */
static struct hci_router s_router;
static bool s_router_active = true;
static PBL_MUTEX_DEFINE(s_tx_mutex);
static PBL_SEM_DEFINE(s_gate, 1, 1);

static int prv_nimble_evt(void *ctx, const uint8_t *evt, size_t len, bool d) {
  printf("  [nimble evt %02x len %zu]\n", evt[0], len);
  return 0;
}
static int prv_nimble_acl(void *ctx, const uint8_t *acl, size_t len) {
  return 0;
}
static bool prv_tx_lock(void *ctx, bool wait) {
  return pbl_mutex_lock(&s_tx_mutex, wait ? PBL_FOREVER : PBL_NO_WAIT) == 0;
}
static void prv_tx_unlock(void *ctx) {
  pbl_mutex_unlock(&s_tx_mutex);
}
static int prv_tx_write(void *ctx, const uint8_t *data, size_t len);
static bool prv_gate_take(void *ctx, bool wait) {
  return pbl_sem_take(&s_gate, wait ? PBL_MSEC(2500) : PBL_NO_WAIT) == 0;
}
static void prv_gate_give(void *ctx) {
  pbl_sem_give(&s_gate);
}
static const struct hci_router_ops s_ops = {
  .nimble_evt = prv_nimble_evt,
  .nimble_acl = prv_nimble_acl,
  .tx_lock = prv_tx_lock,
  .tx_unlock = prv_tx_unlock,
  .tx_write = prv_tx_write,
  .gate_take = prv_gate_take,
  .gate_give = prv_gate_give,
};

bool hci_router_is_active(void) {
  return s_router_active;
}
void hci_router_get_stats(struct hci_router_stats *out) {
  *out = s_router.stats;
}
int hci_router_classic_attach(hci_router_classic_cb_t evt, hci_router_classic_cb_t acl, void *ctx) {
  hci_router_set_classic(&s_router, evt, acl, ctx);
  return 0;
}

/* ---- PebbleOS services the control side uses ---- */
#include <pbl/services/new_timer/new_timer.h>

#include <kernel/event_loop.h>
#include <process_management/worker_manager.h>

#define CBQ 32
static struct {
  CallbackEventCallback cb;
  void *data;
} s_cbq[CBQ];
static int s_cbq_head, s_cbq_tail;
static PBL_SEM_DEFINE(s_cbq_sem, 0, CBQ);
void launcher_task_add_callback(CallbackEventCallback cb, void *data) {
  pbl_irq_lock();
  s_cbq[s_cbq_tail].cb = cb;
  s_cbq[s_cbq_tail].data = data;
  s_cbq_tail = (s_cbq_tail + 1) % CBQ;
  pbl_irq_unlock();
  pbl_sem_give(&s_cbq_sem);
}
static void prv_main_thread(void *arg) {
  for (;;) {
    pbl_sem_take(&s_cbq_sem, PBL_FOREVER);
    pbl_irq_lock();
    CallbackEventCallback cb = s_cbq[s_cbq_head].cb;
    void *d = s_cbq[s_cbq_head].data;
    s_cbq_head = (s_cbq_head + 1) % CBQ;
    pbl_irq_unlock();
    cb(d);
  }
}

// Repeating timers, at a 100 ms resolution: the start/stop polling and the battery sampling.
#define TIMERS 4
static struct {
  NewTimerCallback cb;
  void *data;
  uint32_t ms;
  uint32_t elapsed;
  bool run;
} s_timers[TIMERS];
static int s_timer_count;
TimerID new_timer_create(void) {
  return s_timer_count < TIMERS ? (TimerID)++s_timer_count : TIMER_INVALID_ID;
}
bool new_timer_start(TimerID t, uint32_t ms, NewTimerCallback cb, void *d, uint32_t flags) {
  if (t == TIMER_INVALID_ID || t > TIMERS) {
    return false;
  }
  pbl_irq_lock();
  s_timers[t - 1].cb = cb;
  s_timers[t - 1].data = d;
  s_timers[t - 1].ms = ms;
  s_timers[t - 1].elapsed = 0;
  s_timers[t - 1].run = true;
  pbl_irq_unlock();
  return true;
}
bool new_timer_stop(TimerID t) {
  if (t != TIMER_INVALID_ID && t <= TIMERS) {
    s_timers[t - 1].run = false;
  }
  return true;
}
static void prv_timer_thread(void *arg) {
  for (;;) {
    pbl_thread_sleep(PBL_MSEC(100));
    for (int i = 0; i < TIMERS; i++) {
      pbl_irq_lock();
      bool due = s_timers[i].run && (s_timers[i].elapsed += 100) >= s_timers[i].ms;
      if (due) {
        s_timers[i].elapsed = 0;
      }
      NewTimerCallback cb = s_timers[i].cb;
      void *d = s_timers[i].data;
      pbl_irq_unlock();
      if (due) {
        cb(d);
      }
    }
  }
}

// KernelBG: the same queue mechanism on its own thread.
#include <pbl/services/settings/settings_file.h>
#include <pbl/services/system_task.h>
static struct {
  SystemTaskEventCallback cb;
  void *data;
} s_bgq[CBQ];
static int s_bgq_head, s_bgq_tail;
static PBL_SEM_DEFINE(s_bgq_sem, 0, CBQ);
bool system_task_add_callback(SystemTaskEventCallback cb, void *data) {
  pbl_irq_lock();
  s_bgq[s_bgq_tail].cb = cb;
  s_bgq[s_bgq_tail].data = data;
  s_bgq_tail = (s_bgq_tail + 1) % CBQ;
  pbl_irq_unlock();
  pbl_sem_give(&s_bgq_sem);
  return true;
}
bool system_task_add_callback_droppable(SystemTaskEventCallback cb, void *data) {
  return system_task_add_callback(cb, data);
}
static void prv_bg_thread(void *arg) {
  for (;;) {
    pbl_sem_take(&s_bgq_sem, PBL_FOREVER);
    pbl_irq_lock();
    SystemTaskEventCallback cb = s_bgq[s_bgq_head].cb;
    void *d = s_bgq[s_bgq_head].data;
    s_bgq_head = (s_bgq_head + 1) % CBQ;
    pbl_irq_unlock();
    cb(d);
  }
}

// One-record settings file, as the key file uses it.
static uint8_t s_file_key[32], s_file_val[64];
static size_t s_file_key_len, s_file_val_len;
static bool s_file_has;
static int s_file_opens, s_file_writes;
static char s_file_name[32];
status_t settings_file_open(SettingsFile *f, const char *name, int max) {
  s_file_opens++;
  strncpy(s_file_name, name, sizeof(s_file_name) - 1);
  f->open = 1;
  return S_SUCCESS;
}
void settings_file_close(SettingsFile *f) {
  f->open = 0;
}
static bool prv_file_match(const void *k, size_t kl) {
  return s_file_has && kl == s_file_key_len && memcmp(k, s_file_key, kl) == 0;
}
bool settings_file_exists(SettingsFile *f, const void *k, size_t kl) {
  return prv_file_match(k, kl);
}
status_t settings_file_delete(SettingsFile *f, const void *k, size_t kl) {
  if (!prv_file_match(k, kl))
    return E_DOES_NOT_EXIST;
  s_file_has = false;
  return S_SUCCESS;
}
int settings_file_get_len(SettingsFile *f, const void *k, size_t kl) {
  return prv_file_match(k, kl) ? (int)s_file_val_len : 0;
}
status_t settings_file_get(SettingsFile *f, const void *k, size_t kl, void *v, size_t vl) {
  if (!prv_file_match(k, kl) || vl > s_file_val_len)
    return E_DOES_NOT_EXIST;
  memcpy(v, s_file_val, vl);
  return S_SUCCESS;
}
status_t settings_file_set(SettingsFile *f, const void *k, size_t kl, const void *v, size_t vl) {
  memcpy(s_file_key, k, kl);
  s_file_key_len = kl;
  memcpy(s_file_val, v, vl);
  s_file_val_len = vl;
  s_file_has = true;
  s_file_writes++;
  return S_SUCCESS;
}

// The control side's call log and phone books (ctl/zbt_call_log.c, ctl/zbt_book.c) need
// files; the one-record settings stand-in above is the key file's. Calls are not logged and
// book entries are taken and dropped, the next packet asked for as when they are saved.
#include <time.h>

#include <zbt_link.h>
time_t rtc_get_time(void) {
  return 0;
}
// The call events' times (zbt_trace()).
uint64_t rtc_get_ticks(void) {
  return 0;
}
void zbt_call_log_add(const struct zbt_call_record *rec) {
}
static uint8_t s_pbap_mem[ZBT_PBAP_MEM_SIZE] __attribute__((aligned(8)));
void *zbt_ctl_pbap_acquire(void) {
  return s_pbap_mem;
}
void zbt_ctl_pbap_release(void) {
}
void zbt_book_host_stopped(void) {
}
void zbt_ctl_book_start(enum zbt_book book) {
}
bool zbt_ctl_book_entry(const struct zbt_book_entry *entry) {
  return true;
}
static void prv_book_saved(void *data) {
  zbt_host_pbap_next();
}
void zbt_ctl_book_hand_over(bool end, bool ok) {
  (void)system_task_add_callback(prv_book_saved, NULL);
}

// The speaker's mute, which an answered call plays through.
#include <pbl/services/speaker/speaker_service.h>
void speaker_service_set_call_unmuted(bool unmuted) {
}

// The battery the host samples while it runs.
#include <pbl/drivers/battery.h>
int battery_get_constants(BatteryConstants *constants) {
  return -1;
}

// MPU regions: the POSIX kernel ignores them.
#include <kernel/memory_layout.h>
static const MpuRegion s_sim_region = {.permissions = MpuPermissions_PrivRW};
const MpuRegion *memory_layout_get_app_region(void) {
  return &s_sim_region;
}
const MpuRegion *memory_layout_get_worker_region(void) {
  return &s_sim_region;
}
void mpu_init_region_from_region(MpuRegion *copy, const MpuRegion *from, bool allow_user_access) {
  *copy = *from;
}

// Worker: takes a while to close, scribbles over its RAM while it runs.
extern uint8_t __zbt_wdata_start[], __zbt_wdata_end[], __zbt_wbss_start[], __zbt_wbss_end[];
// In the firmware both are the worker RAM; here they are apart.
static void prv_scribble(void) {
  memset(__zbt_wdata_start, 0xA5, __zbt_wdata_end - __zbt_wdata_start);
  memset(__zbt_wbss_start, 0xA5, __zbt_wbss_end - __zbt_wbss_start);
}
static bool s_lent, s_worker_running = true;
static int64_t s_worker_close_at;
static uint32_t s_refused;
void worker_manager_lend_ram(void) {
  s_lent = true;
  s_worker_close_at = 1;
}
bool worker_manager_ram_is_free(void) {
  if (s_worker_running && s_worker_close_at && ++s_worker_close_at > 4) {
    s_worker_running = false;
  }
  return s_lent && !s_worker_running;
}
void worker_manager_return_ram(void) {
  s_lent = false;
  s_worker_running = true;
  s_worker_close_at = 0;
  prv_scribble();
}
uint32_t worker_manager_launches_refused_ram_lent(void) {
  return s_refused;
}
int hci_router_classic_send_cmd(const uint8_t *cmd, size_t len) {
  return hci_router_classic_cmd(&s_router, cmd, len);
}
struct frag {
  const uint8_t *d;
  size_t l;
};
static bool prv_frag_next(void *it, const uint8_t **data, size_t *len) {
  struct frag *f = it;
  if (!f->d)
    return false;
  *data = f->d;
  *len = f->l;
  f->d = NULL;
  return true;
}
int hci_router_classic_send_acl(const uint8_t *pkt, size_t len) {
  struct frag f = {pkt, len};
  return hci_router_tx_data(&s_router, HCI_ROUTER_H4_ACL, prv_frag_next, &f);
}

/* ---- fake controller ---- */
static uint8_t s_txbuf[2048];
static size_t s_txlen;
#define RSPQ 32
static uint8_t s_rsp[RSPQ][300];
static size_t s_rsp_len[RSPQ];
static int s_rsp_head, s_rsp_tail;
static PBL_SEM_DEFINE(s_rsp_sem, 0, RSPQ);

static void prv_queue_evt(const uint8_t *evt, size_t len) {
  pbl_irq_lock();
  memcpy(s_rsp[s_rsp_tail], evt, len);
  s_rsp_len[s_rsp_tail] = len;
  s_rsp_tail = (s_rsp_tail + 1) % RSPQ;
  pbl_irq_unlock();
  pbl_sem_give(&s_rsp_sem);
}

static void prv_cc(uint16_t op, const uint8_t *ret, size_t n) {
  uint8_t e[300] = {0x04, 0x0E, (uint8_t)(3 + n), 1, op & 0xFF, op >> 8};
  memcpy(&e[6], ret, n);
  prv_queue_evt(e, 6 + n);
}

/* BR/EDR peer ("the phone") */
static const uint8_t s_phone[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
static const uint8_t s_phone_key[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
#define PHONE_HANDLE 0x0081
static bool s_phone_has_key; // the phone keeps its bond
static const uint8_t *s_phone_key_now = s_phone_key;

// The phone's LE pairing (in NimBLE's store), with the sample W of Core Vol 3
// Part H D.8 as LTK, and the link key both sides derive from it (h6 path, from
// an independent AES-CMAC).
static bool s_le_bond;
static const uint8_t s_le_ltk[16] = {0x9b, 0x7d, 0x39, 0x0a, 0xa6, 0x10, 0x10, 0x34,
                                     0x05, 0xad, 0xc8, 0x57, 0xa3, 0x34, 0x02, 0xec};
static const uint8_t s_ctkd_key[16] = {0xd7, 0xf6, 0x2c, 0xc1, 0x8d, 0x9a, 0x95, 0x36,
                                       0xdf, 0x54, 0x55, 0x8c, 0x8d, 0xd9, 0xaa, 0xac};

#include "fake_ag.h"

#include <host/ble_store.h>

static bool prv_le_bond_match(const struct ble_store_key_sec *key) {
  const bool any = key->peer_addr.type == 0 && memcmp(key->peer_addr.val, (uint8_t[6]){0}, 6) == 0;
  return s_le_bond && key->idx == 0 &&
         (any ||
          (key->peer_addr.type == BLE_ADDR_PUBLIC && memcmp(key->peer_addr.val, s_phone, 6) == 0));
}

// The peer's entry has no LTK here, ours has it.
int ble_store_read_peer_sec(const struct ble_store_key_sec *key, struct ble_store_value_sec *v) {
  if (!prv_le_bond_match(key)) {
    return 5; // BLE_HS_ENOENT
  }
  *v = (struct ble_store_value_sec){.sc = 1, .authenticated = 1, .irk_present = 1};
  memcpy(v->peer_addr.val, s_phone, 6);
  return 0;
}

int ble_store_read_our_sec(const struct ble_store_key_sec *key, struct ble_store_value_sec *v) {
  if (!prv_le_bond_match(key)) {
    return 5;
  }
  *v = (struct ble_store_value_sec){.sc = 1, .authenticated = 1, .ltk_present = 1};
  memcpy(v->peer_addr.val, s_phone, 6);
  memcpy(v->ltk, s_le_ltk, 16);
  return 0;
}
static bool s_phone_connected;
static int s_pairings, s_key_replies, s_encryptions;

static void prv_evt(uint8_t code, const uint8_t *p, size_t n) {
  uint8_t e[300] = {0x04, code, (uint8_t)n};
  memcpy(&e[3], p, n);
  prv_queue_evt(e, 3 + n);
}

static void prv_cs(uint16_t op, uint8_t status) {
  const uint8_t p[] = {status, 1, op & 0xFF, op >> 8};
  prv_evt(0x0F, p, sizeof(p));
}

static void prv_cc_addr(uint16_t op) {
  uint8_t r[7] = {0};
  memcpy(&r[1], s_phone, 6);
  prv_cc(op, r, sizeof(r));
}

static int s_sniff_changes;
// The phone refuses a sniff interval over 840 ms (on some rounds).
static bool s_refuse_long_sniff;

static bool prv_br_cmd(uint16_t op, const uint8_t *c) {
  const uint8_t *p = &c[3];
  uint8_t e[32] = {0};
  switch (op) {
    case 0x0405: // Create Connection
      fake_ag_reset();
      prv_cs(op, 0);
      e[0] = 0;
      e[1] = PHONE_HANDLE & 0xFF;
      e[2] = PHONE_HANDLE >> 8;
      memcpy(&e[3], s_phone, 6);
      e[9] = 1;
      e[10] = 0;
      prv_evt(0x03, e, 11);
      s_phone_connected = true;
      return true;
    case 0x041B: // Read Remote Supported Features
      prv_cs(op, 0);
      e[0] = 0;
      e[1] = p[0];
      e[2] = p[1];
      memcpy(&e[3], (const uint8_t[]){0xff, 0xff, 0x8f, 0xfe, 0xdb, 0xff, 0x5b, 0x87}, 8);
      prv_evt(0x0B, e, 11);
      return true;
    case 0x041C: // Read Remote Extended Features
      prv_cs(op, 0);
      e[0] = 0;
      e[1] = p[0];
      e[2] = p[1];
      e[3] = p[2];
      e[4] = 2;
      e[5] = (p[2] == 1) ? 0x0f : 0;
      prv_evt(0x23, e, 13);
      return true;
    case 0x0419: // Remote Name Request
      prv_cs(op, 0);
      return true;
    case 0x0803: // Sniff Mode
    case 0x0804: // Exit Sniff Mode
      prv_cs(op, 0);
      if (op == 0x0803 && s_refuse_long_sniff && (p[2] | p[3] << 8) > 0x0540) {
        // Unsupported LMP parameter value: the link stays active.
        e[0] = 0x20;
        e[1] = p[0];
        e[2] = p[1];
        prv_evt(0x14, e, 6);
        return true;
      }
      e[0] = 0;
      e[1] = p[0];
      e[2] = p[1];
      e[3] = op == 0x0803 ? 2 : 0;
      e[4] = op == 0x0803 ? p[2] : 0; // the max interval
      e[5] = op == 0x0803 ? p[3] : 0;
      s_sniff_changes++;
      prv_evt(0x14, e, 6);
      return true;
    case 0x0411: // Authentication Requested
      prv_cs(op, 0);
      prv_evt(0x17, s_phone, 6); // Link Key Request
      return true;
    case 0x040B: // Link Key Request Reply
      prv_cc_addr(op);
      s_key_replies++;
      if (s_phone_has_key && memcmp(&p[6], s_phone_key_now, 16) == 0) {
        e[0] = 0;
        e[1] = PHONE_HANDLE & 0xFF;
        e[2] = PHONE_HANDLE >> 8;
        prv_evt(0x06, e, 3); // Authentication Complete
      } else {
        e[0] = 0x06;
        e[1] = PHONE_HANDLE & 0xFF;
        e[2] = PHONE_HANDLE >> 8; // PIN or key missing
        prv_evt(0x06, e, 3);
      }
      return true;
    case 0x040C: // Link Key Request Negative Reply
      prv_cc_addr(op);
      prv_evt(0x31, s_phone, 6); // IO Capability Request
      return true;
    case 0x042B: // IO Capability Request Reply
      printf("  watch io_cap %u authreq 0x%02x\n", p[6], p[8]);
      prv_cc_addr(op);
      memcpy(e, s_phone, 6);
      e[6] = 1;
      e[7] = 0;
      e[8] = 0x05; // DisplayYesNo, MITM general bonding
      prv_evt(0x32, e, 9);
      memcpy(e, s_phone, 6);
      e[6] = 0x40;
      e[7] = 0xE2;
      e[8] = 0x01;
      e[9] = 0; // 123456
      prv_evt(0x33, e, 10);
      return true;
    case 0x042C: // User Confirmation Request Reply
      prv_cc_addr(op);
      s_pairings++;
      e[0] = 0;
      memcpy(&e[1], s_phone, 6);
      prv_evt(0x36, e, 7); // Simple Pairing Complete
      memcpy(e, s_phone, 6);
      memcpy(&e[6], s_phone_key, 16);
      e[22] = 0x08;         // auth P-256
      prv_evt(0x18, e, 23); // Link Key Notification
      s_phone_has_key = true;
      e[0] = 0;
      e[1] = PHONE_HANDLE & 0xFF;
      e[2] = PHONE_HANDLE >> 8;
      prv_evt(0x06, e, 3);
      return true;
    case 0x042D: // User Confirmation Request Negative Reply
      prv_cc_addr(op);
      e[0] = 0x05;
      memcpy(&e[1], s_phone, 6);
      prv_evt(0x36, e, 7);
      return true;
    case 0x0413: // Set Connection Encryption
      prv_cs(op, 0);
      s_encryptions++;
      e[0] = 0;
      e[1] = p[0];
      e[2] = p[1];
      e[3] = p[2];
      prv_evt(0x08, e, 4);
      return true;
    case 0x1408: { // Read Encryption Key Size
      uint8_t r[4] = {0, p[0], p[1], 16};
      prv_cc(op, r, 4);
      return true;
    }
    case 0x0429: // Accept Synchronous Connection Request
      printf("  watch accepts SCO: voice %02x%02x pkt types %02x%02x\n", p[17], p[16], p[20],
             p[19]);
      prv_cs(op, 0);
      fake_ag_sco_accepted();
      return true;
    case 0x0428: // Setup Synchronous Connection: the phone takes it
      prv_cs(op, 0);
      fake_ag_sco_accepted();
      return true;
    case 0x0406: // Disconnect
      if ((p[0] | (p[1] << 8)) == PHONE_HANDLE) {
        fake_ag_link_down();
      } else if ((p[0] | (p[1] << 8)) == FAKE_AG_SCO_HANDLE) {
        fake_ag_sco_closed();
      }
      prv_cs(op, 0);
      e[0] = 0;
      e[1] = p[0];
      e[2] = p[1];
      e[3] = 0x16;
      prv_evt(0x05, e, 4);
      s_phone_connected = false;
      return true;
    default:
      return false;
  }
}

static void prv_controller_cmd(const uint8_t *c, size_t len) {
  uint16_t op = c[0] | (c[1] << 8);
  uint8_t r[80] = {0};
  size_t n = 1; // status only
  printf("  ctrl <- cmd %04x plen %u\n", op, c[2]);
  if (prv_br_cmd(op, c)) {
    return;
  }
  switch (op) {
    case 0x1001: {
      uint8_t v[] = {0, 0x0C, 0, 0, 0x0C, 0x0a, 0x4c, 0x8a, 0x03};
      memcpy(r, v, 9);
      n = 9;
      break;
    }
    case 0x1002:
      memset(r + 1, 0xFF, 64);
      n = 65;
      break;
    case 0x1003: {
      uint8_t f[] = {0, 0xff, 0xff, 0x8f, 0xfe, 0xdb, 0xff, 0x5b, 0x87};
      memcpy(r, f, 9);
      n = 9;
      break;
    }
    case 0x1004:
      r[1] = c[3];
      r[2] = 2;
      if (c[3] == 1)
        r[3] = 0x0f;
      n = 11;
      break;
    case 0x1005: {
      uint8_t b[] = {0, 0xfd, 0x03, 0xff, 4, 0, 4, 0};
      memcpy(r, b, 8);
      n = 8;
      break;
    }
    case 0x1009: {
      uint8_t a[] = {0, 0xac, 0x27, 0xee, 0x09, 0x93, 0xd8};
      memcpy(r, a, 7);
      n = 7;
      break;
    }
    case 0x080F:
      n = 3;
      break;     // read default link policy
    case 0x080C: // Read Link Policy Settings
      r[1] = c[3];
      r[2] = c[4];
      n = 5;
      break;
    case 0x2002:
      r[1] = 251;
      r[3] = 8;
      n = 4;
      break;
    case 0x2060:
      r[1] = 251;
      r[3] = 8;
      n = 7;
      break;
    case 0x2003:
      r[1] = 0xff;
      n = 9;
      break;
    case 0x201C:
      memset(r + 1, 0xff, 8);
      n = 9;
      break;
    case 0x202F:
      r[1] = 251;
      r[3] = 0x48;
      r[4] = 0x08;
      r[5] = 251;
      r[7] = 0x48;
      r[8] = 0x08;
      n = 9;
      break;
    case 0x2087:
      n = 1 + 1 + 248;
      break; // LE Read All Local Features
    case 0x0C23:
      r[1] = 0x04;
      r[2] = 0x07;
      r[3] = 0x20;
      n = 4;
      break; // Read CoD
    default:
      // Writes: status only.
      break;
  }
  prv_cc(op, r, n);
}

static int prv_tx_write(void *ctx, const uint8_t *data, size_t len) {
  memcpy(&s_txbuf[s_txlen], data, len);
  s_txlen += len;
  // Parse complete H4 packets.
  for (;;) {
    if (s_txlen == 0)
      return 0;
    if (s_txbuf[0] == 0x01) {
      if (s_txlen < 4 || s_txlen < 4U + s_txbuf[3])
        return 0;
      size_t pl = 4U + s_txbuf[3];
      prv_controller_cmd(&s_txbuf[1], pl - 1);
      memmove(s_txbuf, &s_txbuf[pl], s_txlen - pl);
      s_txlen -= pl;
    } else if (s_txbuf[0] == 0x02) {
      if (s_txlen < 5)
        return 0;
      size_t pl = 5U + (s_txbuf[3] | (s_txbuf[4] << 8));
      if (s_txlen < pl)
        return 0;
      printf("  ctrl <- acl len %zu\n", pl);
      fake_ag_acl(&s_txbuf[1], pl - 1);
      // The packet is out: Number Of Completed Packets.
      const uint8_t nocp[] = {1, s_txbuf[1], s_txbuf[2] & 0x0F, 1, 0};
      prv_evt(0x13, nocp, sizeof(nocp));
      memmove(s_txbuf, &s_txbuf[pl], s_txlen - pl);
      s_txlen -= pl;
    } else {
      printf("  ctrl <- bad type %02x\n", s_txbuf[0]);
      abort();
    }
  }
}

/* "NimbleHCI": delivers controller output through the router */
static void prv_hci_thread(void *arg) {
  for (;;) {
    pbl_sem_take(&s_rsp_sem, PBL_FOREVER);
    uint8_t *e = s_rsp[s_rsp_head];
    size_t l = s_rsp_len[s_rsp_head];
    s_rsp_head = (s_rsp_head + 1) % RSPQ;
    hci_router_rx(&s_router, e, l);
  }
}

static const char *const s_states[] = {"OFF", "STARTING", "READY", "FAILED", "HALTED", "STOPPING"};

static void prv_print_status(const char *tag) {
  struct zbt_status st;
  zbt_get_status(&st);
  printf("[%s] loops BT %lu RX %lu\n", tag, (unsigned long)st.exec_loops,
         (unsigned long)st.rx_items);
  printf(
      "[%s] %s err %d ram %s %lu wait %lu start %u stop %u stoperr %d disc %d/%d "
      "rx %lu/%lu drop %lu tx %lu/%lu stack %u/%u %u/%u fatal %s:%d\n",
      tag, s_states[st.state], st.enable_err, st.ram_held ? "HELD" : "free",
      (unsigned long)st.ram_used, (unsigned long)st.worker_wait_ms, st.starts, st.stops,
      st.stop_err, st.discoverable, st.discoverable_err, (unsigned long)st.rx_evt,
      (unsigned long)st.rx_acl, (unsigned long)st.rx_drop, (unsigned long)st.tx_cmd,
      (unsigned long)st.tx_acl, st.exec_stack.used, st.exec_stack.size, st.rx_stack.used,
      st.rx_stack.size, st.fatal_file ? st.fatal_file : "-", st.fatal_line);
}

static bool prv_wait_state(enum zbt_state want, int ms) {
  for (int t = 0; t < ms; t += 50) {
    struct zbt_status st;
    zbt_get_status(&st);
    if (st.state == want)
      return true;
    pbl_thread_sleep(PBL_MSEC(50));
  }
  return false;
}

static int s_fail;
#define CHECK(c)                                                 \
  do {                                                           \
    if (!(c)) {                                                  \
      printf("CHECK FAILED %s:%d %s\n", __FILE__, __LINE__, #c); \
      s_fail++;                                                  \
    }                                                            \
  } while (0)

static void prv_app(void *arg) {
  // NimBLE sets its event mask first, as on the watch.
  const uint8_t sem[] = {0x01, 0x0C, 8, 0xff, 0xff, 0xff, 0xff, 0xff, 0x9f, 0x00, 0x20};
  hci_router_tx_cmd(&s_router, HCI_ROUTER_OWNER_NIMBLE, sem, sizeof(sem));
  pbl_thread_sleep(PBL_MSEC(10));

  for (int round = 1; round <= 6; round++) {
    printf("=== round %d\n", round);
    if (round == 5) {
      // The phone and the watch redo the LE pairing with the LinkKey bit; both derive the key.
      s_le_bond = true;
      s_phone_key_now = s_ctkd_key;
    }
    const int pairings_before = s_pairings;
    const int replies_before = s_key_replies;
    const int writes_before = s_file_writes;
    CHECK(zbt_start() == 0);
    CHECK(zbt_start() == -EALREADY);
    CHECK(prv_wait_state(ZBT_STATE_READY, 5000));
    prv_print_status("ready");

    struct zbt_status ls;
    CHECK(zbt_connect(s_phone) == 0);
    bool confirmed = false;
    for (int t = 0; t < 5000; t += 50) {
      zbt_get_status(&ls);
      if (ls.pairing == ZBT_PAIRING_CONFIRM && !confirmed) {
        printf("  passkey %06lu\n", (unsigned long)ls.passkey);
        CHECK(ls.passkey == 123456);
        CHECK(zbt_pairing_reply(true) == 0);
        confirmed = true;
      }
      if (ls.encrypted)
        break;
      pbl_thread_sleep(PBL_MSEC(50));
    }
    zbt_get_status(&ls);
    printf("  link %d err %d pairing %d err %d bonded %d sec %u enc %d secerr %d\n", ls.link,
           ls.link_err, ls.pairing, ls.pairing_err, ls.bonded, ls.security, ls.encrypted,
           ls.security_err);
    CHECK(ls.link == ZBT_LINK_CONNECTED && ls.encrypted);
    printf(
        "  phone: pairings %d key replies %d encryptions %d; watch key loaded %d stored %d "
        "err %d; file %s writes %d\n",
        s_pairings, s_key_replies, s_encryptions, ls.key_loaded, ls.key_stored, ls.key_err,
        s_file_name, s_file_writes);
    printf("  derived %d err %d auth %d\n", ls.key_derived, ls.derive_err, ls.derived_auth);

    // Hands-free: the SLC comes up by itself after encryption, with the phone's indicators.
    for (int t = 0; t < 3000 && ls.hfp != ZBT_HFP_CONNECTED; t += 50) {
      pbl_thread_sleep(PBL_MSEC(50));
      zbt_get_status(&ls);
    }
    printf("  hfp %d err %d ch %u ind %d %d %d %d\n", ls.hfp, ls.hfp_err, ls.hfp_channel,
           ls.ind_service, ls.ind_signal, ls.ind_roam, ls.ind_battery);
    CHECK(ls.hfp == ZBT_HFP_CONNECTED && fake_ag_state()->slc && ls.hfp_channel == 3);
    CHECK(ls.ind_service == 1 && ls.ind_signal == 4 && ls.ind_roam == 0 && ls.ind_battery == 3);
    // Sniff is allowed on the link, and the idle link goes to it (the phone book reading,
    // which finds no server here, leaves it first).
    CHECK(ls.link_policy & 0x0004);
    // The long interval, or the shorter one once the phone refuses it.
    s_refuse_long_sniff = round % 2 == 0;
    for (int t = 0; t < 15000 && ls.link_mode != 2; t += 100) {
      pbl_thread_sleep(PBL_MSEC(100));
      zbt_get_status(&ls);
    }
    printf("  link mode %u interval %u n %u err %d policy %x sniff changes %d\n", ls.link_mode,
           ls.sniff_interval, ls.sniff_count, ls.sniff_err, ls.link_policy, s_sniff_changes);
    CHECK(ls.link_mode == 2 && ls.sniff_err == 0 &&
          ls.sniff_interval == (s_refuse_long_sniff ? 0x0540 : 0x0800));
    if (round >= 5) {
      // A call: ring with the number, answer from the watch (the phone opens the voice link),
      // end it from the watch (the phone closes the voice link).
      fake_ag_incoming_call("+819012345678");
      for (int t = 0; t < 2000 && strcmp(ls.clip, "+819012345678") != 0; t += 50) {
        pbl_thread_sleep(PBL_MSEC(50));
        zbt_get_status(&ls);
      }
      printf("  call %d rings %u clip %s\n", ls.call, ls.rings, ls.clip);
      CHECK(ls.call == ZBT_CALL_INCOMING && ls.rings >= 1 && strcmp(ls.clip, "+819012345678") == 0);
      const uint32_t answers = fake_ag_state()->answers;
      CHECK(zbt_hfp_answer() == 0);
      for (int t = 0; t < 2000 && !(ls.call == ZBT_CALL_ACTIVE && ls.sco); t += 50) {
        pbl_thread_sleep(PBL_MSEC(50));
        zbt_get_status(&ls);
      }
      printf("  answered: call %d err %d sco %d n %u audio %d\n", ls.call, ls.call_err, ls.sco,
             ls.sco_count, s_audio_on);
      // The voice link starts the call's audio.
      CHECK(s_audio_on);
      CHECK(fake_ag_state()->answers == answers + 1 && ls.call == ZBT_CALL_ACTIVE && ls.sco &&
            ls.call_err == 0);
      // The router kept the voice link's parameters: status 0, CVSD.
      CHECK(s_router.sync_conn_valid && s_router.sync_conn[0] == 0 &&
            s_router.sync_conn[16] == 0x02);
      const uint32_t hangups = fake_ag_state()->hangups;
      CHECK(zbt_hfp_hangup() == 0);
      for (int t = 0; t < 2000 && !(ls.call == ZBT_CALL_ENDED && !ls.sco); t += 50) {
        pbl_thread_sleep(PBL_MSEC(50));
        zbt_get_status(&ls);
      }
      printf("  ended: call %d err %d sco %d reason %02x audio %d\n", ls.call, ls.call_err, ls.sco,
             ls.sco_reason, s_audio_on);
      // And its end stops it.
      CHECK(!s_audio_on && s_audio_starts == s_audio_stops);
      CHECK(fake_ag_state()->hangups == hangups + 1 && ls.call == ZBT_CALL_ENDED && !ls.sco);
    }
    if (round >= 5) {
      // The derived key: no pairing. The first time, the key file's (from round 4's pairing) goes
      // first, the phone refuses it, the derived one takes its place and is saved; then it is the
      // key file's, taken at once, nothing written.
      CHECK(ls.key_derived && ls.derive_err == 0 && ls.derived_auth);
      CHECK(memcmp(ls.derived_addr, s_phone, 6) == 0);
      printf("  key use %c swaps %u replies %d\n", ls.key_source, ls.key_swaps,
             s_key_replies - replies_before);
      CHECK(s_pairings == pairings_before && ls.key_loaded);
      CHECK(ls.pairing == ZBT_PAIRING_NONE);
      pbl_thread_sleep(PBL_MSEC(100));
      zbt_get_status(&ls);
      if (round == 5) {
        CHECK(s_key_replies == replies_before + 2 && ls.key_swaps == 1 && ls.key_source == 's');
        CHECK(s_file_writes == writes_before + 1);
      } else {
        CHECK(s_key_replies == replies_before + 1 && ls.key_swaps == 0 && ls.key_source == 's');
        CHECK(s_file_writes == writes_before);
      }
      // The router's log shows Authentication Complete and Encryption Change with status 0.
      struct hci_router_log_entry log[HCI_ROUTER_LOG_LEN];
      const size_t n = hci_router_log_copy(&s_router, log);
      bool auth_ok = false, enc_ok = false;
      for (size_t i = 0; i < n; i++) {
        if (log[i].kind == HCI_ROUTER_LOG_EVT && log[i].code == 0x06) {
          auth_ok = log[i].params[0] == 0;
        }
        if (log[i].kind == HCI_ROUTER_LOG_EVT && log[i].code == 0x08) {
          enc_ok = log[i].params[0] == 0;
        }
      }
      CHECK(auth_ok && enc_ok);

      // Android's virtual call (a recorded call played back), which the phone lists active with
      // its own number (AT+CNUM's): its voice link is left alone; kept on the phone, the watch
      // drops it; moved back to the watch's speaker, the watch asks for it.
      const uint16_t sco_before = ls.sco_count;
      fake_ag_outgoing_call();
      for (int t = 0; t < 2000 && !(ls.call == ZBT_CALL_ACTIVE && ls.sco && s_audio_on); t += 50) {
        pbl_thread_sleep(PBL_MSEC(50));
        zbt_get_status(&ls);
      }
      printf("  set up by the phone: call %d here %d on phone %d sco %d n %u audio %d\n", ls.call,
             ls.dialed_here, ls.audio_on_phone, ls.sco, ls.sco_count, s_audio_on);
      CHECK(ls.call == ZBT_CALL_ACTIVE && !ls.dialed_here && !ls.audio_on_phone && ls.sco &&
            ls.sco_count > sco_before && s_audio_on);
      // A virtual call, no call to show.
      for (int t = 0; t < 2000 && !ls.call_virtual; t += 50) {
        pbl_thread_sleep(PBL_MSEC(50));
        zbt_get_status(&ls);
      }
      CHECK(ls.call_virtual);
      if (round == 5) {
        // Still one, its audio still here, after the next look at the calls (every 10 s).
        pbl_thread_sleep(PBL_MSEC(11000));
        zbt_get_status(&ls);
        printf("  virtual after 11 s: %d on phone %d sco %d\n", ls.call_virtual, ls.audio_on_phone,
               ls.sco);
        CHECK(ls.call_virtual && !ls.audio_on_phone && ls.sco && s_audio_on);
      }
      CHECK(zbt_hfp_audio_on_watch(false) == 0);
      for (int t = 0; t < 2000 && (ls.sco || s_audio_on || fake_ag_state()->sco); t += 50) {
        pbl_thread_sleep(PBL_MSEC(50));
        zbt_get_status(&ls);
      }
      printf("  kept on the phone: on phone %d sco %d audio %d\n", ls.audio_on_phone, ls.sco,
             s_audio_on);
      CHECK(ls.audio_on_phone && !ls.sco && !fake_ag_state()->sco && !s_audio_on);
      CHECK(zbt_hfp_audio_on_watch(true) == 0);
      for (int t = 0; t < 2000 && !(ls.sco && s_audio_on); t += 50) {
        pbl_thread_sleep(PBL_MSEC(50));
        zbt_get_status(&ls);
      }
      printf("  moved to the watch: on phone %d sco %d err %d audio %d\n", ls.audio_on_phone,
             ls.sco, ls.call_err, s_audio_on);
      CHECK(!ls.audio_on_phone && ls.sco && fake_ag_state()->sco && s_audio_on);
      CHECK(zbt_hfp_hangup() == 0);
      for (int t = 0; t < 2000 && !(ls.call == ZBT_CALL_ENDED && !ls.sco); t += 50) {
        pbl_thread_sleep(PBL_MSEC(50));
        zbt_get_status(&ls);
      }
      CHECK(ls.call == ZBT_CALL_ENDED && !ls.audio_on_phone && !ls.dialed_here);

      // A call dialed on the phone, which the phone lists with its number: talked on the phone,
      // which takes the voice link back (it stays there once answered).
      fake_ag_outgoing_ringing();
      for (int t = 0; t < 4000 && !(ls.call == ZBT_CALL_ALERTING && ls.audio_on_phone && !ls.sco &&
                                    !fake_ag_state()->sco && !s_audio_on);
           t += 50) {
        pbl_thread_sleep(PBL_MSEC(50));
        zbt_get_status(&ls);
      }
      printf("  dialed on the phone: call %d here %d on phone %d sco %d audio %d clip %s\n",
             ls.call, ls.dialed_here, ls.audio_on_phone, ls.sco, s_audio_on, ls.clip);
      CHECK(ls.call == ZBT_CALL_ALERTING && !ls.dialed_here && ls.audio_on_phone && !ls.sco &&
            !fake_ag_state()->sco && !s_audio_on && strcmp(ls.clip, "0312345678") == 0);
      fake_ag_remote_answer();
      for (int t = 0; t < 2000 && ls.call != ZBT_CALL_ACTIVE; t += 50) {
        pbl_thread_sleep(PBL_MSEC(50));
        zbt_get_status(&ls);
      }
      pbl_thread_sleep(PBL_MSEC(200));
      zbt_get_status(&ls);
      CHECK(ls.call == ZBT_CALL_ACTIVE && ls.audio_on_phone && !ls.sco && !s_audio_on);
      CHECK(zbt_hfp_hangup() == 0);
      for (int t = 0; t < 2000 && ls.call != ZBT_CALL_ENDED; t += 50) {
        pbl_thread_sleep(PBL_MSEC(50));
        zbt_get_status(&ls);
      }
      CHECK(ls.call == ZBT_CALL_ENDED && !ls.audio_on_phone);

      if (round == 6) {
        // A call the phone stops listing, without saying so: it ends here at the next check.
        fake_ag_outgoing_call();
        for (int t = 0; t < 2000 && ls.call != ZBT_CALL_ACTIVE; t += 50) {
          pbl_thread_sleep(PBL_MSEC(50));
          zbt_get_status(&ls);
        }
        fake_ag_forget_call();
        for (int t = 0; t < 15000 && ls.call != ZBT_CALL_ENDED; t += 100) {
          pbl_thread_sleep(PBL_MSEC(100));
          zbt_get_status(&ls);
        }
        printf("  no longer listed: call %d virtual %d\n", ls.call, ls.call_virtual);
        CHECK(ls.call == ZBT_CALL_ENDED && !ls.call_virtual);
        fake_ag_close_sco();
      }

      // A ring answered on the phone: talked there (the watch drops the voice link the phone
      // opens all the same).
      fake_ag_incoming_call("+819011112222");
      for (int t = 0; t < 2000 && ls.call != ZBT_CALL_INCOMING; t += 50) {
        pbl_thread_sleep(PBL_MSEC(50));
        zbt_get_status(&ls);
      }
      fake_ag_answer_on_phone();
      for (int t = 0; t < 2000 && !(ls.call == ZBT_CALL_ACTIVE && ls.audio_on_phone && !ls.sco &&
                                    !fake_ag_state()->sco);
           t += 50) {
        pbl_thread_sleep(PBL_MSEC(50));
        zbt_get_status(&ls);
      }
      pbl_thread_sleep(PBL_MSEC(100));
      zbt_get_status(&ls);
      printf("  answered on the phone: call %d on phone %d sco %d audio %d\n", ls.call,
             ls.audio_on_phone, ls.sco, s_audio_on);
      CHECK(ls.call == ZBT_CALL_ACTIVE && ls.audio_on_phone && !ls.sco && !fake_ag_state()->sco &&
            !s_audio_on);
      CHECK(zbt_hfp_hangup() == 0);
      for (int t = 0; t < 2000 && ls.call != ZBT_CALL_ENDED; t += 50) {
        pbl_thread_sleep(PBL_MSEC(50));
        zbt_get_status(&ls);
      }
      CHECK(ls.call == ZBT_CALL_ENDED && !ls.audio_on_phone);
    } else if (round == 1 || round == 4) {
      CHECK(!ls.key_derived && ls.derive_err == -ENOENT);
      // First time, and after the keys were cleared: a new pairing, then the key is saved.
      CHECK(s_pairings == pairings_before + 1 && !ls.key_loaded);
      pbl_thread_sleep(PBL_MSEC(100));
      zbt_get_status(&ls);
      CHECK(ls.key_stored && s_file_has);
    } else {
      // Reconnection with the stored key: no pairing, no new write.
      CHECK(s_pairings == pairings_before && s_key_replies == replies_before + 1 && ls.key_loaded);
      CHECK(ls.pairing == ZBT_PAIRING_NONE);
    }
    if (round == 3) {
      CHECK(zbt_clear_keys() == 0);
      pbl_thread_sleep(PBL_MSEC(100));
      zbt_get_status(&ls);
      CHECK(!ls.key_stored && !s_file_has);
    }
    CHECK(zbt_set_discoverable("PT2-ZEPHYR", 0x200704) == 0);
    pbl_thread_sleep(PBL_MSEC(500));
    prv_print_status("disc");
    struct zbt_status st;
    zbt_get_status(&st);
    CHECK(st.discoverable && st.discoverable_err == 0);
    CHECK(zbt_stop() == 0);
    CHECK(prv_wait_state(ZBT_STATE_OFF, 20000));
    prv_print_status("off");
    zbt_get_status(&st);
    CHECK(!st.ram_held && st.stops == round && st.starts == round && st.stop_err == 0);
    CHECK(!s_phone_connected);
    CHECK(zbt_stop() == -EALREADY);
  }
  struct hci_router_stats rs = s_router.stats;
  printf("router: pass %lu merge %lu fake %lu reject %lu txerr %lu ccx %lu drop_classic %lu\n",
         (unsigned long)rs.classic_cmd_passed, (unsigned long)rs.classic_cmd_merged,
         (unsigned long)rs.classic_cmd_faked, (unsigned long)rs.classic_cmd_rejected,
         (unsigned long)rs.tx_errors, (unsigned long)rs.cc_unmatched,
         (unsigned long)rs.drop_classic);
  printf(s_fail ? "SIM FAILED (%d)\n" : "SIM PASSED\n", s_fail);
  pbl_test_kernel_stop();
}

uint8_t sim_wdata_image[8192];

static uint8_t s_stk[5][16384] __attribute__((aligned(8)));
static struct pbl_thread s_thr[5];

static void prv_spawn(int i, const char *name, pbl_prio_t prio, void (*entry)(void *)) {
  struct pbl_thread_attr a = {
    .name = name,
    .entry = entry,
    .prio = prio,
    .privileged = true,
    .stack = s_stk[i],
    .stack_size = sizeof(s_stk[i])
  };
  pbl_thread_create(&s_thr[i], &a);
}

int main(void) {
  setvbuf(stdout, NULL, _IONBF, 0);
  // The initial values of the host's data, as the firmware keeps them in flash.
  if ((size_t)(__zbt_wdata_end - __zbt_wdata_start) > sizeof(sim_wdata_image)) {
    printf("SIM FAILED: host data %u B, sim_wdata_image %u B\n",
           (unsigned)(__zbt_wdata_end - __zbt_wdata_start), (unsigned)sizeof(sim_wdata_image));
    return 1;
  }
  memcpy(sim_wdata_image, __zbt_wdata_start, __zbt_wdata_end - __zbt_wdata_start);
  prv_scribble();
  hci_router_init(&s_router, &s_ops, 70);
  fake_ag_init(prv_queue_evt, PHONE_HANDLE, s_phone);
  prv_spawn(0, "NimbleHCI", PBL_PRIO_IDLE + 3, prv_hci_thread);
  prv_spawn(1, "KernelMain", PBL_PRIO_IDLE + 3, prv_main_thread);
  prv_spawn(2, "NewTimers", PBL_PRIO_MAX, prv_timer_thread);
  prv_spawn(3, "App", PBL_PRIO_IDLE + 2, prv_app);
  prv_spawn(4, "KernelBG", PBL_PRIO_IDLE + 1, prv_bg_thread);
  pbl_test_kernel_run();
  printf("done\n");
  return s_fail != 0;
}
