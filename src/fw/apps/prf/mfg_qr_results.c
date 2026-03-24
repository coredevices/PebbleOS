/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/app.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/qr_code.h"
#include "applib/ui/window.h"
#include "apps/prf/mfg_test_result.h"
#include "bluetooth/bluetooth_types.h"
#include "kernel/pbl_malloc.h"
#include "process_state/app_state/app_state.h"
#include "process_management/pebble_process_md.h"
#include "mfg/mfg_serials.h"
#include "services/common/bluetooth/local_id.h"
#include "system/version.h"

#include <stdio.h>
#include <string.h>

typedef struct {
  Window window;
  QRCode qr_code;
  char qr_buffer[256];
} AppData;

static void prv_append_result(char *buf, size_t bufsz, MfgTestId test) {
  const MfgTestResult *r = mfg_test_result_get(test);
  if (!r) {
    return;
  }

  size_t pos = strlen(buf);
  if (pos > 0 && pos < bufsz - 1) {
    buf[pos++] = ';';
    buf[pos] = '\0';
  }

  char entry[16];
  char rc;
  if (r->ran) {
    rc = r->passed ? 'P' : 'F';
  } else {
    rc = 'N';
  }

  switch (test) {
  case MfgTestId_Buttons:
    snprintf(entry, sizeof(entry), "BTN:%c,%X", rc, (unsigned)(r->value & 0xF));
    break;
  case MfgTestId_Display:
    snprintf(entry, sizeof(entry), "DSP:%c", rc);
    break;
#ifdef CONFIG_TOUCH
  case MfgTestId_Touch:
    snprintf(entry, sizeof(entry), "TCH:%c", rc);
    break;
#endif
  case MfgTestId_Backlight:
    snprintf(entry, sizeof(entry), "BKL:%c", rc);
    break;
  case MfgTestId_Accel:
    snprintf(entry, sizeof(entry), "ACC:%c", rc);
    break;
#ifdef CONFIG_MAG
  case MfgTestId_Mag:
    snprintf(entry, sizeof(entry), "MAG:%c", rc);
    break;
#endif
#if PLATFORM_ASTERIX || PLATFORM_OBELIX
  case MfgTestId_Speaker:
    snprintf(entry, sizeof(entry), "SPK:%c", rc);
    break;
#endif
#if PLATFORM_ASTERIX || PLATFORM_OBELIX || PLATFORM_GETAFIX
  case MfgTestId_Mic:
    snprintf(entry, sizeof(entry), "MIC:%c", rc);
    break;
#endif
  case MfgTestId_ALS:
    snprintf(entry, sizeof(entry), "ALS:%c,%lu", rc, (unsigned long)r->value);
    break;
  case MfgTestId_Vibration:
    snprintf(entry, sizeof(entry), "VIB:%c", rc);
    break;
#if PLATFORM_OBELIX && defined(MANUFACTURING_FW)
  case MfgTestId_HrmCtrLeakage:
    snprintf(entry, sizeof(entry), "HRM:%c", rc);
    break;
#endif
  case MfgTestId_ProgramColor:
    snprintf(entry, sizeof(entry), "CLR:%c", rc);
    break;
  case MfgTestId_Adv:
    snprintf(entry, sizeof(entry), "ADV:%c", rc);
    break;
  case MfgTestId_Charge:
    snprintf(entry, sizeof(entry), "CHG:%c", rc);
    break;
  case MfgTestId_Discharge:
    snprintf(entry, sizeof(entry), "DCH:%c,%lu", rc, (unsigned long)r->value);
    break;
  default:
    return;
  }

  strncat(buf, entry, bufsz - strlen(buf) - 1);
}

static void prv_handle_init(void) {
  AppData *data = app_malloc_check(sizeof(AppData));

  app_state_set_user_data(data);

  Window *window = &data->window;
  window_init(window, "");
  window_set_fullscreen(window, true);

  // Start with serial number, Bluetooth MAC address, and firmware version
  char mac[BT_DEVICE_ADDRESS_FMT_BUFFER_SIZE];
  bt_local_id_copy_address_mac_string(mac);
  snprintf(data->qr_buffer, sizeof(data->qr_buffer), "%s;%s;%s",
           mfg_get_serial_number(), mac, TINTIN_METADATA.version_tag);

  for (MfgTestId id = 0; id < MfgTestIdCount; id++) {
    prv_append_result(data->qr_buffer, sizeof(data->qr_buffer), id);
  }

  QRCode *qr_code = &data->qr_code;
#if PBL_ROUND
#define QR_CODE_SIZE ((window->layer.bounds.size.w * 10) / 14)
  qr_code_init_with_parameters(qr_code,
                               &GRect((window->layer.bounds.size.w - QR_CODE_SIZE) / 2,
                                      (window->layer.bounds.size.h - QR_CODE_SIZE) / 2,
                                      QR_CODE_SIZE, QR_CODE_SIZE),
                               data->qr_buffer, strlen(data->qr_buffer), QRCodeECCMedium,
                               GColorBlack, GColorWhite);
#else
  qr_code_init_with_parameters(qr_code,
                               &GRect(10, 10, window->layer.bounds.size.w - 20,
                                      window->layer.bounds.size.h - 20),
                               data->qr_buffer, strlen(data->qr_buffer), QRCodeECCMedium,
                               GColorBlack, GColorWhite);
#endif
  layer_add_child(&window->layer, &qr_code->layer);

  app_window_stack_push(window, true);
}

static void s_main(void) {
  prv_handle_init();

  app_event_loop();
}

const PebbleProcessMd* mfg_qr_results_app_get_info(void) {
  static const PebbleProcessMdSystem s_app_info = {
    .common.main_func = &s_main,
    // UUID: 7b3e9f2a-5d1c-4e8b-a6f0-3c9d8e7a1b5f
    .common.uuid = { 0x7b, 0x3e, 0x9f, 0x2a, 0x5d, 0x1c, 0x4e, 0x8b,
                     0xa6, 0xf0, 0x3c, 0x9d, 0x8e, 0x7a, 0x1b, 0x5f },
    .name = "MfgQRResults",
  };
  return (const PebbleProcessMd*) &s_app_info;
}
