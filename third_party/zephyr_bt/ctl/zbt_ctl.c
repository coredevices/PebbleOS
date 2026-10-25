/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// Starting and stopping the Zephyr host. Everything of the host (Zephyr and
// port/) lives in its own sections of the kernel's RAM (linker/zbt_ram.ld),
// set up afresh at each start; workers keep the worker RAM meanwhile.
//
// Start: set the host's RAM up (KernelMain), start the host.
// Stop: the host disconnects and stops scanning, the HCI gate closes, the host
// threads exit, the router port is detached and the RAM is given back.

#include "zbt_call_log.h"
#include "zbt_ctkd.h"
#include "zbt_link.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#ifdef UNITTEST
#include <stdlib.h>
#endif

#ifdef CONFIG_BT_CALL_AUDIO
#include <pbl/bluetooth/call_audio.h>
#endif
#include <pbl/bluetooth/hci_router.h>
#include <pbl/drivers/battery.h>
#include <pbl/drivers/rtc.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/thread.h>
#include <pbl/services/bluetooth/local_id.h>
#include <pbl/services/comm_session/session.h>
#include <pbl/services/new_timer/new_timer.h>
#include <pbl/services/settings/settings_file.h>
#include <pbl/services/speaker/speaker_service.h>
#include <pbl/services/system_task.h>

#include <host/ble_store.h>
#ifdef CONFIG_DEMO_APP_DIALER
#include <apps/demo/dialer/call_indicator.h>
#endif
#include <kernel/event_loop.h>
#include <kernel/memory_layout.h>
#include <kernel/pebble_tasks.h>
#include <system/status_codes.h>
#ifdef CONFIG_REBOOT_REASON_KEEP_LAST
#include <system/die.h>
#include <system/reboot_reason.h>
#endif

// Disconnecting takes at most 3 s (port), then the host threads exit; a command
// in flight can hold the Zephyr thread for its 10 s timeout.
#define QUIESCE_WAIT_MS 4000
#define STOP_WAIT_MS    15000
#define POLL_MS         100

// The bonded phone's link key. Only zbt_start(), zbt_clear_keys() and a new
// key touch the file, never the boot path.
#define KEY_FILE      "bt_classic_keys"
#define KEY_FILE_SIZE 512
#define KEY_VERSION   1
static const char KEY_NAME[] = "LINK_KEY";

// LE pairings looked at for the derived key.
#define LE_BONDS_MAX 16

struct key_record {
  uint8_t version;
  uint8_t addr[6];
  uint8_t len;
  uint8_t data[ZBT_LINK_KEY_MAX];
};

extern uint8_t __zbt_wdata_start[];
extern uint8_t __zbt_wdata_end[];
extern uint8_t __zbt_wdata_load_start[];
extern uint8_t __zbt_wbss_start[];
extern uint8_t __zbt_wbss_end[];

enum phase {
  PHASE_IDLE,
  PHASE_SETUP,
  PHASE_RUN,
  PHASE_STOP,
};

static struct zbt_status s_status;
// The battery's current while the host is off (since its first start); s_status restarts with
// each start.
static int64_t s_bat_off_sum;
static int32_t s_bat_off_ua;
static uint32_t s_bat_off_samples;
static enum phase s_phase;
#ifdef CONFIG_BT_CALL_AUDIO
// Whether the call's voice link is up, as the host last said.
static volatile bool s_sco_up;
#endif
static TimerID s_timer = TIMER_INVALID_ID;
static uint32_t s_elapsed_ms;
static bool s_poll_queued;
static bool s_quit_sent;

// Key read at start, given to the host; key waiting to be written.
static struct zbt_link_key s_key;
static struct zbt_link_key s_key_to_save;
// Key derived from the LE pairing at start, given to the host instead.
static struct zbt_derived_key s_derived;
// The watch's name, as LE advertises it ("Pebble XXXX"), for BR/EDR too.
static char s_name[PBL_BT_DEVICE_NAME_BUFFER_SIZE];

// HCI receive gate: the router's receive thread only enters the host while it
// is open, and stop waits for it to leave.
static bool s_rx_open;
static uint32_t s_rx_busy;

static uint32_t prv_ram_used(void) {
  return (uint32_t)(__zbt_wbss_end - __zbt_wdata_start);
}

/* HCI receive gate -----------------------------------------------------------*/

static void prv_rx(const uint8_t *pkt, size_t len, bool evt) {
  pbl_irq_lock();
  const bool open = s_rx_open;
  if (open) {
    s_rx_busy++;
  } else {
    s_status.rx_drop++;
  }
  pbl_irq_unlock();
  if (!open) {
    return;
  }

  zbt_host_rx(pkt, len, evt);

  pbl_irq_lock();
  s_rx_busy--;
  pbl_irq_unlock();
}

static void prv_rx_evt(void *ctx, const uint8_t *pkt, size_t len) {
  prv_rx(pkt, len, true);
}

static void prv_rx_acl(void *ctx, const uint8_t *pkt, size_t len) {
  prv_rx(pkt, len, false);
}

int zbt_ctl_hci_open(void) {
  pbl_irq_lock();
  s_rx_open = true;
  pbl_irq_unlock();
  return hci_router_classic_attach(prv_rx_evt, prv_rx_acl, NULL);
}

// Returns once the receive thread is out of the host.
static bool prv_rx_close(void) {
  pbl_irq_lock();
  s_rx_open = false;
  const bool idle = s_rx_busy == 0;
  pbl_irq_unlock();
  return idle;
}

/* Key file, on KernelBG ------------------------------------------------------*/

static status_t prv_key_file_open(SettingsFile *file) {
  return settings_file_open(file, KEY_FILE, KEY_FILE_SIZE);
}

static void prv_load_key(void) {
  s_key = (struct zbt_link_key){};
  SettingsFile file;
  status_t rc = prv_key_file_open(&file);
  if (rc == S_SUCCESS) {
    struct key_record rec;
    if (settings_file_get_len(&file, KEY_NAME, sizeof(KEY_NAME)) == sizeof(rec)) {
      rc = settings_file_get(&file, KEY_NAME, sizeof(KEY_NAME), &rec, sizeof(rec));
      if (rc == S_SUCCESS && rec.version == KEY_VERSION && rec.len <= ZBT_LINK_KEY_MAX) {
        s_key.valid = true;
        memcpy(s_key.addr, rec.addr, sizeof(s_key.addr));
        s_key.len = rec.len;
        memcpy(s_key.data, rec.data, rec.len);
      }
    }
    settings_file_close(&file);
  }
  s_status.key_stored = s_key.valid;
  s_status.key_err = rc;
}

/* Key derived from the LE pairing (CTKD), on KernelBG ------------------------*/

// The LTK of the LE pairing with @p addr: the peer's entry, or ours (Secure
// Connections has one LTK, which either may hold).
static bool prv_read_ltk(const ble_addr_t *addr, struct ble_store_value_sec *sec) {
  const struct ble_store_key_sec key = {.peer_addr = *addr};
  if (ble_store_read_peer_sec(&key, sec) == 0 && sec->ltk_present) {
    return true;
  }
  return ble_store_read_our_sec(&key, sec) == 0 && sec->ltk_present;
}

// The phone is the one LE pairing made with Secure Connections; its identity
// address must be public, as that is its BR/EDR address. NimBLE never sets
// CT2 in its AuthReq, so the agreed CT2 is 0 and the key comes from h6.
static void prv_derive_key(void) {
  s_derived = (struct zbt_derived_key){};
  struct ble_store_value_sec sec;
  struct ble_store_value_sec found = {};
  int found_n = 0;
  bool random_only = false;
  for (uint8_t idx = 0; idx < LE_BONDS_MAX; idx++) {
    const struct ble_store_key_sec key = {.peer_addr = *BLE_ADDR_ANY, .idx = idx};
    if (ble_store_read_peer_sec(&key, &sec) != 0) {
      break;
    }
    if (!prv_read_ltk(&sec.peer_addr, &sec) || !sec.sc) {
      continue;
    }
    if (sec.peer_addr.type != BLE_ADDR_PUBLIC) {
      random_only = true;
      continue;
    }
    found = sec;
    found_n++;
  }

  int rc = 0;
  if (found_n == 0) {
    rc = random_only ? -EADDRNOTAVAIL : -ENOENT;
  } else if (found_n > 1) {
    rc = -E2BIG;
  } else {
    rc = zbt_ctkd_link_key(found.ltk, false /* ct2 */, s_derived.val);
  }
  if (rc == 0) {
    s_derived.valid = true;
    s_derived.authenticated = found.authenticated;
    memcpy(s_derived.addr, found.peer_addr.val, sizeof(s_derived.addr));
    memcpy(s_status.derived_addr, s_derived.addr, sizeof(s_status.derived_addr));
  }
  s_status.derive_err = rc;
  s_status.key_derived = s_derived.valid;
  s_status.derived_auth = s_derived.authenticated;
  memset(&found, 0, sizeof(found));
  memset(&sec, 0, sizeof(sec));
}

static void prv_save_key_bg(void *data) {
  struct key_record rec = {
    .version = KEY_VERSION,
    .len = s_key_to_save.len,
  };
  memcpy(rec.addr, s_key_to_save.addr, sizeof(rec.addr));
  memcpy(rec.data, s_key_to_save.data, s_key_to_save.len);

  SettingsFile file;
  status_t rc = prv_key_file_open(&file);
  if (rc == S_SUCCESS) {
    rc = settings_file_set(&file, KEY_NAME, sizeof(KEY_NAME), &rec, sizeof(rec));
    settings_file_close(&file);
  }
  if (rc == S_SUCCESS) {
    s_key = s_key_to_save;
  }
  s_status.key_stored = rc == S_SUCCESS;
  s_status.key_err = rc;
}

static void prv_clear_keys_bg(void *data) {
  SettingsFile file;
  status_t rc = prv_key_file_open(&file);
  if (rc == S_SUCCESS) {
    if (settings_file_exists(&file, KEY_NAME, sizeof(KEY_NAME))) {
      rc = settings_file_delete(&file, KEY_NAME, sizeof(KEY_NAME));
    }
    settings_file_close(&file);
  }
  s_key = (struct zbt_link_key){};
  s_status.key_stored = false;
  s_status.key_loaded = false;
  s_status.key_err = rc;
}

/* Polling on KernelMain --------------------------------------------------------*/

static void prv_poll_main(void *data);

static void prv_timer_cb(void *data) {
  s_elapsed_ms += POLL_MS;
  if (!s_poll_queued) {
    s_poll_queued = true;
    launcher_task_add_callback(prv_poll_main, NULL);
  }
}

static void prv_poll_start(void) {
  if (s_timer == TIMER_INVALID_ID) {
    s_timer = new_timer_create();
  }
  s_elapsed_ms = 0;
  new_timer_start(s_timer, POLL_MS, prv_timer_cb, NULL, TIMER_START_FLAG_REPEATING);
}

static void prv_poll_stop(void) {
  new_timer_stop(s_timer);
}

// The host threads' MPU regions: those of a kernel task (pebble_tasks.c), so
// they see the app and worker RAM as every other kernel task does. The host's
// own RAM is the kernel's, which no context caches.
static MpuRegion s_app_region;
static MpuRegion s_worker_region;
static struct zbt_host_threads s_threads;

// Runs on KernelMain.
static void prv_take_ram(void) {
  memset(__zbt_wbss_start, 0, __zbt_wbss_end - __zbt_wbss_start);
  memcpy(__zbt_wdata_start, __zbt_wdata_load_start, __zbt_wdata_end - __zbt_wdata_start);
  s_status.ram_held = true;
  s_status.starts++;
  s_phase = PHASE_RUN;

  mpu_init_region_from_region(&s_app_region, memory_layout_get_app_region(),
                              false /* allow_user_access */);
  mpu_init_region_from_region(&s_worker_region, memory_layout_get_worker_region(),
                              false /* allow_user_access */);
  const MpuRegion *const regions[PBL_THREAD_MAX_MEM_REGIONS] = {&s_app_region, &s_worker_region};
  const int rc = zbt_host_start(s_key.valid ? &s_key : NULL, s_derived.valid ? &s_derived : NULL,
                                s_name, regions, &s_threads);
  if (rc != 0) {
    s_status.enable_err = rc;
    s_status.state = ZBT_STATE_FAILED;
  }
}

static void prv_release_ram(void) {
#ifdef CONFIG_BT_CALL_AUDIO
  s_sco_up = false;
  call_audio_stop();
#endif
  (void)hci_router_classic_attach(NULL, NULL, NULL);
  zbt_host_stacks(&s_status.exec_stack, &s_status.rx_stack);
  s_status.ram_held = false;
  s_status.discoverable = false;
  s_status.encrypted = false;
  if (s_status.link == ZBT_LINK_CONNECTING || s_status.link == ZBT_LINK_CONNECTED) {
    s_status.link = ZBT_LINK_DISCONNECTED;
  }
  zbt_book_host_stopped();
  s_status.stops++;
  s_phase = PHASE_IDLE;
  s_status.state = ZBT_STATE_OFF;
}

static void prv_poll_stopping(void) {
  if (!s_quit_sent) {
    if (!zbt_host_quiesced() && s_elapsed_ms < QUIESCE_WAIT_MS) {
      return;
    }
    if (!prv_rx_close()) {
      return;
    }
    zbt_host_quit();
    s_quit_sent = true;
  }

  if (zbt_host_stopped()) {
    prv_poll_stop();
    prv_release_ram();
  } else if (s_elapsed_ms >= STOP_WAIT_MS) {
    // The host threads did not exit; leave the host's RAM as it is.
    prv_poll_stop();
    s_status.stop_err = -ETIMEDOUT;
    s_status.state = ZBT_STATE_HALTED;
    s_phase = PHASE_RUN;
  }
}

static void prv_poll_main(void *data) {
  s_poll_queued = false;

  switch (s_phase) {
    case PHASE_STOP:
      prv_poll_stopping();
      break;
    default:
      prv_poll_stop();
      break;
  }
}

static void prv_setup_main(void *data) {
  prv_take_ram();
}

static void prv_start_bg(void *data) {
  prv_load_key();
  prv_derive_key();
  bt_local_id_copy_device_name(s_name, false);
  s_name[sizeof(s_name) - 1] = '\0';
  launcher_task_add_callback(prv_setup_main, NULL);
}

/* Public API -----------------------------------------------------------------*/

int zbt_start(void) {
  if (!hci_router_is_active()) {
    return -ENODEV;
  }

  pbl_irq_lock();
  const bool idle = s_phase == PHASE_IDLE;
  if (idle) {
    s_phase = PHASE_SETUP;
  }
  pbl_irq_unlock();
  if (!idle) {
    return -EALREADY;
  }

  const struct zbt_status keep = s_status;
  s_status = (struct zbt_status){
    .state = ZBT_STATE_STARTING,
    .ind_service = -1,
    .ind_signal = -1,
    .ind_roam = -1,
    .ind_battery = -1,
    .inband_ring = -1,
    .ram_used = prv_ram_used(),
    .starts = keep.starts,
    .stops = keep.stops,
  };
  s_quit_sent = false;
  if (!system_task_add_callback(prv_start_bg, NULL)) {
    s_status.enable_err = -ENOMEM;
    s_status.state = ZBT_STATE_FAILED;
    s_phase = PHASE_IDLE;
    return -ENOMEM;
  }
  return 0;
}

int zbt_stop(void) {
  pbl_irq_lock();
  const bool running = s_phase == PHASE_RUN;
  if (running) {
    s_phase = PHASE_STOP;
  }
  pbl_irq_unlock();
  if (!running) {
    return -EALREADY;
  }

  s_status.state = ZBT_STATE_STOPPING;
  s_status.stop_err = 0;
  s_quit_sent = false;
  zbt_host_stop();
  prv_poll_start();
  return 0;
}

#ifdef CONFIG_BT_ZEPHYR_CLASSIC_AUTO
int zbt_stop_wait(uint32_t timeout_ms) {
  // On KernelMain, where the stop is polled, it is polled here meanwhile.
  const bool on_main = pebble_task_get_current() == PebbleTask_KernelMain;
  uint32_t waited = 0;
  // A start under way first comes up (or fails).
  while (s_phase == PHASE_SETUP && waited < timeout_ms) {
    pbl_thread_sleep(PBL_MSEC(POLL_MS));
    waited += POLL_MS;
  }
  if (zbt_stop() != 0) {
    return s_phase == PHASE_IDLE ? 0 : -EBUSY;
  }
  while (waited < timeout_ms) {
    if (on_main) {
      s_elapsed_ms += POLL_MS;
      prv_poll_main(NULL);
    }
    if (s_phase != PHASE_STOP) {
      break;
    }
    pbl_thread_sleep(PBL_MSEC(POLL_MS));
    waited += POLL_MS;
  }
  return s_status.state == ZBT_STATE_OFF ? 0 : -ETIMEDOUT;
}
#endif

int zbt_set_discoverable(const char *name, uint32_t cod) {
  if (s_status.state != ZBT_STATE_READY) {
    return -EAGAIN;
  }
  return zbt_host_set_discoverable(true, name, cod);
}

int zbt_clear_discoverable(void) {
  if (s_status.state != ZBT_STATE_READY) {
    return -EAGAIN;
  }
  return zbt_host_set_discoverable(false, NULL, 0);
}

int zbt_connect(const uint8_t addr[6]) {
  if (s_status.state != ZBT_STATE_READY) {
    return -EAGAIN;
  }
  return zbt_host_connect(addr, false /* unattended */);
}

int zbt_hfp_answer(void) {
  if (s_status.state != ZBT_STATE_READY) {
    return -EAGAIN;
  }
  return zbt_host_hfp_answer();
}

int zbt_hfp_hangup(void) {
  if (s_status.state != ZBT_STATE_READY) {
    return -EAGAIN;
  }
  return zbt_host_hfp_hangup();
}

int zbt_hfp_dial(const char *number) {
  return zbt_hfp_dial_after(number, 0);
}

int zbt_hfp_dial_after(const char *number, uint32_t delay_ms) {
  if (s_status.state != ZBT_STATE_READY) {
    return -EAGAIN;
  }
  const size_t len = number != NULL ? strlen(number) : 0;
  if (len == 0 || len >= ZBT_CLIP_MAX) {
    return -EINVAL;
  }
  return zbt_host_hfp_dial(number, delay_ms);
}

bool zbt_hfp_dial_cancel(void) {
  return s_status.state == ZBT_STATE_READY && zbt_host_hfp_dial_cancel();
}

int zbt_hfp_redial(void) {
  if (s_status.state != ZBT_STATE_READY) {
    return -EAGAIN;
  }
  return zbt_host_hfp_redial();
}

int zbt_pbap_sync(unsigned books) {
  if (s_status.state != ZBT_STATE_READY) {
    return -EAGAIN;
  }
  return zbt_host_pbap_sync(books & ZBT_BOOK_ALL);
}

int zbt_hfp_audio_on_watch(bool on_watch) {
  if (s_status.state != ZBT_STATE_READY) {
    return -EAGAIN;
  }
  return zbt_host_hfp_audio_on_watch(on_watch);
}

int zbt_pairing_reply(bool accept) {
  if (s_status.state != ZBT_STATE_READY) {
    return -EAGAIN;
  }
  return zbt_host_pairing_reply(accept);
}

int zbt_clear_keys(void) {
  if (s_status.state == ZBT_STATE_READY) {
    (void)zbt_host_unpair();
  }
  return system_task_add_callback(prv_clear_keys_bg, NULL) ? 0 : -ENOMEM;
}

/* Call events, for the probe -----------------------------------------------------------------*/

#define TRACE_LEN 24

static struct {
  char event[TRACE_LEN];
  uint32_t ds[TRACE_LEN];
  uint8_t next;
  uint8_t count;
} s_trace;

void zbt_trace(char event) {
  const uint32_t ds = (uint32_t)(rtc_get_ticks() * 10 / RTC_TICKS_HZ);
  pbl_irq_lock();
  s_trace.event[s_trace.next] = event;
  s_trace.ds[s_trace.next] = ds;
  s_trace.next = (s_trace.next + 1) % TRACE_LEN;
  if (s_trace.count < TRACE_LEN) {
    s_trace.count++;
  }
  pbl_irq_unlock();
}

size_t zbt_trace_format(char *out, size_t size) {
  size_t len = 0;
  if (size == 0) {
    return 0;
  }
  out[0] = '\0';
  pbl_irq_lock();
  const uint8_t count = s_trace.count;
  const uint8_t first = (uint8_t)((s_trace.next + TRACE_LEN - count) % TRACE_LEN);
  char event[TRACE_LEN];
  uint32_t ds[TRACE_LEN];
  for (uint8_t i = 0; i < count; i++) {
    event[i] = s_trace.event[(first + i) % TRACE_LEN];
    ds[i] = s_trace.ds[(first + i) % TRACE_LEN];
  }
  pbl_irq_unlock();
  for (uint8_t i = 0; i < count && len + 1 < size; i++) {
    const uint32_t t = ds[i] - ds[0];
    const int n = snprintf(&out[len], size - len, "%c%lu.%lu%s", event[i], (unsigned long)(t / 10),
                           (unsigned long)(t % 10), (i % 4) == 3 ? "\n" : " ");
    if (n <= 0) {
      break;
    }
    len += (size_t)n;
    if (len >= size) {
      len = size - 1;
    }
  }
  return len;
}

void zbt_get_status(struct zbt_status *out) {
  *out = s_status;
  out->bat_off_ua = s_bat_off_ua;
  out->bat_off_samples = s_bat_off_samples;
  if (s_status.ram_held) {
    zbt_host_stacks(&out->exec_stack, &out->rx_stack);
    zbt_host_counts(&out->exec_loops, &out->rx_items);
  }
}

/* Called by the host side ----------------------------------------------------*/

struct zbt_status *zbt_ctl_status(void) {
  return &s_status;
}

/* The battery's current while the host runs ---------------------------------*/

// Only while the phone is not linked (zbt_ctl_link_changed()).
#define BAT_SAMPLE_MS 30000

static TimerID s_bat_timer = TIMER_INVALID_ID;
static int64_t s_bat_linked_sum;
static int64_t s_bat_unlinked_sum;

#ifdef CONFIG_BT_ZEPHYR_CLASSIC_AUTO
static void prv_keep_connected(bool now);
#endif

// On KernelBG; stops once the host does.
static void prv_bat_sample_bg(void *data) {
  BatteryConstants bat;
  if (s_status.state == ZBT_STATE_OFF) {
#ifndef CONFIG_DEMO_APP_BT_CLASSIC_PROBE
    // Only the probe shows it: no waking the watch for it.
    new_timer_stop(s_bat_timer);
    return;
#endif
    // The host stopped: what the watch draws without it.
    if (battery_get_constants(&bat) == 0) {
      s_bat_off_sum += bat.i_ua;
      s_bat_off_samples++;
      s_bat_off_ua = (int32_t)(s_bat_off_sum / s_bat_off_samples);
    }
    return;
  }
  if (s_status.state != ZBT_STATE_READY) {
    return;
  }
#ifdef CONFIG_BT_ZEPHYR_CLASSIC_AUTO
  prv_keep_connected(false);
#endif
  if (battery_get_constants(&bat) != 0) {
    return;
  }
  const bool calling =
      s_status.sco || (s_status.call != ZBT_CALL_NONE && s_status.call != ZBT_CALL_ENDED);
  if (s_status.link == ZBT_LINK_CONNECTED) {
    if (!calling) {
      s_bat_linked_sum += bat.i_ua;
      s_status.bat_linked_samples++;
      s_status.bat_linked_ua = (int32_t)(s_bat_linked_sum / s_status.bat_linked_samples);
    }
  } else {
    s_bat_unlinked_sum += bat.i_ua;
    s_status.bat_unlinked_samples++;
    s_status.bat_unlinked_ua = (int32_t)(s_bat_unlinked_sum / s_status.bat_unlinked_samples);
  }
}

#ifdef CONFIG_BT_ZEPHYR_CLASSIC_AUTO
// Keeping the phone connected: a try now and then while the link is down, more often while
// the phone's app is connected over LE (the phone is near and its Bluetooth on). Each try that
// does not bring the hands-free link up waits twice as long as the last, up to
// RECONNECT_MAX_MS; a phone that turns the watch's key or a pairing down is not tried again
// until the hands-free link comes up otherwise (the phone connecting) or the host restarts.
#define RECONNECT_NEAR_MS (60 * 1000)
#define RECONNECT_FAR_MS  (5 * 60 * 1000)
#define RECONNECT_MAX_MS  (60 * 60 * 1000)

static RtcTicks s_next_connect;
static uint8_t s_connect_fails;
static bool s_connect_refused;

// The bonded phone's address: the LE pairing's, else the saved key's.
static const uint8_t *prv_phone_addr(void) {
  if (s_derived.valid) {
    return s_derived.addr;
  }
  return s_key.valid ? s_key.addr : NULL;
}

// On KernelBG.
static void prv_keep_connected(bool now) {
  if (s_status.hfp == ZBT_HFP_CONNECTED) {
    s_connect_fails = 0;
    s_connect_refused = false;
  } else if (s_status.link_refused) {
    s_connect_refused = true;
  }
  const uint8_t *addr = prv_phone_addr();
  if (s_status.state != ZBT_STATE_READY || addr == NULL || s_connect_refused ||
      s_status.link == ZBT_LINK_CONNECTED || s_status.link == ZBT_LINK_CONNECTING ||
      s_status.pairing == ZBT_PAIRING_CONFIRM || s_status.pairing == ZBT_PAIRING_CONSENT) {
    return;
  }
  const RtcTicks ticks = rtc_get_ticks();
  if (!now && ticks < s_next_connect) {
    return;
  }
  const bool near = comm_session_get_system_session() != NULL;
  uint64_t wait_ms = near ? RECONNECT_NEAR_MS : RECONNECT_FAR_MS;
  for (uint8_t i = 0; i < s_connect_fails && wait_ms < RECONNECT_MAX_MS; i++) {
    wait_ms *= 2;
  }
  wait_ms = MIN(wait_ms, RECONNECT_MAX_MS);
  s_next_connect = ticks + wait_ms * RTC_TICKS_HZ / 1000;
  if (s_connect_fails < UINT8_MAX) {
    s_connect_fails++;
  }
  (void)zbt_host_connect(addr, true /* unattended */);
}

static void prv_started_bg(void *data) {
  // The routed boot came up: the next one routes too.
  hci_router_boot_confirm();
  s_connect_fails = 0;
  s_connect_refused = false;
  s_status.link_refused = false;
  prv_keep_connected(true);
}
#endif

static void prv_bat_timer_cb(void *data) {
  (void)system_task_add_callback_droppable(prv_bat_sample_bg, NULL);
}

// While the phone is linked nothing needs the timer: no reconnecting to try, and a sample would
// be the watch woken for it. Any thread.
void zbt_ctl_link_changed(bool up) {
#ifdef CONFIG_BT_ZEPHYR_CLASSIC_AUTO
  // A link that had the hands-free link up: the tries after it start over.
  if (!up && s_status.hfp == ZBT_HFP_CONNECTED) {
    s_connect_fails = 0;
    s_connect_refused = false;
  }
#endif
  if (s_bat_timer == TIMER_INVALID_ID || s_status.state != ZBT_STATE_READY) {
    return;
  }
  if (up) {
    new_timer_stop(s_bat_timer);
  } else {
    new_timer_start(s_bat_timer, BAT_SAMPLE_MS, prv_bat_timer_cb, NULL, TIMER_START_FLAG_REPEATING);
  }
}

static void prv_bat_start(void) {
  if (s_bat_timer == TIMER_INVALID_ID) {
    s_bat_timer = new_timer_create();
  }
  s_bat_linked_sum = 0;
  s_bat_unlinked_sum = 0;
  s_status.bat_linked_ua = 0;
  s_status.bat_linked_samples = 0;
  s_status.bat_unlinked_ua = 0;
  s_status.bat_unlinked_samples = 0;
  new_timer_start(s_bat_timer, BAT_SAMPLE_MS, prv_bat_timer_cb, NULL, TIMER_START_FLAG_REPEATING);
}

void zbt_ctl_enabled(int err) {
  s_status.enable_err = err;
  if (s_status.state == ZBT_STATE_STARTING) {
    s_status.state = err == 0 ? ZBT_STATE_READY : ZBT_STATE_FAILED;
    if (err == 0) {
      prv_bat_start();
    }
#ifdef CONFIG_BT_ZEPHYR_CLASSIC_AUTO
    (void)system_task_add_callback(prv_started_bg, NULL);
#endif
  }
}

void zbt_ctl_save_key(const struct zbt_link_key *key) {
  s_key_to_save = *key;
  if (!system_task_add_callback(prv_save_key_bg, NULL)) {
    s_status.key_err = -ENOMEM;
  }
}

#ifdef CONFIG_BT_CALL_AUDIO
// On KernelBG: bring the call audio in line with the voice link.
static void prv_sco_bg(void *data) {
  if (s_sco_up && s_phase == PHASE_RUN) {
    call_audio_start();
  } else if (!s_sco_up) {
    call_audio_stop();
  }
}
#endif

#ifdef CONFIG_BT_CALL_AUDIO
// On KernelBG: an answered call's audio plays through the speaker mute; a ringing one not.
static void prv_call_bg(void *data) {
  const enum zbt_call call = s_status.call;
  speaker_service_set_call_unmuted(call == ZBT_CALL_ACTIVE || call == ZBT_CALL_HELD);
}
#endif

#ifdef CONFIG_DEMO_APP_DIALER
static void prv_call_main(void *data) {
  call_indicator_update();
}
#endif

void zbt_ctl_call_changed(void) {
  const enum zbt_call call = s_status.call;
  if (call == ZBT_CALL_ACTIVE || call == ZBT_CALL_HELD) {
    if (s_status.call_since == 0) {
      s_status.call_since = (uint32_t)rtc_get_time();
    }
  } else if (call != ZBT_CALL_OUTGOING && call != ZBT_CALL_ALERTING) {
    s_status.call_since = 0;
  }
#ifdef CONFIG_BT_CALL_AUDIO
  // Only the last state counts.
  (void)system_task_add_callback_droppable(prv_call_bg, NULL);
#endif
#ifdef CONFIG_DEMO_APP_DIALER
  launcher_task_add_callback(prv_call_main, NULL);
#endif
}

// On KernelBG: the call that ended, onto the log.
static struct zbt_call_record s_ended;

static void prv_call_ended_bg(void *data) {
  struct zbt_call_record rec = s_ended;
  rec.time = (uint32_t)rtc_get_time();
  zbt_call_log_add(&rec);
}

void zbt_ctl_call_ended(const struct zbt_call_record *rec) {
  s_ended = *rec;
  (void)system_task_add_callback(prv_call_ended_bg, NULL);
}

void zbt_ctl_sco(bool up) {
#ifdef CONFIG_BT_CALL_AUDIO
  // The host's callbacks can run with the scheduler locked (its HCI input); starting and
  // stopping the call audio takes mutexes and waits, so it happens on KernelBG. Only the
  // last state counts, so a callback that cannot be queued is caught up by the next.
  s_sco_up = up;
  (void)system_task_add_callback_droppable(prv_sco_bg, NULL);
#else
  (void)up;
#endif
}

void zbt_ctl_diag(uint16_t kind, uint32_t a, uint32_t b, uint32_t c) {
#ifdef CONFIG_REBOOT_REASON_KEEP_LAST
  RebootReason reason = {
    .code = RebootReasonCode_Assert,
    .data16 = kind,
    .watchdog = {.stuck_task_pc = a, .stuck_task_lr = b, .stuck_task_callback = c},
  };
  reboot_reason_set(&reason);
  reset_due_to_software_failure();
#elif defined(UNITTEST)
  fprintf(stderr, "zbt diag %x: %08lx %08lx %08lx\n", kind, (unsigned long)a, (unsigned long)b,
          (unsigned long)c);
  abort();
#else
  zbt_ctl_fatal("zbt_ctl_diag", kind);
#endif
}

void zbt_ctl_fatal(const char *file, int line) {
  if (s_status.fatal_file == NULL) {
    s_status.fatal_file = file;
    s_status.fatal_line = line;
  }
  if (s_status.state != ZBT_STATE_STOPPING) {
    s_status.state = ZBT_STATE_HALTED;
  }
}
