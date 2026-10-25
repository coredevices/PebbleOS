/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// Probes whether the SF32LB52 controller has BR/EDR alive with the current LCPU
// config and patch. Reads run on launch; Select toggles BR/EDR discoverability
// and a long Select opens a menu to refresh, to reserve the HCI router for the
// next boot, and to start the Zephyr host and make it discoverable.
// Only commands the controller lists as supported are sent, and nothing is
// persisted, so a reboot always returns to the stock state.

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <pbl/logging/logging.h>
#include <pbl/services/time.h>
#include <pbl/util/heap.h>
#include <pbl/util/size.h>

#include <applib/app.h>
#include <applib/fonts/fonts.h>
#include <applib/ui/app_window_stack.h>
#include <applib/ui/ui.h>
#include <host/ble_hs.h>
#include <kernel/kernel_heap.h>
#include <kernel/pbl_malloc.h>
#include <nimble/hci_common.h>
#include <process_management/pebble_process_md.h>
#include <process_state/app_state/app_state.h>
#include <resource/resource_ids.auto.h>
#include <system/reboot_reason.h>
#include <system/reset.h>
#ifdef CONFIG_BT_HCI_ROUTER
#include <pbl/bluetooth/hci_router.h>
#endif
#ifdef CONFIG_BT_CALL_AUDIO
#include <pbl/bluetooth/lcpu_audio_probe.h>
#endif
// Where the power goes.
#include <pbl/drivers/display/display.h>
#include <pbl/drivers/mic.h>
#include <pbl/drivers/rtc.h>
#include <pbl/drivers/speaker/sf32lb52/audio_definitions.h>
#include <pbl/drivers/watchdog.h>
#include <pbl/services/speaker/speaker_service.h>
#include <pbl/soc/sf32lb/sleep.h>
#include <pbl/util/bits.h>

#include <board/board.h>
#ifdef CONFIG_BT_ZEPHYR_CLASSIC

#ifdef CONFIG_BT_CALL_AUDIO
#include <pbl/bluetooth/call_audio.h>
#include <pbl/drivers/battery.h>
#endif
#include <pbl/bluetooth/lcpu_audio_probe.h>
#include <pbl/services/bluetooth/bluetooth_persistent_storage.h>

#include <applib/app_timer.h>
#include <applib/touch_service.h>
#include <comm/ble/gap_le_connection.h>
#include <comm/bt_lock.h>
#include <kernel/event_loop.h>
#include <process_management/worker_manager.h>
#include <shell/system_app_ids.auto.h>
#include <zbt/zbt.h>
#endif

// NimBLE host internal (ble_hs_hci_priv.h). Goes through the host so its
// command credit accounting stays right.
int ble_hs_hci_cmd_tx(uint16_t opcode, const void *cmd, uint8_t cmd_len, void *rsp,
                      uint8_t rsp_len);

#define OP_WRITE_LOCAL_NAME        BLE_HCI_OP(BLE_HCI_OGF_CTLR_BASEBAND, 0x0013)
#define OP_READ_SCAN_ENABLE        BLE_HCI_OP(BLE_HCI_OGF_CTLR_BASEBAND, 0x0019)
#define OP_WRITE_SCAN_ENABLE       BLE_HCI_OP(BLE_HCI_OGF_CTLR_BASEBAND, 0x001A)
#define OP_WRITE_CLASS_OF_DEVICE   BLE_HCI_OP(BLE_HCI_OGF_CTLR_BASEBAND, 0x0024)
#define OP_READ_VOICE_SETTING      BLE_HCI_OP(BLE_HCI_OGF_CTLR_BASEBAND, 0x0025)
#define OP_WRITE_EIR               BLE_HCI_OP(BLE_HCI_OGF_CTLR_BASEBAND, 0x0052)
#define OP_READ_LOCAL_VERSION      BLE_HCI_OP(BLE_HCI_OGF_INFO_PARAMS, 0x0001)
#define OP_READ_SUPPORTED_COMMANDS BLE_HCI_OP(BLE_HCI_OGF_INFO_PARAMS, 0x0002)
#define OP_READ_LOCAL_FEATURES     BLE_HCI_OP(BLE_HCI_OGF_INFO_PARAMS, 0x0003)
#define OP_READ_BUFFER_SIZE        BLE_HCI_OP(BLE_HCI_OGF_INFO_PARAMS, 0x0005)
#define OP_READ_BD_ADDR            BLE_HCI_OP(BLE_HCI_OGF_INFO_PARAMS, 0x0009)
#define OP_LE_READ_BUFFER_SIZE     BLE_HCI_OP(BLE_HCI_OGF_LE, 0x0002)
#define OP_LE_READ_LOCAL_FEATURES  BLE_HCI_OP(BLE_HCI_OGF_LE, 0x0003)

// Supported Commands bitmask positions (octet, bit)
#define CMD_WRITE_LOCAL_NAME      7, 0
#define CMD_READ_SCAN_ENABLE      7, 6
#define CMD_WRITE_SCAN_ENABLE     7, 7
#define CMD_WRITE_CLASS_OF_DEVICE 9, 1
#define CMD_READ_VOICE_SETTING    9, 2
#define CMD_SETUP_SYNC_CONN       16, 3
#define CMD_WRITE_EIR             17, 1

#define LOCAL_NAME            "PT2-CLASSIC-TEST"
#define LOCAL_NAME_LEN        248
#define EIR_LEN               240
#define SCAN_INQUIRY_AND_PAGE 0x03
#define SCAN_OFF              0x00
// Service class Audio, major Wearable, minor Wrist Watch
#define CLASS_OF_DEVICE 0x200704

#define TEXT_BUF_SIZE 4096

typedef struct {
  Window window;
  ScrollLayer scroll_layer;
  TextLayer text_layer;
  char text[TEXT_BUF_SIZE];
  size_t text_len;
  uint8_t supported_cmds[64];
  bool supported_cmds_valid;
  bool scan_enabled_by_us;
  int last_scan_rc;
  int last_scan_enable;
  uint8_t cmd_buf[LOCAL_NAME_LEN];
  unsigned int heap_samples;
  Window menu_window;
  SimpleMenuLayer menu_layer;
  SimpleMenuSection menu_section;
  SimpleMenuItem menu_items[18];
#ifdef CONFIG_BT_HCI_ROUTER
  Window log_window;
  ScrollLayer log_scroll;
  TextLayer log_text;
  char log_buf[1280];
#endif
  AppTimer *refresh_timer;
  unsigned int refresh_ticks;
#ifdef CONFIG_BT_ZEPHYR_CLASSIC
  // The phone: identity address of the active LE gateway.
  bool phone_valid;
  bool phone_random;
  uint8_t phone_addr[6];
  const char *phone_source;
  Window heap_window;
  TextLayer heap_text;
  char heap_buf[192];
  Window pair_window;
  TextLayer pair_text;
  char pair_buf[96];
  bool pair_shown;
  Window call_window;
  ScrollLayer call_scroll;
  TextLayer call_text;
  char call_buf[1280];
  AppTimer *call_timer;
  // Battery current over the call's audio (since its last start): sum and count of the
  // once-a-second readings, and the start they belong to.
  int64_t bat_ua_sum;
  uint32_t bat_reads;
  uint16_t bat_start;
#endif
} ProbeData;

static void prv_append_router(ProbeData *data);

static void prv_append(ProbeData *data, const char *fmt, ...) {
  if (data->text_len >= sizeof(data->text) - 1) {
    return;
  }
  va_list args;
  va_start(args, fmt);
  int n = vsnprintf(&data->text[data->text_len], sizeof(data->text) - data->text_len, fmt, args);
  va_end(args);
  if (n > 0) {
    data->text_len += n;
    if (data->text_len >= sizeof(data->text)) {
      data->text_len = sizeof(data->text) - 1;
    }
  }
}

static const char *prv_rc_str(int rc, char *buf, size_t len) {
  if (rc == 0) {
    return "OK";
  }
  if (rc > BLE_HS_ERR_HCI_BASE && rc < BLE_HS_ERR_HCI_BASE + 0x100) {
    snprintf(buf, len, "HCI 0x%02x", rc - BLE_HS_ERR_HCI_BASE);
  } else {
    snprintf(buf, len, "HS %d", rc);
  }
  return buf;
}

static int prv_cmd(uint16_t opcode, const void *cmd, uint8_t cmd_len, void *rsp, uint8_t rsp_len) {
  if (!ble_hs_synced()) {
    return BLE_HS_ENOTSYNCED;
  }
  int rc = ble_hs_hci_cmd_tx(opcode, cmd, cmd_len, rsp, rsp_len);
  PBL_LOG_DBG("HCI 0x%04x rc %d", opcode, rc);
  return rc;
}

static bool prv_cmd_supported(const ProbeData *data, unsigned int octet, unsigned int bit) {
  return data->supported_cmds_valid && (data->supported_cmds[octet] & (1U << bit)) != 0;
}

static bool prv_bit(const uint8_t *feat, unsigned int octet, unsigned int bit) {
  return (feat[octet] & (1U << bit)) != 0;
}

static void prv_append_rc(ProbeData *data, const char *label, int rc) {
  char buf[16];
  prv_append(data, "%s: %s\n", label, prv_rc_str(rc, buf, sizeof(buf)));
}

static void prv_append_hex(ProbeData *data, const uint8_t *bytes, size_t len) {
  for (size_t i = 0; i < len; i++) {
    prv_append(data, "%02x", bytes[i]);
  }
  prv_append(data, "\n");
}

static void prv_read_scan_enable(ProbeData *data) {
  uint8_t scan = 0;
  data->last_scan_enable = -1;
  if (!prv_cmd_supported(data, CMD_READ_SCAN_ENABLE)) {
    data->last_scan_rc = BLE_HS_ENOTSUP;
    return;
  }
  data->last_scan_rc = prv_cmd(OP_READ_SCAN_ENABLE, NULL, 0, &scan, sizeof(scan));
  if (data->last_scan_rc == 0) {
    data->last_scan_enable = scan;
  }
}

static void prv_append_scan_state(ProbeData *data) {
  char buf[16];
  if (data->last_scan_enable >= 0) {
    prv_append(data, "Scan enable: 0x%02x (%s)\n", data->last_scan_enable,
               data->last_scan_enable == SCAN_INQUIRY_AND_PAGE ? "discoverable"
               : data->last_scan_enable == SCAN_OFF            ? "off"
                                                               : "partial");
  } else {
    prv_append(data, "Scan enable: ? (%s)\n", prv_rc_str(data->last_scan_rc, buf, sizeof(buf)));
  }
}

static void prv_run_reads(ProbeData *data) {
  int rc;

  prv_append(data, "Host synced: %s\n\n", ble_hs_synced() ? "yes" : "NO");

  uint8_t ver[8];
  rc = prv_cmd(OP_READ_LOCAL_VERSION, NULL, 0, ver, sizeof(ver));
  prv_append_rc(data, "1 Version", rc);
  if (rc == 0) {
    prv_append(data, " HCI %u rev %04x\n LMP %u sub %04x\n Mfr %04x\n", ver[0],
               ver[1] | (ver[2] << 8), ver[3], ver[6] | (ver[7] << 8), ver[4] | (ver[5] << 8));
  }

  uint8_t feat[8];
  rc = prv_cmd(OP_READ_LOCAL_FEATURES, NULL, 0, feat, sizeof(feat));
  prv_append_rc(data, "2 LMP feat", rc);
  if (rc == 0) {
    prv_append(data, " ");
    prv_append_hex(data, feat, sizeof(feat));
    prv_append(data, " BR/EDR NotSup %u\n SCO %u HV3 %u\n eSCO(EV3) %u 2-EV3 %u\n LE %u\n",
               prv_bit(feat, 4, 5), prv_bit(feat, 1, 3), prv_bit(feat, 1, 5), prv_bit(feat, 3, 7),
               prv_bit(feat, 5, 5), prv_bit(feat, 4, 6));
  }

  rc = prv_cmd(OP_READ_SUPPORTED_COMMANDS, NULL, 0, data->supported_cmds,
               sizeof(data->supported_cmds));
  data->supported_cmds_valid = (rc == 0);
  prv_append_rc(data, "3 Sup cmds", rc);
  if (rc == 0) {
    prv_append(data, " o7 %02x o9 %02x o16 %02x o17 %02x\n", data->supported_cmds[7],
               data->supported_cmds[9], data->supported_cmds[16], data->supported_cmds[17]);
    prv_append(data, " WrName %u WrScan %u\n RdScan %u WrCoD %u\n WrEIR %u SetupSync %u\n",
               prv_cmd_supported(data, CMD_WRITE_LOCAL_NAME),
               prv_cmd_supported(data, CMD_WRITE_SCAN_ENABLE),
               prv_cmd_supported(data, CMD_READ_SCAN_ENABLE),
               prv_cmd_supported(data, CMD_WRITE_CLASS_OF_DEVICE),
               prv_cmd_supported(data, CMD_WRITE_EIR),
               prv_cmd_supported(data, CMD_SETUP_SYNC_CONN));
  }

  uint8_t addr[6];
  rc = prv_cmd(OP_READ_BD_ADDR, NULL, 0, addr, sizeof(addr));
  prv_append_rc(data, "4 BD_ADDR", rc);
  if (rc == 0) {
    prv_append(data, " %02x:%02x:%02x:%02x:%02x:%02x\n", addr[5], addr[4], addr[3], addr[2],
               addr[1], addr[0]);
  }

  uint8_t le_feat[8];
  rc = prv_cmd(OP_LE_READ_LOCAL_FEATURES, NULL, 0, le_feat, sizeof(le_feat));
  prv_append_rc(data, "5 LE feat", rc);
  if (rc == 0) {
    prv_append(data, " ");
    prv_append_hex(data, le_feat, sizeof(le_feat));
    prv_append(data, " CIS Central %u\n CIS Periph %u\n", prv_bit(le_feat, 3, 4),
               prv_bit(le_feat, 3, 5));
  }

  uint8_t buf_size[7];
  rc = prv_cmd(OP_READ_BUFFER_SIZE, NULL, 0, buf_size, sizeof(buf_size));
  prv_append_rc(data, "R1 Buf size", rc);
  if (rc == 0) {
    prv_append(data, " ACL len %u num %u\n SCO len %u num %u\n", buf_size[0] | (buf_size[1] << 8),
               buf_size[3] | (buf_size[4] << 8), buf_size[2], buf_size[5] | (buf_size[6] << 8));
  }

  uint8_t le_buf_size[3];
  rc = prv_cmd(OP_LE_READ_BUFFER_SIZE, NULL, 0, le_buf_size, sizeof(le_buf_size));
  prv_append_rc(data, "R2 LE buf", rc);
  if (rc == 0) {
    prv_append(data, " LE ACL len %u num %u\n", le_buf_size[0] | (le_buf_size[1] << 8),
               le_buf_size[2]);
  }

  if (prv_cmd_supported(data, CMD_READ_VOICE_SETTING)) {
    uint8_t voice[2];
    rc = prv_cmd(OP_READ_VOICE_SETTING, NULL, 0, voice, sizeof(voice));
    prv_append_rc(data, "R3 Voice", rc);
    if (rc == 0) {
      prv_append(data, " 0x%04x\n", voice[0] | (voice[1] << 8));
    }
  } else {
    prv_append_rc(data, "R3 Voice", BLE_HS_ENOTSUP);
  }

  prv_read_scan_enable(data);
  prv_append_scan_state(data);
}

// min_headroom only covers the time since the last (hourly) analytics
// heartbeat, which resets the high water mark; boot_min covers the whole boot.
// Where the CPU's time went since boot (deep sleep is what idle should be), what keeps it from
// deeper sleep now and who: the display updating, the speaker or the microphone running.
static void prv_append_power(ProbeData *data) {
  uint64_t wfi;
  uint64_t dwfi;
  uint64_t ds;
  uint32_t ipc;
  soc_sf32lb_sleep_totals(&wfi, &dwfi, &ds, &ipc);
  const uint64_t total = rtc_get_ticks();
  const int max = (int)soc_sf32lb_sleep_max_level();
  const bool disp = display_update_in_progress();
  const bool aud = AUDIO->state->running;
  const bool mic = mic_is_running((MicDevice *)MIC);
  prv_append(data, " cpu ds %u%% dwfi %u%% wfi %u%% max %d ipc %lu spk %d\n",
             (unsigned)(total ? ds * 100 / total : 0), (unsigned)(total ? dwfi * 100 / total : 0),
             (unsigned)(total ? wfi * 100 / total : 0), max, (unsigned long)ipc,
             (int)speaker_service_get_state());
  uint32_t blocked;
  uint32_t near;
  soc_sf32lb_sleep_why(&blocked, &near);
  prv_append(data, " nods blk %lu near %lu b %d/%d/%d disp %d aud %d mic %d\n",
             (unsigned long)blocked, (unsigned long)near, soc_sf32lb_sleep_blocks(SOC_SF32LB_WFI),
             soc_sf32lb_sleep_blocks(SOC_SF32LB_DEEPWFI),
             soc_sf32lb_sleep_blocks(SOC_SF32LB_DEEPSLEEP), disp, aud, mic);
  uint32_t w_timer;
  uint32_t w_lcpu;
  uint32_t w_other;
  uint32_t w_pins[SOC_SF32LB_WAKE_PINS];
  soc_sf32lb_sleep_wakes(&w_timer, &w_lcpu, &w_other, w_pins);
  prv_append(data, " wake tmr %lu lcpu %lu oth %lu gpio", (unsigned long)w_timer,
             (unsigned long)w_lcpu, (unsigned long)w_other);
  for (int i = 0; i < SOC_SF32LB_WAKE_PINS; i++) {
    if (w_pins[i] != 0) {
      prv_append(data, " PA%d:%lu", i, (unsigned long)w_pins[i]);
    }
  }
  uint32_t len[4];
  soc_sf32lb_sleep_lengths(len);
  prv_append(data, "\n ds len .2s %lu 1s %lu 5s %lu more %lu\n", (unsigned long)len[0],
             (unsigned long)len[1], (unsigned long)len[2], (unsigned long)len[3]);
#ifdef CONFIG_BT_CALL_AUDIO
  // A request left set keeps the LCPU awake (outside a call: refs 0, req 0).
  struct lcpu_audio_probe lp;
  lcpu_audio_probe_read(&lp);
  prv_append(data, " lcpu refs %u req %d act %d\n", lp.lcpu_wake_refs, lp.lcpu_wake_req,
             lp.lcpu_active);
#endif
}

static void prv_append_heap(ProbeData *data) {
  Heap *heap = kernel_heap_get();
  unsigned int used, free_bytes, max_free;
  heap_calc_totals(heap, &used, &free_bytes, &max_free);
  data->heap_samples++;
  prv_append(data,
             "R4 Kernel heap #%u\n size %u\n used %u free %u\n max_free %u\n min_headroom %lu\n",
             data->heap_samples, (unsigned int)heap_size(heap), used, free_bytes, max_free,
             (unsigned long)heap_get_minimum_headroom(heap));
  prv_append(data, " boot_min %lu\n uptime %lus\n",
             (unsigned long)kernel_heap_get_boot_minimum_headroom(),
             (unsigned long)time_get_uptime_seconds());
}

static int prv_write_scan(ProbeData *data, uint8_t scan) {
  if (!prv_cmd_supported(data, CMD_WRITE_SCAN_ENABLE)) {
    return BLE_HS_ENOTSUP;
  }
  return prv_cmd(OP_WRITE_SCAN_ENABLE, &scan, sizeof(scan), NULL, 0);
}

static int prv_write_local_name(ProbeData *data) {
  if (!prv_cmd_supported(data, CMD_WRITE_LOCAL_NAME)) {
    return BLE_HS_ENOTSUP;
  }
  memset(data->cmd_buf, 0, LOCAL_NAME_LEN);
  memcpy(data->cmd_buf, LOCAL_NAME, strlen(LOCAL_NAME));
  return prv_cmd(OP_WRITE_LOCAL_NAME, data->cmd_buf, LOCAL_NAME_LEN, NULL, 0);
}

static int prv_write_class_of_device(ProbeData *data) {
  if (!prv_cmd_supported(data, CMD_WRITE_CLASS_OF_DEVICE)) {
    return BLE_HS_ENOTSUP;
  }
  const uint8_t cod[3] = {
    CLASS_OF_DEVICE & 0xff, (CLASS_OF_DEVICE >> 8) & 0xff, (CLASS_OF_DEVICE >> 16) & 0xff
  };
  return prv_cmd(OP_WRITE_CLASS_OF_DEVICE, cod, sizeof(cod), NULL, 0);
}

static int prv_write_eir(ProbeData *data) {
  if (!prv_cmd_supported(data, CMD_WRITE_EIR)) {
    return BLE_HS_ENOTSUP;
  }
  const size_t name_len = strlen(LOCAL_NAME);
  uint8_t *p = data->cmd_buf;
  memset(p, 0, 1 + EIR_LEN);
  p[0] = 0x01; // FEC required
  p[1] = 1 + name_len;
  p[2] = 0x09; // Complete Local Name
  memcpy(&p[3], LOCAL_NAME, name_len);
  return prv_cmd(OP_WRITE_EIR, p, 1 + EIR_LEN, NULL, 0);
}

static void prv_refresh_text(ProbeData *data) {
  TextLayer *text = &data->text_layer;
  const GRect *bounds = &data->window.layer.bounds;
  text_layer_set_size(text, GSize(bounds->size.w, 2000));
  text_layer_set_text(text, data->text);
  GSize size = text_layer_get_content_size(app_state_get_graphics_context(), text);
  size.h += 8;
  text_layer_set_size(text, GSize(bounds->size.w, size.h));
  scroll_layer_set_content_size(&data->scroll_layer, GSize(bounds->size.w, size.h));
}

static void prv_select_click(ClickRecognizerRef recognizer, void *context) {
  ProbeData *data = context;
  int rc;

  data->text_len = 0;
  data->text[0] = '\0';

  if (data->last_scan_enable == SCAN_INQUIRY_AND_PAGE || data->scan_enabled_by_us) {
    rc = prv_write_scan(data, SCAN_OFF);
    prv_append_rc(data, "Scan off", rc);
    if (rc == 0) {
      data->scan_enabled_by_us = false;
    }
  } else {
    prv_append_rc(data, "6 WrName", prv_write_local_name(data));
    prv_append_rc(data, "7 WrCoD", prv_write_class_of_device(data));
    prv_append_rc(data, "8 WrEIR", prv_write_eir(data));
    rc = prv_write_scan(data, SCAN_INQUIRY_AND_PAGE);
    prv_append_rc(data, "9 WrScan 03", rc);
    if (rc == 0) {
      data->scan_enabled_by_us = true;
    }
    prv_append(data, " name %s\n", LOCAL_NAME);
  }

  prv_read_scan_enable(data);
  prv_append_scan_state(data);
  prv_append_router(data);
  prv_append(data, "\nSelect: %s\nHold Select: menu\n",
             data->scan_enabled_by_us ? "stop discoverable" : "make discoverable");
  prv_refresh_text(data);
  scroll_layer_set_content_offset(&data->scroll_layer, GPointZero, false);
}

static void prv_append_router(ProbeData *data) {
#ifdef CONFIG_BT_HCI_ROUTER
  if (!hci_router_is_active()) {
    prv_append(data, "HCI router: off (plain path)\n");
    return;
  }
  struct hci_router_stats st;
  hci_router_get_stats(&st);
  prv_append(data, "HCI router: ACTIVE (this boot)\n");
  prv_append(data, " NimBLE evt %lu acl %lu\n classic evt %lu acl %lu\n",
             (unsigned long)st.nimble_evt, (unsigned long)st.nimble_acl,
             (unsigned long)st.classic_evt, (unsigned long)st.classic_acl);
  prv_append(data, " drop cl %lu long %lu\n unk %lu sco %lu iso %lu\n",
             (unsigned long)st.drop_classic, (unsigned long)st.drop_evt_too_long,
             (unsigned long)st.drop_unknown_type, (unsigned long)st.drop_sco,
             (unsigned long)st.drop_iso);
  prv_append(data, " big %lu bad %lu alloc %lu\n rdy %lu ccx %lu rtr %lu\n",
             (unsigned long)st.drop_oversize, (unsigned long)st.drop_malformed,
             (unsigned long)st.drop_alloc, (unsigned long)st.drop_vendor_ready,
             (unsigned long)st.cc_unmatched, (unsigned long)st.router_acks);
  prv_append(data, " nocp %lu tx cmd %lu data %lu\n txerr %lu lockw %lu\n",
             (unsigned long)st.nocp_split, (unsigned long)st.tx_cmd, (unsigned long)st.tx_data,
             (unsigned long)st.tx_errors, (unsigned long)st.tx_lock_waits);
  prv_append(data, " gate wait %lu timeout %lu\n", (unsigned long)st.gate_waits,
             (unsigned long)st.gate_timeouts);
  prv_append(data, " cls pass %lu merge %lu\n cls fake %lu reject %lu\n",
             (unsigned long)st.classic_cmd_passed, (unsigned long)st.classic_cmd_merged,
             (unsigned long)st.classic_cmd_faked, (unsigned long)st.classic_cmd_rejected);
#else
  (void)data;
#endif
}

#ifdef CONFIG_BT_ZEPHYR_CLASSIC
static const char *const s_zbt_state_names[] = {
  [ZBT_STATE_OFF] = "off",       [ZBT_STATE_STARTING] = "STARTING",
  [ZBT_STATE_READY] = "READY",   [ZBT_STATE_FAILED] = "FAILED",
  [ZBT_STATE_HALTED] = "HALTED", [ZBT_STATE_STOPPING] = "STOPPING",
};
#endif

#ifdef CONFIG_BT_ZEPHYR_CLASSIC
static const char *const s_zbt_link_names[] = {
  [ZBT_LINK_NONE] = "none",
  [ZBT_LINK_CONNECTING] = "CONNECTING",
  [ZBT_LINK_CONNECTED] = "CONNECTED",
  [ZBT_LINK_DISCONNECTED] = "DISCONN",
};

static const char *const s_zbt_hfp_names[] = {
  [ZBT_HFP_NONE] = "none",
  [ZBT_HFP_SDP] = "SDP",
  [ZBT_HFP_CONNECTING] = "CONNECTING",
  [ZBT_HFP_CONNECTED] = "CONNECTED",
  [ZBT_HFP_DISCONNECTED] = "DISCONN",
};

static const char *const s_zbt_call_names[] = {
  [ZBT_CALL_NONE] = "none",         [ZBT_CALL_INCOMING] = "INCOMING",
  [ZBT_CALL_OUTGOING] = "OUTGOING", [ZBT_CALL_ALERTING] = "ALERTING",
  [ZBT_CALL_ACTIVE] = "ACTIVE",     [ZBT_CALL_HELD] = "HELD",
  [ZBT_CALL_ENDED] = "ENDED",
};

static const char *const s_zbt_pairing_names[] = {
  [ZBT_PAIRING_NONE] = "none",       [ZBT_PAIRING_CONFIRM] = "CONFIRM",
  [ZBT_PAIRING_CONSENT] = "CONSENT", [ZBT_PAIRING_DONE] = "DONE",
  [ZBT_PAIRING_FAILED] = "FAILED",   [ZBT_PAIRING_CANCELLED] = "CANCELLED",
};

static void prv_set_phone(ProbeData *data, const struct pbl_bt_device_internal *dev,
                          const char *source) {
  data->phone_valid = true;
  data->phone_random = dev->is_random_address;
  memcpy(data->phone_addr, dev->address.octets, sizeof(data->phone_addr));
  data->phone_source = source;
}

struct bond_count {
  int n;
  struct pbl_bt_device_internal dev;
};

static void prv_count_bond(struct pbl_bt_device_internal *device, struct pbl_bt_sm_key *irk,
                           const char *name, pbl_bt_bonding_id_t *id, void *context) {
  struct bond_count *count = context;
  count->n++;
  count->dev = *device;
}

// The phone's identity address, from the LE pairing of the connected gateway (the phone running
// the Pebble app); BR/EDR needs it to be the phone's public address. Without a gateway
// connection, the only LE pairing if there is just one.
static void prv_read_phone(ProbeData *data) {
  data->phone_valid = false;

  bt_lock();
  const GAPLEConnection *conn = gap_le_connection_get_gateway();
  const pbl_bt_bonding_id_t id = conn ? conn->bonding_id : PBL_BT_BONDING_ID_INVALID;
  const struct pbl_bt_device_internal conn_dev =
      conn ? conn->device : (struct pbl_bt_device_internal){};
  bt_unlock();

  struct pbl_bt_device_internal dev;
  if (id != PBL_BT_BONDING_ID_INVALID &&
      bt_persistent_storage_get_ble_pairing_by_id(id, NULL, &dev, NULL)) {
    prv_set_phone(data, &dev, "gw bond");
    return;
  }
  if (conn != NULL) {
    prv_set_phone(data, &conn_dev, "gw conn");
    return;
  }
  struct bond_count count = {0};
  bt_persistent_storage_for_each_ble_pairing(prv_count_bond, &count);
  if (count.n == 1) {
    prv_set_phone(data, &count.dev, "only bond");
  }
}
#endif

#ifdef CONFIG_BT_ZEPHYR_CLASSIC
#define SMP_AUTHREQ_CT2   0x20
#define SMP_KEY_DIST_LINK 0x08

// The last LE pairing on this boot: key distribution each side asked for, and CT2. With Secure
// Connections, the agreed distribution is the AND of both.
static void prv_append_smp(ProbeData *data) {
  struct hci_router_smp_pdu req;
  struct hci_router_smp_pdu rsp;
  hci_router_get_smp(&req, &rsp);
  if (!req.valid && !rsp.valid) {
    prv_append(data, " LE pairing: none this boot\n");
    return;
  }
  prv_append(data, " LE req %s ik %02x rk %02x ct2 %d\n", req.rx ? "phone" : "watch",
             req.init_key_dist, req.resp_key_dist, !!(req.authreq & SMP_AUTHREQ_CT2));
  prv_append(data, " LE rsp %s ik %02x rk %02x ct2 %d\n", rsp.rx ? "phone" : "watch",
             rsp.init_key_dist, rsp.resp_key_dist, !!(rsp.authreq & SMP_AUTHREQ_CT2));
  if (req.valid && rsp.valid) {
    const uint8_t ik = req.init_key_dist & rsp.init_key_dist;
    const uint8_t rk = req.resp_key_dist & rsp.resp_key_dist;
    prv_append(data, " agreed ik %02x rk %02x LINK %s\n", ik, rk,
               (ik & rk & SMP_KEY_DIST_LINK) ? "YES" : "NO");
  }
}
#endif

static void prv_append_zephyr(ProbeData *data) {
#ifdef CONFIG_BT_ZEPHYR_CLASSIC
  struct zbt_status st;
  zbt_get_status(&st);
  prv_append(data, "Zephyr: %s err %d\n", s_zbt_state_names[st.state], st.enable_err);
  // Where a start stands: RAM, work queue and HCI input activity, last command.
  prv_append(data, " RAM %s loops BT %lu RX %lu\n cmd sent %04x done %04x\n",
             st.ram_held ? "HELD" : "free", (unsigned long)st.exec_loops,
             (unsigned long)st.rx_items, st.last_cmd_sent, st.last_cmd_done);
  if (st.state == ZBT_STATE_HALTED) {
    const char *file = st.fatal_file ? strrchr(st.fatal_file, '/') : NULL;
    prv_append(data, " at %s:%d\n", file ? file + 1 : "?", st.fatal_line);
  }
  if (data->phone_valid) {
    const uint8_t *a = data->phone_addr;
    prv_append(data, "Phone " PBL_BT_ADDR_FMT " %s\n (%s)\n", a[5], a[4], a[3], a[2], a[1], a[0],
               data->phone_random ? "RANDOM" : "public", data->phone_source);
  } else {
    prv_append(data, "Phone: not found\n");
  }
  prv_append(data, " link %s err %d enc %s L%u\n", s_zbt_link_names[st.link], st.link_err,
             st.encrypted ? "YES" : "no", st.security);
  prv_append(data, " pair %s err %d bond %d\n", s_zbt_pairing_names[st.pairing], st.pairing_err,
             st.bonded);
  if (st.pairing == ZBT_PAIRING_CONFIRM) {
    prv_append(data, " PASSKEY %06lu\n", (unsigned long)st.passkey);
  }
  prv_append(data, " key stored %d loaded %d err %d\n", st.key_stored, st.key_loaded, st.key_err);
  prv_append(data, " derived %s err %d auth %d\n", st.key_derived ? "YES" : "no", st.derive_err,
             st.derived_auth);
  prv_append_smp(data);
  prv_append(data, " HFP %s err %d call %s\n", s_zbt_hfp_names[st.hfp], st.hfp_err,
             s_zbt_call_names[st.call]);
  prv_append(data, " link %s %ums n %u err %d pol %x\n", st.link_mode == 2 ? "SNIFF" : "active",
             (unsigned)(st.sniff_interval * 5 / 8), st.sniff_count, st.sniff_err, st.link_policy);
  prv_append(data, " off %ld.%ldmA n %lu\n", (long)(st.bat_off_ua / 1000),
             (long)(st.bat_off_ua / 100 % 10), (unsigned long)st.bat_off_samples);
  // The battery's current while the host runs: link up and idle, and link down.
  prv_append(data, " idle %ld.%ldmA n %lu  nolink %ld.%ldmA n %lu\n",
             (long)(st.bat_linked_ua / 1000), (long)(st.bat_linked_ua / 100 % 10),
             (unsigned long)st.bat_linked_samples, (long)(st.bat_unlinked_ua / 1000),
             (long)(st.bat_unlinked_ua / 100 % 10), (unsigned long)st.bat_unlinked_samples);
  prv_append(data, " disc %s err %d\n", st.discoverable ? "ON" : "off", st.discoverable_err);
  prv_append(data, " rx evt %lu acl %lu drop %lu\n tx cmd %lu acl %lu\n", (unsigned long)st.rx_evt,
             (unsigned long)st.rx_acl, (unsigned long)st.rx_drop, (unsigned long)st.tx_cmd,
             (unsigned long)st.tx_acl);
  prv_append(data, " RAM %s %lu B wait %lums\n", st.ram_held ? "HELD" : "free",
             (unsigned long)st.ram_used, (unsigned long)st.worker_wait_ms);
  prv_append(data, " start %u stop %u err %d wrk refused %lu\n", st.starts, st.stops, st.stop_err,
             (unsigned long)st.worker_refused);
  uint32_t hci_size;
  uint32_t hci_unused;
  hci_router_rx_stack(&hci_size, &hci_unused);
  prv_append(data, " stack BT %u/%u RX %u/%u\n HCI %lu/%lu\n", st.exec_stack.used,
             st.exec_stack.size, st.rx_stack.used, st.rx_stack.size,
             (unsigned long)(hci_size - hci_unused), (unsigned long)hci_size);
#else
  (void)data;
#endif
}

// Why the watch last rebooted, so a crash shows on the first screen after it.
static void prv_append_last_reboot(ProbeData *data) {
  const RebootReasonCode code = reboot_reason_get_last_reboot_reason();
  const char *name;
  switch (code) {
    case RebootReasonCode_Watchdog:
      name = "Watchdog";
      break;
    case RebootReasonCode_Assert:
      name = "Assert";
      break;
    case RebootReasonCode_StackOverflow:
      name = "StackOverflow";
      break;
    case RebootReasonCode_HardFault:
      name = "HardFault";
      break;
    case RebootReasonCode_OutOfMemory:
      name = "OOM";
      break;
    case RebootReasonCode_BtCoredump:
      name = "BtCoredump";
      break;
    case RebootReasonCode_CoreDump:
      name = "CoreDump";
      break;
    default:
      name = code < RebootReasonCode_Watchdog ? "normal" : "error";
      break;
  }
  RebootReason last;
  reboot_reason_get_last(&last);
  if (code == RebootReasonCode_Watchdog) {
    prv_append(data, "Last reboot: %d %s\n PC %08lX LR %08lX\n", (int)code, name,
               (unsigned long)last.watchdog.stuck_task_pc,
               (unsigned long)last.watchdog.stuck_task_lr);
  } else if (code == RebootReasonCode_HardFault && last.data16 == 0xF5) {
    // A fault in a thread that is neither the app nor the worker: stack pointer, fault status
    // and characters 5 to 8 of the thread's name.
    const uint32_t packed = last.watchdog.stuck_task_callback;
    prv_append(data, "Last reboot: %d %s\n SP %08lX CFSR %08lX\n thread ....%c%c%c%c\n", (int)code,
               name, (unsigned long)last.watchdog.stuck_task_pc,
               (unsigned long)last.watchdog.stuck_task_lr, (char)(packed & 0xFF),
               (char)((packed >> 8) & 0xFF), (char)((packed >> 16) & 0xFF),
               (char)((packed >> 24) & 0xFF));
  } else if (code == RebootReasonCode_HardFault && last.watchdog.stuck_task_callback != 0) {
    // A fault in a thread that is neither the app nor the worker: PC, LR and the thread's name.
    const uint32_t packed = last.watchdog.stuck_task_callback;
    prv_append(data, "Last reboot: %d %s\n PC %08lX LR %08lX\n thread %c%c%c%c\n", (int)code, name,
               (unsigned long)last.watchdog.stuck_task_pc,
               (unsigned long)last.watchdog.stuck_task_lr, (char)(packed & 0xFF),
               (char)((packed >> 8) & 0xFF), (char)((packed >> 16) & 0xFF),
               (char)((packed >> 24) & 0xFF));
  } else if (code == RebootReasonCode_Assert && last.data16 == 0xC1) {
    // The Zephyr port waited with the interrupt lock held: who waited.
    prv_append(data, "Last reboot: %d %s (locked)\n wait by %08lX\n", (int)code, name,
               (unsigned long)last.watchdog.stuck_task_pc);
  } else if (code == RebootReasonCode_Assert && last.data16 == 0xC2) {
    // A waiter left in a Zephyr port wait list: the list, the waiter, the waker.
    prv_append(data, "Last reboot: %d %s (stale)\n list %08lX w %08lX\n by %08lX\n", (int)code,
               name, (unsigned long)last.watchdog.stuck_task_pc,
               (unsigned long)last.watchdog.stuck_task_lr,
               (unsigned long)last.watchdog.stuck_task_callback);
  } else if (code == RebootReasonCode_Assert && (last.data16 & 0xFF00) == 0xB000) {
    // A thread woken while not blocked: characters 5 to 8 of its name, its state, the exception,
    // and the waits made with the interrupt lock held (how many, the caller of the last).
    const uint32_t w = last.watchdog.stuck_task_lr, c = last.watchdog.stuck_task_callback;
    char woken[5];
    for (int i = 0; i < 4; i++) {
      woken[i] = (char)((w >> (8 * i)) & 0xFF);
    }
    woken[4] = '\0';
    prv_append(data,
               "Last reboot: %d %s (wake2)\n woke ....%s st %u exc %lu\n lockwait %lu %08lX\n",
               (int)code, name, woken, last.data16 & 0xFF, (unsigned long)(c & 0xFF),
               (unsigned long)(c >> 16), (unsigned long)last.watchdog.stuck_task_pc);
  } else if (code >= RebootReasonCode_Watchdog) {
    prv_append(data, "Last reboot: %d %s\n LR %08lX\n", (int)code, name,
               (unsigned long)last.extra.value);
  } else {
    prv_append(data, "Last reboot: %d %s\n", (int)code, name);
  }
#if defined(CONFIG_WATCHDOG) && defined(CONFIG_SOC_SF32LB52)
  // What the hardware says reset it, which a reset that left no reason still shows.
  const uint32_t wsr = watchdog_get_reset_wsr();
  prv_append(data, " wsr %03lX%s%s%s%s%s%s\n", (unsigned long)wsr,
             (wsr & PBL_BIT(1)) ? " WDT1" : "", (wsr & PBL_BIT(2)) ? " WDT2" : "",
             (wsr & PBL_BIT(5)) ? " IWDT" : "", (wsr & PBL_BIT(7)) ? " LOWBAT" : "",
             (wsr & PBL_BIT(6)) ? " PWRKEY" : "", (wsr & (PBL_BIT(3) | PBL_BIT(4))) ? " PIN" : "");
#endif
}

// The summary (Zephyr, router, heap) fits the first screen; the HCI reads follow.
static void prv_render(ProbeData *data, bool with_reads) {
  data->text_len = 0;
  data->text[0] = '\0';
  prv_append_last_reboot(data);
  prv_append_zephyr(data);
  prv_append_router(data);
  prv_append_power(data);
  prv_append_heap(data);
  if (with_reads) {
    prv_run_reads(data);
  } else {
    prv_append(data, "(HCI reads: menu > Refresh)\n");
  }
  prv_append(data, "\nSelect: %s\nHold Select: menu\n",
             data->scan_enabled_by_us ? "stop discoverable" : "make discoverable");
  prv_refresh_text(data);
  scroll_layer_set_content_offset(&data->scroll_layer, GPointZero, false);
}

static void prv_render_main(ProbeData *data) {
  prv_render(data, true);
}

#ifdef CONFIG_BT_ZEPHYR_CLASSIC
static void prv_check_pairing(ProbeData *data);

// Redraws the summary while the Zephyr host works, for up to 60 s.
static void prv_refresh_timer_cb(void *context) {
  ProbeData *data = context;
  data->refresh_timer = NULL;
  // Keep the place the user scrolled to.
  const GPoint offset = scroll_layer_get_content_offset(&data->scroll_layer);
  prv_render(data, false);
  scroll_layer_set_content_offset(&data->scroll_layer, offset, false);
  prv_check_pairing(data);
  if (++data->refresh_ticks < 120) {
    data->refresh_timer = app_timer_register(500, prv_refresh_timer_cb, data);
  }
}

static void prv_start_refresh(ProbeData *data) {
  data->refresh_ticks = 0;
  if (data->refresh_timer == NULL) {
    data->refresh_timer = app_timer_register(500, prv_refresh_timer_cb, data);
  }
}
#endif

#ifdef CONFIG_BT_ZEPHYR_CLASSIC
/* Heap blocks: the largest free blocks of the kernel heap, large enough to read in a photo. */

static void prv_heap_window_load(Window *window) {
  ProbeData *data = window_get_user_data(window);
  Heap *heap = kernel_heap_get();
  unsigned int used, free_bytes, max_free;
  heap_calc_totals(heap, &used, &free_bytes, &max_free);
  unsigned int blocks[5] = {0};
  const size_t n = heap_get_largest_free_blocks(heap, blocks, ARRAY_LENGTH(blocks));

  size_t len = snprintf(data->heap_buf, sizeof(data->heap_buf), "size %u\nfree %u\n",
                        (unsigned int)heap_size(heap), free_bytes);
  for (size_t i = 0; i < n && len < sizeof(data->heap_buf); i++) {
    len += snprintf(&data->heap_buf[len], sizeof(data->heap_buf) - len, "%u: %u\n",
                    (unsigned int)(i + 1), blocks[i]);
  }

  TextLayer *text = &data->heap_text;
  text_layer_init(text, &window->layer.bounds);
  text_layer_set_font(text, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text(text, data->heap_buf);
  layer_add_child(&window->layer, &text->layer);
}

static void prv_push_heap_window(ProbeData *data) {
  Window *window = &data->heap_window;
  window_init(window, WINDOW_NAME("BT Classic Probe Heap"));
  window_set_user_data(window, data);
  window_set_window_handlers(window, &(WindowHandlers){
                                       .load = prv_heap_window_load,
                                     });
  app_window_stack_push(window, true);
}

/* Pairing: the passkey to compare with the phone's, SELECT to confirm, BACK to reject. */

static void prv_pair_reply(ClickRecognizerRef recognizer, void *context) {
  const bool accept = click_recognizer_get_button_id(recognizer) == BUTTON_ID_SELECT;
  (void)zbt_pairing_reply(accept);
  app_window_stack_pop(true);
}

static void prv_pair_click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_pair_reply);
  window_single_click_subscribe(BUTTON_ID_BACK, prv_pair_reply);
}

static void prv_pair_window_load(Window *window) {
  ProbeData *data = window_get_user_data(window);
  TextLayer *text = &data->pair_text;
  text_layer_init(text, &window->layer.bounds);
  text_layer_set_font(text, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_alignment(text, GTextAlignmentCenter);
  text_layer_set_text(text, data->pair_buf);
  layer_add_child(&window->layer, &text->layer);
}

static void prv_check_pairing(ProbeData *data) {
  struct zbt_status st;
  zbt_get_status(&st);
  const bool asking = st.pairing == ZBT_PAIRING_CONFIRM || st.pairing == ZBT_PAIRING_CONSENT;
  if (!asking) {
    data->pair_shown = false;
    return;
  }
  if (data->pair_shown) {
    return;
  }
  data->pair_shown = true;
  if (st.pairing == ZBT_PAIRING_CONFIRM) {
    snprintf(data->pair_buf, sizeof(data->pair_buf),
             "Same on phone?\n%06lu\nSELECT: match\nBACK: no", (unsigned long)st.passkey);
  } else {
    snprintf(data->pair_buf, sizeof(data->pair_buf), "Pair (no code)?\nSELECT: yes\nBACK: no");
  }
  Window *window = &data->pair_window;
  window_init(window, WINDOW_NAME("BT Classic Probe Pairing"));
  window_set_user_data(window, data);
  window_set_window_handlers(window, &(WindowHandlers){
                                       .load = prv_pair_window_load,
                                     });
  window_set_click_config_provider_with_context(window, prv_pair_click_config, data);
  app_window_stack_push(window, true);
}
#endif

#ifdef CONFIG_BT_ZEPHYR_CLASSIC
/* Call view: hands-free state and, once the phone opens the voice link, what the LCPU's voice
 * path does. Redrawn every second while shown. */

static void prv_call_append(ProbeData *data, size_t *len, const char *fmt, ...) {
  if (*len >= sizeof(data->call_buf) - 1) {
    return;
  }
  va_list args;
  va_start(args, fmt);
  const int n = vsnprintf(&data->call_buf[*len], sizeof(data->call_buf) - *len, fmt, args);
  va_end(args);
  if (n > 0) {
    *len = MIN(*len + (size_t)n, sizeof(data->call_buf) - 1);
  }
}

static void prv_call_append_ring(ProbeData *data, size_t *len, const char *name,
                                 const struct lcpu_audio_ring *r) {
  prv_call_append(data, len, "%s w %u/%u r %u/%u n %d\n", name, r->write_mirror, r->write_index,
                  r->read_mirror, r->read_index, r->buffer_size);
}

static void prv_call_render(ProbeData *data) {
  struct zbt_status st;
  zbt_get_status(&st);
  struct lcpu_audio_probe lp;
  lcpu_audio_probe_read(&lp);
  struct hci_router_stats rs;
  hci_router_get_stats(&rs);
  uint8_t sc[HCI_ROUTER_SYNC_CONN_LEN];
  const bool sc_valid = hci_router_get_sync_conn(sc);

  size_t len = 0;
  data->call_buf[0] = '\0';
  prv_call_append(data, &len, "SLC %s err %d ch %u\n", s_zbt_hfp_names[st.hfp], st.hfp_err,
                  st.hfp_channel);
  prv_call_append(data, &len, "svc %d sig %d roam %d bat %d\n", st.ind_service, st.ind_signal,
                  st.ind_roam, st.ind_battery);
  prv_call_append(data, &len, "call %s ring %u err %d\n", s_zbt_call_names[st.call], st.rings,
                  st.call_err);
  prv_call_append(data, &len, "CLIP %s\n", st.clip[0] ? st.clip : "-");
  prv_call_append(data, &len, "last CLIP %s\n", st.last_clip[0] ? st.last_clip : "-");
  // Its bytes too: the probe's font may not show them (a name in Japanese, emoji, spaces).
  const size_t clip_len = strnlen(st.last_clip, sizeof(st.last_clip));
  prv_call_append(data, &len, " len %u", (unsigned)clip_len);
  for (size_t i = 0; i < clip_len && i < 24; i++) {
    prv_call_append(data, &len, "%s%02x", (i % 8) == 0 ? "\n " : "", (uint8_t)st.last_clip[i]);
  }
  prv_call_append(data, &len, "\n");
  // The last call events, oldest first (zbt_trace()).
  prv_call_append(data, &len, "trace\n");
  if (len < sizeof(data->call_buf) - 1) {
    len += zbt_trace_format(&data->call_buf[len], sizeof(data->call_buf) - len);
    prv_call_append(data, &len, "\n");
  }
  prv_call_append(data, &len, "SCO %s n %u rsn %02x\n", st.sco ? "UP" : "down", st.sco_count,
                  st.sco_reason);
  if (sc_valid) {
    // Status, link type, transmission interval and window, rx/tx packet lengths, air mode.
    prv_call_append(data, &len, "sync st %02x lt %u air %u\n int %u win %u rx %u tx %u\n", sc[0],
                    sc[9], sc[16], sc[10], sc[11], sc[12] | (sc[13] << 8), sc[14] | (sc[15] << 8));
  } else {
    prv_call_append(data, &len, "sync: none\n");
  }
  prv_call_append(data, &len, "router sco %lu ccx %lu\nqid6 %lu rev %u\n",
                  (unsigned long)rs.drop_sco, (unsigned long)rs.cc_unmatched,
                  (unsigned long)lp.notifications, lp.rev_id);
  // The LCPU's soft CVSD word, the gateway's features (bit 3: in-band ring) and last +BSIR.
  prv_call_append(data, &len, "scv %08lx ag %03lx bsir %d\n", (unsigned long)lp.soft_cvsd,
                  (unsigned long)st.ag_features, st.inband_ring);
#ifdef CONFIG_BT_CALL_AUDIO
  // The call's audio: from the phone to the speaker, from the microphone to the phone.
  struct call_audio_stats ca;
  call_audio_get_stats(&ca);
  const int mic = call_audio_mic_gain_step(0);
  char mic_str[24];
  if (mic > 0) {
    snprintf(mic_str, sizeof(mic_str), "x%d", mic);
  } else {
    // The automatic gain, now.
    snprintf(mic_str, sizeof(mic_str), "agc %ddB", (mic - CALL_AUDIO_MIC_AGC) / 10);
  }
  prv_call_append(data, &len, "aud %s spk %d mic %d n %u\nvol %ux%u mic %s\n",
                  ca.running ? "ON" : "off", ca.speaker_on, ca.mic_on, ca.starts,
                  call_audio_volume_step(0), call_audio_rx_gain(), mic_str);
  // The battery's current now and averaged over the call audio's run, once a second.
  BatteryConstants bat;
  if (battery_get_constants(&bat) == 0) {
    if (ca.starts != data->bat_start) {
      data->bat_start = ca.starts;
      data->bat_ua_sum = 0;
      data->bat_reads = 0;
    }
    if (ca.running) {
      data->bat_ua_sum += bat.i_ua;
      data->bat_reads++;
    }
    const int32_t avg = data->bat_reads ? (int32_t)(data->bat_ua_sum / data->bat_reads) : 0;
    prv_call_append(data, &len, "bat %ldmA avg %ldmA n %lu %ldmV\n", (long)(bat.i_ua / 1000),
                    (long)(avg / 1000), (unsigned long)data->bat_reads, (long)bat.v_mv);
  }
  prv_call_append(data, &len, "rx %lu bad %lu hdr %02x%02x%02x%02x\n", (unsigned long)ca.rx_packets,
                  (unsigned long)ca.rx_bad, ca.rx_hdr[0], ca.rx_hdr[1], ca.rx_hdr[2], ca.rx_hdr[3]);
  prv_call_append(data, &len, "rx rms %u dc %d gap %u\n", ca.rx_rms, ca.rx_mean, ca.rx_max_gap_ms);
  // Steps at the packets' edges and inside them; longest CVSD decode and encode.
  prv_call_append(data, &len, "edge %u in %u cvsd %u/%uus\n", ca.rx_edge_step, ca.rx_inner_step,
                  ca.rx_cvsd_us, ca.tx_cvsd_us);
  prv_call_append(data, &len, "drop %lu fill %lu short %lu\n", (unsigned long)ca.rx_drops,
                  (unsigned long)ca.rx_fills, (unsigned long)ca.rx_short);
  prv_call_append(data, &len, "tx %lu ovf %lu mic %lu\n", (unsigned long)ca.tx_packets,
                  (unsigned long)ca.tx_overflows, (unsigned long)ca.mic_frames);
  prv_call_append(data, &len, "tx rms %u dc %d stk %u/%u\n", ca.tx_rms, ca.tx_mean, ca.stack_used,
                  ca.stack_size);
#endif
  if (!lp.valid) {
    prv_call_append(data, &len, "LCPU: not readable\n");
    return;
  }
  prv_call_append(data, &len, "var %08lx %08lx\n    %08lx %08lx\n", (unsigned long)lp.vars[0],
                  (unsigned long)lp.vars[1], (unsigned long)lp.vars[2], (unsigned long)lp.vars[3]);
  prv_call_append_ring(data, &len, ">H", &lp.to_hcpu);
  prv_call_append_ring(data, &len, ">L", &lp.to_lcpu);
  prv_call_append(data, &len, "sta %u %u %u %u %u %u\n", lp.sco_sta[0], lp.sco_sta[1],
                  lp.sco_sta[2], lp.sco_sta[3], lp.sco_sta[4], lp.sco_sta[5]);
  prv_call_append(data, &len, "para @%08lx", (unsigned long)lp.sco_para_addr);
  for (size_t i = 0; i < sizeof(lp.sco_para); i++) {
    prv_call_append(data, &len, "%s%02x", (i % 10) == 0 ? "\n " : "", lp.sco_para[i]);
  }
  prv_call_append(data, &len, "\n");
}

static void prv_call_refresh(ProbeData *data) {
  prv_call_render(data);
  const GRect *bounds = &data->call_window.layer.bounds;
  TextLayer *text = &data->call_text;
  text_layer_set_size(text, GSize(bounds->size.w, 2000));
  text_layer_set_text(text, data->call_buf);
  GSize size = text_layer_get_content_size(app_state_get_graphics_context(), text);
  size.h += 8;
  text_layer_set_size(text, GSize(bounds->size.w, size.h));
  scroll_layer_set_content_size(&data->call_scroll, GSize(bounds->size.w, size.h));
}

static void prv_call_timer_cb(void *context) {
  ProbeData *data = context;
  prv_call_refresh(data);
  data->call_timer = app_timer_register(1000, prv_call_timer_cb, data);
}

#ifdef CONFIG_BT_CALL_AUDIO
// Select: speaker volume up (around from the top); held: the microphone's gain to the next.
static void prv_call_volume_up(ClickRecognizerRef recognizer, void *context) {
  (void)call_audio_volume_step(1);
  prv_call_refresh(context);
}

static void prv_call_mic_gain(ClickRecognizerRef recognizer, void *context) {
  (void)call_audio_mic_gain_step(1);
  prv_call_refresh(context);
}

static void prv_call_click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_call_volume_up);
  window_long_click_subscribe(BUTTON_ID_SELECT, 0, prv_call_mic_gain, NULL);
}
#endif

static void prv_call_window_load(Window *window) {
  ProbeData *data = window_get_user_data(window);
  const GRect *bounds = &window->layer.bounds;
  ScrollLayer *scroll = &data->call_scroll;
  scroll_layer_init(scroll, bounds);
  scroll_layer_set_click_config_onto_window(scroll, window);
#ifdef CONFIG_BT_CALL_AUDIO
  scroll_layer_set_callbacks(scroll, (ScrollLayerCallbacks){
                                       .click_config_provider = prv_call_click_config,
                                     });
  scroll_layer_set_context(scroll, data);
#endif
  TextLayer *text = &data->call_text;
  text_layer_init(text, &GRect(0, 0, bounds->size.w, 2000));
  text_layer_set_font(text, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
  scroll_layer_add_child(scroll, &text->layer);
  layer_add_child(&window->layer, &scroll->layer);
  prv_call_refresh(data);
  data->call_timer = app_timer_register(1000, prv_call_timer_cb, data);
}

static void prv_call_window_unload(Window *window) {
  ProbeData *data = window_get_user_data(window);
  if (data->call_timer != NULL) {
    app_timer_cancel(data->call_timer);
    data->call_timer = NULL;
  }
}

static void prv_push_call_window(ProbeData *data) {
  Window *window = &data->call_window;
  window_init(window, WINDOW_NAME("BT Classic Probe Call"));
  window_set_user_data(window, data);
  window_set_window_handlers(window, &(WindowHandlers){
                                       .load = prv_call_window_load,
                                       .unload = prv_call_window_unload,
                                     });
  app_window_stack_push(window, true);
}

#endif

#ifdef CONFIG_BT_HCI_ROUTER
/* HCI log: the router's last classic exchanges, newest first, large enough to read in a photo.
 * "07>0411 P 8100" is a command (P pass, M merge, F fake, R reject) and its first parameters;
 * "08<AuthCmp 05 0081" an event with its first parameter (the status) and the next two (a
 * handle). */

static const char *prv_evt_name(uint16_t code) {
  switch (code) {
    case 0x03:
      return "ConnCmp";
    case 0x04:
      return "ConnReq";
    case 0x05:
      return "DiscCmp";
    case 0x06:
      return "AuthCmp";
    case 0x07:
      return "NameCmp";
    case 0x08:
      return "EncChg";
    case 0x0B:
      return "RemFeat";
    case 0x12:
      return "RoleChg";
    case 0x13:
      return "NOCP";
    case 0x17:
      return "LKReq";
    case 0x18:
      return "LKNotif";
    case 0x1B:
      return "MaxSlot";
    case 0x23:
      return "ExtFeat";
    case 0x31:
      return "IOReq";
    case 0x32:
      return "IORsp";
    case 0x33:
      return "UCReq";
    case 0x36:
      return "SPCmp";
    case 0x3D:
      return "HostFeat";
    default:
      return NULL;
  }
}

static size_t prv_log_line(char *buf, size_t size, const struct hci_router_log_entry *e) {
  static const char s_actions[] = "PMFR";
  const uint8_t *p = e->params;
  const unsigned int seq = e->seq % 100;
  if (e->kind == HCI_ROUTER_LOG_CMD) {
    return snprintf(buf, size, "%02u>%04x %c %02x%02x\n", seq, e->code, s_actions[e->action & 3],
                    p[0], p[1]);
  }
  if (e->code == 0x0E) { // Command Complete: opcode, status
    return snprintf(buf, size, "%02u<CC %04x st %02x\n", seq, p[1] | (p[2] << 8), p[3]);
  }
  if (e->code == 0x0F) { // Command Status: status, opcode
    return snprintf(buf, size, "%02u<CS %04x st %02x\n", seq, p[2] | (p[3] << 8), p[0]);
  }
  if (e->code == 0x05) { // Disconnection Complete: status, handle, reason
    return snprintf(buf, size, "%02u<DiscCmp %02x %02x%02x r%02x\n", seq, p[0], p[2], p[1], p[3]);
  }
  const char *name = prv_evt_name(e->code);
  if (name != NULL) {
    return snprintf(buf, size, "%02u<%s %02x %02x%02x\n", seq, name, p[0], p[2], p[1]);
  }
  return snprintf(buf, size, "%02u<E%02x %02x %02x%02x\n", seq, e->code, p[0], p[2], p[1]);
}

static void prv_log_window_load(Window *window) {
  ProbeData *data = window_get_user_data(window);
  struct hci_router_log_entry *log = app_malloc_check(sizeof(*log) * HCI_ROUTER_LOG_LEN);
  const size_t n = hci_router_get_log(log);
  size_t len = snprintf(data->log_buf, sizeof(data->log_buf), "HCI log %u\n", (unsigned int)n);
  for (size_t i = n; i > 0 && len < sizeof(data->log_buf); i--) {
    len += prv_log_line(&data->log_buf[len], sizeof(data->log_buf) - len, &log[i - 1]);
  }
  app_free(log);

  const GRect *bounds = &window->layer.bounds;
  ScrollLayer *scroll = &data->log_scroll;
  scroll_layer_init(scroll, bounds);
  scroll_layer_set_click_config_onto_window(scroll, window);
  TextLayer *text = &data->log_text;
  text_layer_init(text, &GRect(0, 0, bounds->size.w, 2000));
  text_layer_set_font(text, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text(text, data->log_buf);
  const GSize size = text_layer_get_content_size(app_state_get_graphics_context(), text);
  text_layer_set_size(text, GSize(bounds->size.w, size.h + 8));
  scroll_layer_set_content_size(scroll, GSize(bounds->size.w, size.h + 8));
  scroll_layer_add_child(scroll, &text->layer);
  layer_add_child(&window->layer, &scroll->layer);
}

static void prv_push_log_window(ProbeData *data) {
  Window *window = &data->log_window;
  window_init(window, WINDOW_NAME("BT Classic Probe HCI Log"));
  window_set_user_data(window, data);
  window_set_window_handlers(window, &(WindowHandlers){
                                       .load = prv_log_window_load,
                                     });
  app_window_stack_push(window, true);
}
#endif

enum {
  MENU_REFRESH,
#ifdef CONFIG_BT_ZEPHYR_CLASSIC
  MENU_ZEPHYR_START,
  MENU_ZEPHYR_STOP,
  MENU_CONNECT_PHONE,
  MENU_HEAP_BLOCKS,
  MENU_CLEAR_KEYS,
  MENU_ZEPHYR_DISCOVERABLE,
  MENU_HFP_ANSWER,
  MENU_HFP_HANGUP,
  MENU_CALL_VIEW,
#endif
#ifdef CONFIG_BT_HCI_ROUTER
  MENU_HCI_LOG,
  MENU_ROUTER_NEXT_BOOT,
  MENU_REBOOT,
#endif
  MENU_COUNT,
};

static void prv_menu_select(int index, void *context) {
  ProbeData *data = context;

  switch (index) {
    case MENU_REFRESH:
      app_window_stack_pop(true);
      prv_render_main(data);
      break;
#ifdef CONFIG_BT_ZEPHYR_CLASSIC
    case MENU_ZEPHYR_START:
      (void)zbt_start();
      app_window_stack_pop(true);
      prv_render(data, false);
      prv_start_refresh(data);
      break;
    case MENU_ZEPHYR_STOP:
      (void)zbt_stop();
      app_window_stack_pop(true);
      prv_render(data, false);
      prv_start_refresh(data);
      break;
    case MENU_CONNECT_PHONE:
      prv_read_phone(data);
      if (!data->phone_valid) {
        data->menu_items[MENU_CONNECT_PHONE].subtitle = "Phone not found";
      } else if (data->phone_random) {
        // Not the phone's BR/EDR address: do not try.
        data->menu_items[MENU_CONNECT_PHONE].subtitle = "Random address, not tried";
      } else if (zbt_connect(data->phone_addr) != 0) {
        data->menu_items[MENU_CONNECT_PHONE].subtitle = "Zephyr not ready";
      } else {
        app_window_stack_pop(true);
        prv_render(data, false);
        prv_start_refresh(data);
        break;
      }
      layer_mark_dirty(simple_menu_layer_get_layer(&data->menu_layer));
      break;
    case MENU_HEAP_BLOCKS:
      prv_push_heap_window(data);
      break;
    case MENU_CLEAR_KEYS:
      data->menu_items[MENU_CLEAR_KEYS].subtitle = zbt_clear_keys() == 0 ? "Cleared" : "Failed";
      layer_mark_dirty(simple_menu_layer_get_layer(&data->menu_layer));
      break;
    case MENU_ZEPHYR_DISCOVERABLE: {
      struct zbt_status st;
      zbt_get_status(&st);
      if (st.discoverable) {
        (void)zbt_clear_discoverable();
      } else {
        (void)zbt_set_discoverable(NULL, 0x200704);
      }
      app_window_stack_pop(true);
      prv_render(data, false);
      prv_start_refresh(data);
      break;
    }
    case MENU_HFP_ANSWER:
      data->menu_items[MENU_HFP_ANSWER].subtitle = zbt_hfp_answer() == 0 ? "Sent" : "Not ready";
      layer_mark_dirty(simple_menu_layer_get_layer(&data->menu_layer));
      break;
    case MENU_HFP_HANGUP:
      data->menu_items[MENU_HFP_HANGUP].subtitle = zbt_hfp_hangup() == 0 ? "Sent" : "Not ready";
      layer_mark_dirty(simple_menu_layer_get_layer(&data->menu_layer));
      break;
    case MENU_CALL_VIEW:
      prv_push_call_window(data);
      break;
#endif
#ifdef CONFIG_BT_HCI_ROUTER
    case MENU_HCI_LOG:
      prv_push_log_window(data);
      break;
    case MENU_ROUTER_NEXT_BOOT:
      if (hci_router_boot_reserve() == 0) {
        system_reset();
      }
      data->menu_items[MENU_ROUTER_NEXT_BOOT].subtitle = "Reserve failed";
      layer_mark_dirty(simple_menu_layer_get_layer(&data->menu_layer));
      break;
    case MENU_REBOOT:
      system_reset();
      break;
#endif
    default:
      break;
  }
}

static void prv_menu_window_load(Window *window) {
  ProbeData *data = window_get_user_data(window);

  data->menu_items[MENU_REFRESH] = (SimpleMenuItem){
    .title = "Refresh",
    .subtitle = "Heap, router, reads",
    .callback = prv_menu_select,
  };
#ifdef CONFIG_BT_ZEPHYR_CLASSIC
  data->menu_items[MENU_ZEPHYR_START] = (SimpleMenuItem){
    .title = "Zephyr start",
    .subtitle = "Borrow worker RAM",
    .callback = prv_menu_select,
  };
  data->menu_items[MENU_ZEPHYR_STOP] = (SimpleMenuItem){
    .title = "Zephyr stop",
    .subtitle = "Return worker RAM",
    .callback = prv_menu_select,
  };
  data->menu_items[MENU_CONNECT_PHONE] = (SimpleMenuItem){
    .title = "Connect phone",
    .subtitle = "BR/EDR, pair if needed",
    .callback = prv_menu_select,
  };
  data->menu_items[MENU_HEAP_BLOCKS] = (SimpleMenuItem){
    .title = "Heap blocks",
    .subtitle = "Largest free, big digits",
    .callback = prv_menu_select,
  };
  data->menu_items[MENU_CLEAR_KEYS] = (SimpleMenuItem){
    .title = "Clear classic keys",
    .subtitle = "Forget the phone",
    .callback = prv_menu_select,
  };
  data->menu_items[MENU_ZEPHYR_DISCOVERABLE] = (SimpleMenuItem){
    .title = "Zephyr discoverable",
    .subtitle = "BR/EDR on/off",
    .callback = prv_menu_select,
  };
  data->menu_items[MENU_HFP_ANSWER] = (SimpleMenuItem){
    .title = "Answer (HFP)",
    .subtitle = "ATA",
    .callback = prv_menu_select,
  };
  data->menu_items[MENU_HFP_HANGUP] = (SimpleMenuItem){
    .title = "Hang up (HFP)",
    .subtitle = "AT+CHUP",
    .callback = prv_menu_select,
  };
  data->menu_items[MENU_CALL_VIEW] = (SimpleMenuItem){
    .title = "Call view",
    .subtitle = "HFP, SCO, LCPU audio",
    .callback = prv_menu_select,
  };
#endif
#ifdef CONFIG_BT_HCI_ROUTER
  data->menu_items[MENU_HCI_LOG] = (SimpleMenuItem){
    .title = "HCI log",
    .subtitle = "Classic, newest first",
    .callback = prv_menu_select,
  };
  data->menu_items[MENU_ROUTER_NEXT_BOOT] = (SimpleMenuItem){
    .title = "Router next boot",
    .subtitle = "Reserve once, reboot",
    .callback = prv_menu_select,
  };
  data->menu_items[MENU_REBOOT] = (SimpleMenuItem){
    .title = "Reboot",
    .subtitle = "No reservation",
    .callback = prv_menu_select,
  };
#endif
  data->menu_section = (SimpleMenuSection){
    .items = data->menu_items,
    .num_items = MENU_COUNT,
  };
  simple_menu_layer_init(&data->menu_layer, &window->layer.bounds, window, &data->menu_section, 1,
                         data);
  layer_add_child(&window->layer, simple_menu_layer_get_layer(&data->menu_layer));
}

static void prv_menu_window_unload(Window *window) {
  ProbeData *data = window_get_user_data(window);
  simple_menu_layer_deinit(&data->menu_layer);
}

static void prv_select_long_click(ClickRecognizerRef recognizer, void *context) {
  ProbeData *data = context;
  Window *window = &data->menu_window;

  window_init(window, WINDOW_NAME("BT Classic Probe Menu"));
  window_set_user_data(window, data);
  window_set_window_handlers(window, &(WindowHandlers){
                                       .load = prv_menu_window_load,
                                       .unload = prv_menu_window_unload,
                                     });
  app_window_stack_push(window, true);
}

static void prv_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click);
  window_long_click_subscribe(BUTTON_ID_SELECT, 0, prv_select_long_click, NULL);
}

static void prv_window_load(Window *window) {
  ProbeData *data = window_get_user_data(window);
  const GRect *bounds = &window->layer.bounds;

  ScrollLayer *scroll_layer = &data->scroll_layer;
  scroll_layer_init(scroll_layer, bounds);
  scroll_layer_set_click_config_onto_window(scroll_layer, window);
  scroll_layer_set_callbacks(scroll_layer, (ScrollLayerCallbacks){
                                             .click_config_provider = prv_click_config_provider,
                                           });
  scroll_layer_set_context(scroll_layer, data);

  TextLayer *text = &data->text_layer;
  text_layer_init(text, &GRect(0, 0, bounds->size.w, 2000));
  text_layer_set_font(text, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  scroll_layer_add_child(scroll_layer, &text->layer);
  layer_add_child(&window->layer, &scroll_layer->layer);

  prv_render_main(data);
}

static void prv_init(void) {
  ProbeData *data = app_zalloc_check(sizeof(ProbeData));
  app_state_set_user_data(data);
  data->last_scan_enable = -1;
#ifdef CONFIG_BT_ZEPHYR_CLASSIC
  prv_read_phone(data);
#endif

  Window *window = &data->window;
  window_init(window, WINDOW_NAME("BT Classic Probe"));
  window_set_user_data(window, data);
  window_set_window_handlers(window, &(WindowHandlers){
                                       .load = prv_window_load,
                                     });
  app_window_stack_push(window, true);
}

static void prv_deinit(void) {
  ProbeData *data = app_state_get_user_data();
#ifdef CONFIG_BT_ZEPHYR_CLASSIC
  if (data->refresh_timer != NULL) {
    app_timer_cancel(data->refresh_timer);
  }
  struct zbt_status st;
  zbt_get_status(&st);
  if (st.discoverable) {
    (void)zbt_clear_discoverable();
  }
#endif
  if (data->scan_enabled_by_us || data->last_scan_enable > 0) {
    int rc = prv_write_scan(data, SCAN_OFF);
    PBL_LOG_DBG("BR/EDR scan off on exit: %d", rc);
  }
  app_free(data);
}

static void prv_main(void) {
  // Menus and pages by touch too, whatever the touch navigation setting.
  app_touch_navigation_enable(true);
  prv_init();
  app_event_loop();
  prv_deinit();
}

const PebbleProcessMd *bt_classic_probe_get_app_info(void) {
  static const PebbleProcessMdSystem s_app_info = {
    .common.main_func = prv_main,
    .name = "BT Classic Probe",
    .icon_resource_id = RESOURCE_ID_BT_CLASSIC_PROBE_MENU_ICON,
  };
  return (const PebbleProcessMd *)&s_app_info;
}
